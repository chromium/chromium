// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_handler.h"

#include <stdint.h>
#include <memory>
#include <unordered_set>
#include <utility>

#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/bad_clock_blocking_page.h"
#include "chrome/browser/ssl/captive_portal_blocking_page.h"
#include "chrome/browser/ssl/captive_portal_helper.h"
#include "chrome/browser/ssl/mitm_software_blocking_page.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "chrome/browser/ssl/ssl_error_assistant.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "components/ssl_errors/error_classification.h"
#include "components/ssl_errors/error_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
#endif

#if defined(OS_WIN)
#include "base/win/win_util.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#endif  // #if defined(OS_WIN)

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "chrome/browser/ssl/captive_portal_helper_android.h"
#endif

const base::Feature kMITMSoftwareInterstitial{"MITMSoftwareInterstitial",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCaptivePortalInterstitial{
    "CaptivePortalInterstitial", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCaptivePortalCertificateList{
    "CaptivePortalCertificateList", base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_WIN)
const base::Feature kSuperfishInterstitial{"SuperfishInterstitial",
                                           base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kSuperfishInterstitial{"SuperfishInterstitial",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif

namespace {

const base::Feature kSSLCommonNameMismatchHandling{
    "SSLCommonNameMismatchHandling", base::FEATURE_ENABLED_BY_DEFAULT};

// Default delay in milliseconds before displaying the SSL interstitial.
// This can be changed in tests.
// - If there is a name mismatch and a suggested URL available result arrives
//   during this time, the user is redirected to the suggester URL.
// - If a "captive portal detected" result arrives during this time,
//   a captive portal interstitial is displayed.
// - Otherwise, an SSL interstitial is displayed.
const int64_t kInterstitialDelayInMilliseconds = 3000;

const char kHistogram[] = "interstitial.ssl_error_handler";

bool IsSuperfish(const scoped_refptr<net::X509Certificate>& cert) {
  // This is the fingerprint of the well-known Superfish certificate at
  // https://pastebin.com/WcXv8QcG. Superfish is identified by certificate
  // fingerprint rather than SPKI because net::SSLInfo does not guarantee
  // |public_key_hashes| (the SPKIs) to be populated if the certificate doesn't
  // verify successfully. It so happens that Superfish uses the same certificate
  // universally (not just the same public key), and calculating the fingerprint
  // is more convenient here than calculating the SPKI.
  const net::SHA256HashValue kSuperfishFingerprint{
      {0xB6, 0xFE, 0x91, 0x51, 0x40, 0x2B, 0xAD, 0x1C, 0x06, 0xD7, 0xE6,
       0x6D, 0xB6, 0x7A, 0x26, 0xAA, 0x73, 0x56, 0xF2, 0xE6, 0xC6, 0x44,
       0xDB, 0xCF, 0x9F, 0x98, 0x96, 0x8F, 0xF6, 0x32, 0xE1, 0xB7}};
  for (const auto& intermediate : cert->intermediate_buffers()) {
    net::SHA256HashValue hash =
        net::X509Certificate::CalculateFingerprint256(intermediate.get());
    if (hash == kSuperfishFingerprint) {
      return true;
    }
  }
  return false;
}

// Adds a message to console after navigation commits and then, deletes itself.
// Also deletes itself if the navigation is stopped.
class CommonNameMismatchRedirectObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<CommonNameMismatchRedirectObserver> {
 public:
  ~CommonNameMismatchRedirectObserver() override {}

  static void AddToConsoleAfterNavigation(
      content::WebContents* web_contents,
      const std::string& request_url_hostname,
      const std::string& suggested_url_hostname) {
    web_contents->SetUserData(
        UserDataKey(),
        base::WrapUnique(new CommonNameMismatchRedirectObserver(
            web_contents, request_url_hostname, suggested_url_hostname)));
  }

 private:
  CommonNameMismatchRedirectObserver(content::WebContents* web_contents,
                                     const std::string& request_url_hostname,
                                     const std::string& suggested_url_hostname)
      : WebContentsObserver(web_contents),
        web_contents_(web_contents),
        request_url_hostname_(request_url_hostname),
        suggested_url_hostname_(suggested_url_hostname) {}

  // WebContentsObserver:
  void NavigationStopped() override {
    // Deletes |this|.
    web_contents_->RemoveUserData(UserDataKey());
  }

  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& /* load_details */) override {
    web_contents_->GetMainFrame()->AddMessageToConsole(
        content::CONSOLE_MESSAGE_LEVEL_INFO,
        base::StringPrintf(
            "Redirecting navigation %s -> %s because the server presented a "
            "certificate valid for %s but not for %s. To disable such "
            "redirects launch Chrome with the following flag: "
            "--disable-features=SSLCommonNameMismatchHandling",
            request_url_hostname_.c_str(), suggested_url_hostname_.c_str(),
            suggested_url_hostname_.c_str(), request_url_hostname_.c_str()));
    web_contents_->RemoveUserData(UserDataKey());
  }

  void WebContentsDestroyed() override {
    web_contents_->RemoveUserData(UserDataKey());
  }

  content::WebContents* web_contents_;
  const std::string request_url_hostname_;
  const std::string suggested_url_hostname_;

  DISALLOW_COPY_AND_ASSIGN(CommonNameMismatchRedirectObserver);
};

void RecordUMA(SSLErrorHandler::UMAEvent event) {
  UMA_HISTOGRAM_ENUMERATION(kHistogram, event,
                            SSLErrorHandler::SSL_ERROR_HANDLER_EVENT_COUNT);
}

bool IsCaptivePortalInterstitialEnabled() {
  return base::FeatureList::IsEnabled(kCaptivePortalInterstitial);
}

bool IsMITMSoftwareInterstitialEnabled() {
  return base::FeatureList::IsEnabled(kMITMSoftwareInterstitial);
}

bool IsSSLCommonNameMismatchHandlingEnabled() {
  return base::FeatureList::IsEnabled(kSSLCommonNameMismatchHandling);
}

// Configuration for SSLErrorHandler.
class ConfigSingleton {
 public:
  ConfigSingleton();

  base::TimeDelta interstitial_delay() const;
  SSLErrorHandler::TimerStartedCallback* timer_started_callback() const;
  base::Clock* clock() const;
  network_time::NetworkTimeTracker* network_time_tracker() const;

  bool IsKnownCaptivePortalCertificate(const net::SSLInfo& ssl_info);

  // Returns the name of a known MITM software provider that matches the
  // certificate passed in as the |cert| parameter. Returns empty string if
  // there is no match.
  const std::string MatchKnownMITMSoftware(
      const scoped_refptr<net::X509Certificate> cert);

  // Returns a DynamicInterstitialInfo that matches with |ssl_info|. If is no
  // match, return null.
  base::Optional<DynamicInterstitialInfo> MatchDynamicInterstitial(
      const net::SSLInfo& ssl_info,
      bool is_overridable);

  // Testing methods:
  void ResetForTesting();
  void SetInterstitialDelayForTesting(const base::TimeDelta& delay);
  void SetTimerStartedCallbackForTesting(
      SSLErrorHandler::TimerStartedCallback* callback);
  void SetClockForTesting(base::Clock* clock);
  void SetNetworkTimeTrackerForTesting(
      network_time::NetworkTimeTracker* tracker);
  void SetReportNetworkConnectivityCallbackForTesting(
      base::OnceClosure callback);

  void SetErrorAssistantProto(
      std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
          error_assistant_proto);

  void SetEnterpriseManagedForTesting(bool enterprise_managed);
  bool IsEnterpriseManagedFlagSetForTesting() const;
  int GetErrorAssistantProtoVersionIdForTesting() const;

  bool IsEnterpriseManaged() const;

  void SetOSReportsCaptivePortalForTesting(bool os_reports_captive_portal);
  bool DoesOSReportCaptivePortalForTesting() const;

  base::OnceClosure report_network_connectivity_callback() {
    return std::move(report_network_connectivity_callback_);
  }

 private:
  base::TimeDelta interstitial_delay_;

  // Callback to call when the interstitial timer is started. Used for
  // testing.
  SSLErrorHandler::TimerStartedCallback* timer_started_callback_ = nullptr;

  // The clock to use when deciding which error type to display. Used for
  // testing.
  base::Clock* testing_clock_ = nullptr;

  network_time::NetworkTimeTracker* network_time_tracker_ = nullptr;

  base::OnceClosure report_network_connectivity_callback_;

  enum EnterpriseManaged {
    ENTERPRISE_MANAGED_STATUS_NOT_SET,
    ENTERPRISE_MANAGED_STATUS_TRUE,
    ENTERPRISE_MANAGED_STATUS_FALSE
  };
  EnterpriseManaged is_enterprise_managed_for_testing_;

  enum OSCaptivePortalStatus {
    OS_CAPTIVE_PORTAL_STATUS_NOT_SET,
    OS_CAPTIVE_PORTAL_STATUS_BEHIND_PORTAL,
    OS_CAPTIVE_PORTAL_STATUS_NOT_BEHIND_PORTAL,
  };
  OSCaptivePortalStatus os_captive_portal_status_for_testing_;

  std::unique_ptr<SSLErrorAssistant> ssl_error_assistant_;
};

ConfigSingleton::ConfigSingleton()
    : interstitial_delay_(
          base::TimeDelta::FromMilliseconds(kInterstitialDelayInMilliseconds)),
      is_enterprise_managed_for_testing_(ENTERPRISE_MANAGED_STATUS_NOT_SET),
      os_captive_portal_status_for_testing_(OS_CAPTIVE_PORTAL_STATUS_NOT_SET),
      ssl_error_assistant_(std::make_unique<SSLErrorAssistant>()) {}

base::TimeDelta ConfigSingleton::interstitial_delay() const {
  return interstitial_delay_;
}

SSLErrorHandler::TimerStartedCallback* ConfigSingleton::timer_started_callback()
    const {
  return timer_started_callback_;
}

network_time::NetworkTimeTracker* ConfigSingleton::network_time_tracker()
    const {
  return network_time_tracker_ ? network_time_tracker_
                               : g_browser_process->network_time_tracker();
}

base::Clock* ConfigSingleton::clock() const {
  return testing_clock_;
}

void ConfigSingleton::ResetForTesting() {
  interstitial_delay_ =
      base::TimeDelta::FromMilliseconds(kInterstitialDelayInMilliseconds);
  timer_started_callback_ = nullptr;
  network_time_tracker_ = nullptr;
  testing_clock_ = nullptr;
  ssl_error_assistant_->ResetForTesting();
  is_enterprise_managed_for_testing_ = ENTERPRISE_MANAGED_STATUS_NOT_SET;
  os_captive_portal_status_for_testing_ = OS_CAPTIVE_PORTAL_STATUS_NOT_SET;
}

void ConfigSingleton::SetInterstitialDelayForTesting(
    const base::TimeDelta& delay) {
  interstitial_delay_ = delay;
}

void ConfigSingleton::SetTimerStartedCallbackForTesting(
    SSLErrorHandler::TimerStartedCallback* callback) {
  DCHECK(!callback || !callback->is_null());
  timer_started_callback_ = callback;
}

void ConfigSingleton::SetClockForTesting(base::Clock* clock) {
  testing_clock_ = clock;
}

void ConfigSingleton::SetNetworkTimeTrackerForTesting(
    network_time::NetworkTimeTracker* tracker) {
  network_time_tracker_ = tracker;
}

void ConfigSingleton::SetReportNetworkConnectivityCallbackForTesting(
    base::OnceClosure closure) {
  report_network_connectivity_callback_ = std::move(closure);
}

void ConfigSingleton::SetEnterpriseManagedForTesting(bool enterprise_managed) {
  if (enterprise_managed) {
    is_enterprise_managed_for_testing_ = ENTERPRISE_MANAGED_STATUS_TRUE;
  } else {
    is_enterprise_managed_for_testing_ = ENTERPRISE_MANAGED_STATUS_FALSE;
  }
}

bool ConfigSingleton::IsEnterpriseManagedFlagSetForTesting() const {
  if (is_enterprise_managed_for_testing_ == ENTERPRISE_MANAGED_STATUS_NOT_SET) {
    return false;
  }
  return true;
}

int ConfigSingleton::GetErrorAssistantProtoVersionIdForTesting() const {
  return ssl_error_assistant_->GetErrorAssistantProtoVersionIdForTesting();
}

bool ConfigSingleton::IsEnterpriseManaged() const {
  // Return the value of the testing flag if it's set.
  if (is_enterprise_managed_for_testing_ == ENTERPRISE_MANAGED_STATUS_TRUE) {
    return true;
  }
  if (is_enterprise_managed_for_testing_ == ENTERPRISE_MANAGED_STATUS_FALSE) {
    return false;
  }

#if defined(OS_WIN)
  if (base::win::IsEnterpriseManaged()) {
    return true;
  }
#elif defined(OS_CHROMEOS)
  if (g_browser_process->platform_part()->browser_policy_connector_chromeos()) {
    return true;
  }
#endif  // #if defined(OS_WIN)
  return false;
}

void ConfigSingleton::SetOSReportsCaptivePortalForTesting(
    bool os_reports_captive_portal) {
  os_captive_portal_status_for_testing_ =
      os_reports_captive_portal ? OS_CAPTIVE_PORTAL_STATUS_BEHIND_PORTAL
                                : OS_CAPTIVE_PORTAL_STATUS_NOT_BEHIND_PORTAL;
}

bool ConfigSingleton::DoesOSReportCaptivePortalForTesting() const {
  return os_captive_portal_status_for_testing_ ==
         OS_CAPTIVE_PORTAL_STATUS_BEHIND_PORTAL;
}

void ConfigSingleton::SetErrorAssistantProto(
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> proto) {
  ssl_error_assistant_->SetErrorAssistantProto(std::move(proto));
}

bool ConfigSingleton::IsKnownCaptivePortalCertificate(
    const net::SSLInfo& ssl_info) {
  return ssl_error_assistant_->IsKnownCaptivePortalCertificate(ssl_info);
}

const std::string ConfigSingleton::MatchKnownMITMSoftware(
    const scoped_refptr<net::X509Certificate> cert) {
  return ssl_error_assistant_->MatchKnownMITMSoftware(cert);
}

base::Optional<DynamicInterstitialInfo>
ConfigSingleton::MatchDynamicInterstitial(const net::SSLInfo& ssl_info,
                                          bool is_overridable) {
  return ssl_error_assistant_->MatchDynamicInterstitial(ssl_info,
                                                        is_overridable);
}

class SSLErrorHandlerDelegateImpl : public SSLErrorHandler::Delegate {
 public:
  SSLErrorHandlerDelegateImpl(
      content::WebContents* web_contents,
      const net::SSLInfo& ssl_info,
      Profile* const profile,
      int cert_error,
      int options_mask,
      bool is_superfish,
      const GURL& request_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      const base::Callback<void(content::CertificateRequestResultType)>&
          decision_callback,
      SSLErrorHandler::BlockingPageReadyCallback blocking_page_ready_callback)
      : web_contents_(web_contents),
        ssl_info_(ssl_info),
        profile_(profile),
        cert_error_(cert_error),
        options_mask_(options_mask),
        is_superfish_(is_superfish),
        request_url_(request_url),
        ssl_cert_reporter_(std::move(ssl_cert_reporter)),
        decision_callback_(decision_callback),
        blocking_page_ready_callback_(std::move(blocking_page_ready_callback)) {
  }
  ~SSLErrorHandlerDelegateImpl() override;

  // SSLErrorHandler::Delegate methods:
  void CheckForCaptivePortal() override;
  bool DoesOSReportCaptivePortal() override;
  bool GetSuggestedUrl(const std::vector<std::string>& dns_names,
                       GURL* suggested_url) const override;
  void CheckSuggestedUrl(
      const GURL& suggested_url,
      const CommonNameMismatchHandler::CheckUrlCallback& callback) override;
  void NavigateToSuggestedURL(const GURL& suggested_url) override;
  bool IsErrorOverridable() const override;
  void ShowCaptivePortalInterstitial(const GURL& landing_url) override;
  void ShowMITMSoftwareInterstitial(const std::string& mitm_software_name,
                                    bool is_enterprise_managed) override;
  void ShowSSLInterstitial(const GURL& support_url) override;
  void ShowBadClockInterstitial(const base::Time& now,
                                ssl_errors::ClockState clock_state) override;
  void ReportNetworkConnectivity(base::OnceClosure callback) override;

 private:
  // Calls the |blocking_page_ready_callback_| if it's not null, else calls
  // Show() on the given interstitial.
  void OnBlockingPageReady(
      security_interstitials::SecurityInterstitialPage* interstitial_page);

  content::WebContents* web_contents_;
  const net::SSLInfo ssl_info_;
  Profile* const profile_;
  const int cert_error_;
  const int options_mask_;
  const bool is_superfish_;
  const GURL request_url_;
  std::unique_ptr<CommonNameMismatchHandler> common_name_mismatch_handler_;
  std::unique_ptr<SSLCertReporter> ssl_cert_reporter_;
  const base::Callback<void(content::CertificateRequestResultType)>
      decision_callback_;
  SSLErrorHandler::BlockingPageReadyCallback blocking_page_ready_callback_;
};

SSLErrorHandlerDelegateImpl::~SSLErrorHandlerDelegateImpl() {
  if (common_name_mismatch_handler_) {
    common_name_mismatch_handler_->Cancel();
    common_name_mismatch_handler_.reset();
  }
}

void SSLErrorHandlerDelegateImpl::CheckForCaptivePortal() {
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalService* captive_portal_service =
      CaptivePortalServiceFactory::GetForProfile(profile_);
  captive_portal_service->DetectCaptivePortal();
#else
  NOTREACHED();
#endif
}

bool SSLErrorHandlerDelegateImpl::DoesOSReportCaptivePortal() {
#if defined(OS_ANDROID) || defined(OS_WIN)
  return chrome::IsBehindCaptivePortal();
#else
  return false;
#endif
}

bool SSLErrorHandlerDelegateImpl::GetSuggestedUrl(
    const std::vector<std::string>& dns_names,
    GURL* suggested_url) const {
  return CommonNameMismatchHandler::GetSuggestedUrl(request_url_, dns_names,
                                                    suggested_url);
}

void SSLErrorHandlerDelegateImpl::CheckSuggestedUrl(
    const GURL& suggested_url,
    const CommonNameMismatchHandler::CheckUrlCallback& callback) {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory(
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess());
  common_name_mismatch_handler_.reset(
      new CommonNameMismatchHandler(request_url_, url_loader_factory));

  common_name_mismatch_handler_->CheckSuggestedUrl(suggested_url, callback);
}

void SSLErrorHandlerDelegateImpl::NavigateToSuggestedURL(
    const GURL& suggested_url) {
  content::NavigationController::LoadURLParams load_params(suggested_url);
  load_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  web_contents_->GetController().LoadURLWithParams(load_params);
}

bool SSLErrorHandlerDelegateImpl::IsErrorOverridable() const {
  return SSLBlockingPage::IsOverridable(options_mask_);
}

void SSLErrorHandlerDelegateImpl::ShowCaptivePortalInterstitial(
    const GURL& landing_url) {
  // Show captive portal blocking page. The interstitial owns the blocking page.
  OnBlockingPageReady(new CaptivePortalBlockingPage(
      web_contents_, request_url_, landing_url, std::move(ssl_cert_reporter_),
      ssl_info_, cert_error_, decision_callback_));
}

void SSLErrorHandlerDelegateImpl::ShowMITMSoftwareInterstitial(
    const std::string& mitm_software_name,
    bool is_enterprise_managed) {
  // Show MITM software blocking page. The interstitial owns the blocking page.
  OnBlockingPageReady(new MITMSoftwareBlockingPage(
      web_contents_, cert_error_, request_url_, std::move(ssl_cert_reporter_),
      ssl_info_, mitm_software_name, is_enterprise_managed,
      decision_callback_));
}

void SSLErrorHandlerDelegateImpl::ShowSSLInterstitial(const GURL& support_url) {
  // Show SSL blocking page. The interstitial owns the blocking page.
  OnBlockingPageReady(SSLBlockingPage::Create(
      web_contents_, cert_error_, ssl_info_, request_url_, options_mask_,
      base::Time::NowFromSystemTime(), support_url,
      std::move(ssl_cert_reporter_), is_superfish_, decision_callback_));
}

void SSLErrorHandlerDelegateImpl::ShowBadClockInterstitial(
    const base::Time& now,
    ssl_errors::ClockState clock_state) {
  // Show bad clock page. The interstitial owns the blocking page.
  OnBlockingPageReady(new BadClockBlockingPage(
      web_contents_, cert_error_, ssl_info_, request_url_, now, clock_state,
      std::move(ssl_cert_reporter_), decision_callback_));
}

void SSLErrorHandlerDelegateImpl::ReportNetworkConnectivity(
    base::OnceClosure callback) {
#if defined(OS_ANDROID)
  chrome::android::ReportNetworkConnectivity(
      base::android::AttachCurrentThread());
#else
// Nothing to do on other platforms.
#endif
  if (callback)
    std::move(callback).Run();
}

void SSLErrorHandlerDelegateImpl::OnBlockingPageReady(
    security_interstitials::SecurityInterstitialPage* interstitial_page) {
  if (blocking_page_ready_callback_.is_null()) {
    interstitial_page->Show();
  } else {
    std::move(blocking_page_ready_callback_)
        .Run(std::unique_ptr<security_interstitials::SecurityInterstitialPage>(
            interstitial_page));
  }
}

int IsCertErrorFatal(int cert_error) {
  switch (cert_error) {
    case net::ERR_CERT_COMMON_NAME_INVALID:
    case net::ERR_CERT_DATE_INVALID:
    case net::ERR_CERT_AUTHORITY_INVALID:
    case net::ERR_CERT_WEAK_SIGNATURE_ALGORITHM:
    case net::ERR_CERT_WEAK_KEY:
    case net::ERR_CERT_NAME_CONSTRAINT_VIOLATION:
    case net::ERR_CERT_VALIDITY_TOO_LONG:
    case net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED:
    case net::ERR_CERT_SYMANTEC_LEGACY:
      return false;
    case net::ERR_CERT_CONTAINS_ERRORS:
    case net::ERR_CERT_REVOKED:
    case net::ERR_CERT_INVALID:
    case net::ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY:
    case net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN:
      return true;
    default:
      NOTREACHED();
      return true;
  }
}

}  // namespace

static base::LazyInstance<ConfigSingleton>::Leaky g_config =
    LAZY_INSTANCE_INITIALIZER;

void SSLErrorHandler::HandleSSLError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool expired_previous_decision,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    const base::Callback<void(content::CertificateRequestResultType)>&
        decision_callback,
    base::OnceCallback<
        void(std::unique_ptr<security_interstitials::SecurityInterstitialPage>)>
        blocking_page_ready_callback) {
  DCHECK(!FromWebContents(web_contents));

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  bool hard_override_disabled =
      !profile->GetPrefs()->GetBoolean(prefs::kSSLErrorOverrideAllowed);
  bool is_superfish_cert = IsSuperfish(ssl_info.cert);
  UMA_HISTOGRAM_BOOLEAN("interstitial.ssl_error_handler.superfish",
                        is_superfish_cert);
  bool is_superfish =
      base::FeatureList::IsEnabled(kSuperfishInterstitial) && is_superfish_cert;
  int options_mask = CalculateOptionsMask(
      cert_error, hard_override_disabled, ssl_info.is_fatal_cert_error,
      is_superfish, expired_previous_decision);

  SSLErrorHandler* error_handler = new SSLErrorHandler(
      std::unique_ptr<SSLErrorHandler::Delegate>(
          new SSLErrorHandlerDelegateImpl(
              web_contents, ssl_info, profile, cert_error, options_mask,
              is_superfish, request_url, std::move(ssl_cert_reporter),
              decision_callback, std::move(blocking_page_ready_callback))),
      web_contents, profile, cert_error, ssl_info, request_url,
      decision_callback);
  web_contents->SetUserData(UserDataKey(), base::WrapUnique(error_handler));
  error_handler->StartHandlingError();
}

// static
void SSLErrorHandler::ResetConfigForTesting() {
  g_config.Pointer()->ResetForTesting();
}

// static
void SSLErrorHandler::SetInterstitialDelayForTesting(
    const base::TimeDelta& delay) {
  g_config.Pointer()->SetInterstitialDelayForTesting(delay);
}

// static
void SSLErrorHandler::SetInterstitialTimerStartedCallbackForTesting(
    TimerStartedCallback* callback) {
  g_config.Pointer()->SetTimerStartedCallbackForTesting(callback);
}

// static
void SSLErrorHandler::SetClockForTesting(base::Clock* testing_clock) {
  g_config.Pointer()->SetClockForTesting(testing_clock);
}

// static
void SSLErrorHandler::SetNetworkTimeTrackerForTesting(
    network_time::NetworkTimeTracker* tracker) {
  g_config.Pointer()->SetNetworkTimeTrackerForTesting(tracker);
}

// static
void SSLErrorHandler::SetReportNetworkConnectivityCallbackForTesting(
    base::OnceClosure closure) {
  g_config.Pointer()->SetReportNetworkConnectivityCallbackForTesting(
      std::move(closure));
}

// static
void SSLErrorHandler::SetEnterpriseManagedForTesting(bool enterprise_managed) {
  g_config.Pointer()->SetEnterpriseManagedForTesting(enterprise_managed);
}

// static
bool SSLErrorHandler::IsEnterpriseManagedFlagSetForTesting() {
  return g_config.Pointer()->IsEnterpriseManagedFlagSetForTesting();
}

// static
std::string SSLErrorHandler::GetHistogramNameForTesting() {
  return kHistogram;
}

// static
int SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() {
  return g_config.Pointer()->GetErrorAssistantProtoVersionIdForTesting();
}

// static
void SSLErrorHandler::SetOSReportsCaptivePortalForTesting(
    bool os_reports_captive_portal) {
  g_config.Pointer()->SetOSReportsCaptivePortalForTesting(
      os_reports_captive_portal);
}

bool SSLErrorHandler::IsTimerRunningForTesting() const {
  return timer_.IsRunning();
}

// static
void SSLErrorHandler::SetErrorAssistantProto(
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> config_proto) {
  g_config.Pointer()->SetErrorAssistantProto(std::move(config_proto));
}

SSLErrorHandler::SSLErrorHandler(
    std::unique_ptr<Delegate> delegate,
    content::WebContents* web_contents,
    Profile* profile,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    const base::Callback<void(content::CertificateRequestResultType)>&
        decision_callback)
    : content::WebContentsObserver(web_contents),
      delegate_(std::move(delegate)),
      web_contents_(web_contents),
      profile_(profile),
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      request_url_(request_url),
      decision_callback_(decision_callback),
      weak_ptr_factory_(this) {}

SSLErrorHandler::~SSLErrorHandler() {
}

void SSLErrorHandler::StartHandlingError() {
  RecordUMA(HANDLE_ALL);

  if (ssl_errors::ErrorInfo::NetErrorToErrorType(cert_error_) ==
      ssl_errors::ErrorInfo::CERT_DATE_INVALID) {
    HandleCertDateInvalidError();
    return;
  }

  base::Optional<DynamicInterstitialInfo> dynamic_interstitial =
      g_config.Pointer()->MatchDynamicInterstitial(
          ssl_info_, delegate_->IsErrorOverridable());
  if (dynamic_interstitial) {
    ShowDynamicInterstitial(dynamic_interstitial.value());
    return;
  }

  // Ideally, a captive portal interstitial should only be displayed if the only
  // SSL error is a name mismatch error. However, captive portal detector always
  // opens a new tab if it detects a portal ignoring the types of SSL errors. To
  // be consistent with captive portal detector, use the result of OS detection
  // without checking only_error_is_name_mismatch.
  if (IsCaptivePortalInterstitialEnabled() &&
      (g_config.Pointer()->DoesOSReportCaptivePortalForTesting() ||
       delegate_->DoesOSReportCaptivePortal())) {
    delegate_->ReportNetworkConnectivity(
        g_config.Pointer()->report_network_connectivity_callback());
    RecordUMA(OS_REPORTS_CAPTIVE_PORTAL);
    ShowCaptivePortalInterstitial(GURL());
    return;
  }

  const bool only_error_is_name_mismatch =
      IsOnlyCertError(net::CERT_STATUS_COMMON_NAME_INVALID);

  // Check known captive portal certificate list if the only error is
  // name-mismatch. If there are multiple errors, it indicates that the captive
  // portal landing page itself will have SSL errors, and so it's not a very
  // helpful place to direct the user to go.
  if (only_error_is_name_mismatch) {
    delegate_->ReportNetworkConnectivity(
        g_config.Pointer()->report_network_connectivity_callback());

    if (base::FeatureList::IsEnabled(kCaptivePortalCertificateList) &&
        g_config.Pointer()->IsKnownCaptivePortalCertificate(ssl_info_)) {
      RecordUMA(CAPTIVE_PORTAL_CERT_FOUND);
      ShowCaptivePortalInterstitial(GURL());
      return;
    }
  }

  // The MITM software interstitial is displayed if and only if:
  // - the error thrown is not overridable
  // - the only certificate error is CERT_STATUS_AUTHORITY_INVALID
  // - the certificate contains a string that indicates it was issued by a
  //   MITM software
  if (IsMITMSoftwareInterstitialEnabled() && !delegate_->IsErrorOverridable() &&
      IsOnlyCertError(net::CERT_STATUS_AUTHORITY_INVALID)) {
    const std::string found_mitm_software =
        g_config.Pointer()->MatchKnownMITMSoftware(ssl_info_.cert);
    if (!found_mitm_software.empty()) {
      ShowMITMSoftwareInterstitial(found_mitm_software,
                                   g_config.Pointer()->IsEnterpriseManaged());
      return;
    }
  }

  if (IsSSLCommonNameMismatchHandlingEnabled() &&
      cert_error_ == net::ERR_CERT_COMMON_NAME_INVALID &&
      delegate_->IsErrorOverridable()) {
    std::vector<std::string> dns_names;
    ssl_info_.cert->GetSubjectAltName(&dns_names, nullptr);
    GURL suggested_url;
    if (!dns_names.empty() &&
        delegate_->GetSuggestedUrl(dns_names, &suggested_url)) {
      RecordUMA(WWW_MISMATCH_FOUND_IN_SAN);

      // Show the SSL interstitial if |CERT_STATUS_COMMON_NAME_INVALID| is not
      // the only error. Need not check for captive portal in this case.
      // (See the comment below).
      if (!only_error_is_name_mismatch) {
        ShowSSLInterstitial();
        return;
      }
      delegate_->CheckSuggestedUrl(
          suggested_url,
          base::Bind(&SSLErrorHandler::CommonNameMismatchHandlerCallback,
                     weak_ptr_factory_.GetWeakPtr()));
      timer_.Start(FROM_HERE, g_config.Pointer()->interstitial_delay(), this,
                   &SSLErrorHandler::ShowSSLInterstitial);

      if (g_config.Pointer()->timer_started_callback())
        g_config.Pointer()->timer_started_callback()->Run(web_contents_);

      // Do not check for a captive portal in this case, because a captive
      // portal most likely cannot serve a valid certificate which passes the
      // similarity check.
      return;
    }
  }

  // Always listen to captive portal notifications, otherwise build fails
  // because profile_ isn't used. This is a no-op on platforms where
  // captive portal detection is disabled.
  registrar_.Add(this, chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
                 content::Source<Profile>(profile_));

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalTabHelper* captive_portal_tab_helper =
      CaptivePortalTabHelper::FromWebContents(web_contents_);
  if (captive_portal_tab_helper) {
    captive_portal_tab_helper->OnSSLCertError(ssl_info_);
  }

  if (IsCaptivePortalInterstitialEnabled()) {
    delegate_->CheckForCaptivePortal();
    timer_.Start(FROM_HERE, g_config.Pointer()->interstitial_delay(), this,
                 &SSLErrorHandler::ShowSSLInterstitial);
    if (g_config.Pointer()->timer_started_callback())
      g_config.Pointer()->timer_started_callback()->Run(web_contents_);
    return;
  }
#endif
  // Display an SSL interstitial.
  ShowSSLInterstitial();
}

void SSLErrorHandler::ShowCaptivePortalInterstitial(const GURL& landing_url) {
  // Show captive portal blocking page. The interstitial owns the blocking page.
  RecordUMA(delegate_->IsErrorOverridable()
                ? SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE
                : SHOW_CAPTIVE_PORTAL_INTERSTITIAL_NONOVERRIDABLE);
  delegate_->ShowCaptivePortalInterstitial(landing_url);

  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this". It also destroys the timer.
  web_contents_->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::ShowMITMSoftwareInterstitial(
    const std::string& mitm_software_name,
    bool is_enterprise_managed) {
  // Show SSL blocking page. The interstitial owns the blocking page.
  RecordUMA(SHOW_MITM_SOFTWARE_INTERSTITIAL);
  delegate_->ShowMITMSoftwareInterstitial(mitm_software_name,
                                          is_enterprise_managed);
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents_->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::ShowSSLInterstitial() {
  // Show SSL blocking page. The interstitial owns the blocking page.
  RecordUMA(delegate_->IsErrorOverridable()
                ? SHOW_SSL_INTERSTITIAL_OVERRIDABLE
                : SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE);
  delegate_->ShowSSLInterstitial(GURL());
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents_->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::ShowBadClockInterstitial(
    const base::Time& now,
    ssl_errors::ClockState clock_state) {
  RecordUMA(SHOW_BAD_CLOCK);
  delegate_->ShowBadClockInterstitial(now, clock_state);
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents_->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::ShowDynamicInterstitial(
    const DynamicInterstitialInfo dynamic_interstitial) {
  switch (dynamic_interstitial.interstitial_type) {
    case chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_NONE:
      NOTREACHED();
      return;
    case chrome_browser_ssl::DynamicInterstitial::INTERSTITIAL_PAGE_SSL:
      delegate_->ShowSSLInterstitial(dynamic_interstitial.support_url);
      return;
    case chrome_browser_ssl::DynamicInterstitial::
        INTERSTITIAL_PAGE_CAPTIVE_PORTAL:
      delegate_->ShowCaptivePortalInterstitial(GURL());
      return;
    case chrome_browser_ssl::DynamicInterstitial::
        INTERSTITIAL_PAGE_MITM_SOFTWARE:
      DCHECK(!dynamic_interstitial.mitm_software_name.empty());
      delegate_->ShowMITMSoftwareInterstitial(
          dynamic_interstitial.mitm_software_name,
          g_config.Pointer()->IsEnterpriseManaged());
      return;
  }
}

void SSLErrorHandler::CommonNameMismatchHandlerCallback(
    CommonNameMismatchHandler::SuggestedUrlCheckResult result,
    const GURL& suggested_url) {
  timer_.Stop();
  if (result == CommonNameMismatchHandler::SuggestedUrlCheckResult::
                    SUGGESTED_URL_AVAILABLE) {
    RecordUMA(WWW_MISMATCH_URL_AVAILABLE);
    CommonNameMismatchRedirectObserver::AddToConsoleAfterNavigation(
        web_contents(), request_url_.host(), suggested_url.host());
    delegate_->NavigateToSuggestedURL(suggested_url);
  } else {
    RecordUMA(WWW_MISMATCH_URL_NOT_AVAILABLE);
    ShowSSLInterstitial();
  }
}

void SSLErrorHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  DCHECK_EQ(chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT, type);

  timer_.Stop();
  CaptivePortalService::Results* results =
      content::Details<CaptivePortalService::Results>(details).ptr();
  if (results->result == captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL)
    ShowCaptivePortalInterstitial(results->landing_url);
  else
    ShowSSLInterstitial();
#else
  NOTREACHED();
#endif
}

void SSLErrorHandler::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Destroy the error handler on all new navigations. This ensures that the
  // handler is properly recreated when a hanging page is navigated to an SSL
  // error, even when the tab's WebContents doesn't change.
  DeleteSSLErrorHandler();
}

void SSLErrorHandler::NavigationStopped() {
// Destroy the error handler when the page load is stopped.
  DeleteSSLErrorHandler();
}

void SSLErrorHandler::DeleteSSLErrorHandler() {
  // Need to explicity deny the certificate via the callback, otherwise memory
  // is leaked.
  if (!decision_callback_.is_null()) {
    base::ResetAndReturn(&decision_callback_)
        .Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
  }
  delegate_.reset();
  // Deletes |this| and also destroys the timer.
  web_contents_->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::HandleCertDateInvalidError() {
  const base::TimeTicks now = base::TimeTicks::Now();
  timer_.Start(FROM_HERE, g_config.Pointer()->interstitial_delay(),
               base::Bind(&SSLErrorHandler::HandleCertDateInvalidErrorImpl,
                          base::Unretained(this), now));
  // Try kicking off a time fetch to get an up-to-date estimate of the
  // true time. This will only have an effect if network time is
  // unavailable or if there is not already a query in progress.
  //
  // Pass a weak pointer as the callback; if the timer fires before the
  // fetch completes and shows an interstitial, this SSLErrorHandler
  // will be deleted.
  network_time::NetworkTimeTracker* tracker =
      g_config.Pointer()->network_time_tracker();
  if (!tracker->StartTimeFetch(
          base::Bind(&SSLErrorHandler::HandleCertDateInvalidErrorImpl,
                     weak_ptr_factory_.GetWeakPtr(), now))) {
    HandleCertDateInvalidErrorImpl(now);
    return;
  }

  if (g_config.Pointer()->timer_started_callback())
    g_config.Pointer()->timer_started_callback()->Run(web_contents_);
}

void SSLErrorHandler::HandleCertDateInvalidErrorImpl(
    base::TimeTicks started_handling_error) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "interstitial.ssl_error_handler.cert_date_error_delay",
      base::TimeTicks::Now() - started_handling_error,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(4),
      50);

  timer_.Stop();
  base::Clock* testing_clock = g_config.Pointer()->clock();
  const base::Time now =
      testing_clock ? testing_clock->Now() : base::Time::NowFromSystemTime();

  network_time::NetworkTimeTracker* tracker =
      g_config.Pointer()->network_time_tracker();
  ssl_errors::ClockState clock_state = ssl_errors::GetClockState(now, tracker);
  if (clock_state == ssl_errors::CLOCK_STATE_FUTURE ||
      clock_state == ssl_errors::CLOCK_STATE_PAST) {
    ShowBadClockInterstitial(now, clock_state);
    return;  // |this| is deleted after showing the interstitial.
  }
  ShowSSLInterstitial();
}

// Returns true if |only_cert_error_expected| is the only error code present in
// the certificate. The parameter |only_cert_error_expected| is a
// net::CertStatus code representing the most serious error identified on the
// certificate. For example, this could be net::CERT_STATUS_COMMON_NAME_INVALID.
// This function is useful for rendering interstitials that are triggered by one
// specific error code only.
bool SSLErrorHandler::IsOnlyCertError(
    net::CertStatus only_cert_error_expected) const {
  const net::CertStatus other_errors =
      ssl_info_.cert_status ^ only_cert_error_expected;

  return cert_error_ ==
             net::MapCertStatusToNetError(only_cert_error_expected) &&
         (!net::IsCertStatusError(other_errors) ||
          net::IsCertStatusMinorError(ssl_info_.cert_status));
}

// static
int SSLErrorHandler::CalculateOptionsMask(int cert_error,
                                          bool hard_override_disabled,
                                          bool should_ssl_errors_be_fatal,
                                          bool is_superfish,
                                          bool expired_previous_decision) {
  int options_mask = 0;
  if (!IsCertErrorFatal(cert_error) && !hard_override_disabled &&
      !should_ssl_errors_be_fatal && !is_superfish) {
    options_mask |= security_interstitials::SSLErrorUI::SOFT_OVERRIDE_ENABLED;
  }
  if (hard_override_disabled) {
    options_mask |= security_interstitials::SSLErrorUI::HARD_OVERRIDE_DISABLED;
  }
  if (should_ssl_errors_be_fatal) {
    options_mask |= security_interstitials::SSLErrorUI::STRICT_ENFORCEMENT;
  }
  if (expired_previous_decision) {
    options_mask |=
        security_interstitials::SSLErrorUI::EXPIRED_BUT_PREVIOUSLY_ALLOWED;
  }
  return options_mask;
}
