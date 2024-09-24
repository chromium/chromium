// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_service.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service_factory.h"
#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"
#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"
#include "chrome/browser/safe_browsing/chrome_user_population_helper.h"
#include "chrome/browser/safe_browsing/chrome_v4_protocol_config_provider.h"
#include "chrome/browser/safe_browsing/network_context_service.h"
#include "chrome/browser/safe_browsing/network_context_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/services_delegate.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/content/browser/triggers/trigger_manager.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/browser/realtime/policy_engine.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "services/network/public/cpp/features.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/install_static/install_util.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/android/safe_browsing_referring_app_bridge_android.h"
#endif

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "components/safe_browsing/content/browser/password_protection/password_protection_service.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/hash_realtime_service_factory.h"
#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif

using content::BrowserThread;

namespace safe_browsing {

using enum ExtendedReportingLevel;

namespace {

// The number of user gestures to trace back for the referrer chain.
const int kReferrerChainUserGestureLimit = 2;

#if BUILDFLAG(FULL_SAFE_BROWSING)
void PopulateDownloadWarningActions(download::DownloadItem* download,
                                    ClientSafeBrowsingReportRequest* report) {
  for (auto& event :
       DownloadItemWarningData::GetWarningActionEvents(download)) {
    report->mutable_download_warning_actions()->Add(
        DownloadItemWarningData::ConstructCsbrrDownloadWarningAction(event));
  }
  base::UmaHistogramCounts100(
      "SafeBrowsing.ClientSafeBrowsingReport.DownloadWarningActionSize",
      report->download_warning_actions_size());
}

std::unique_ptr<ClientSafeBrowsingReportRequest> CreateDownloadReport(
    download::DownloadItem* download,
    ClientSafeBrowsingReportRequest::ReportType report_type,
    bool did_proceed,
    std::optional<bool> show_download_in_folder) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(download));
  auto report = std::make_unique<ClientSafeBrowsingReportRequest>();
  report->set_type(report_type);
  report->set_download_verdict(
      DownloadProtectionService::GetDownloadProtectionVerdict(download));
  report->set_url(download->GetURL().spec());
  report->set_did_proceed(did_proceed);
  if (show_download_in_folder.has_value()) {
    report->set_show_download_in_folder(show_download_in_folder.value());
  }
  std::string token = DownloadProtectionService::GetDownloadPingToken(download);
  if (!token.empty()) {
    report->set_token(std::move(token));
  }
  if (IsExtendedReportingEnabled(*profile->GetPrefs())) {
    PopulateDownloadWarningActions(download, report.get());
    base::Time warning_first_shown_time =
        DownloadItemWarningData::WarningFirstShownTime(download);
    if (!warning_first_shown_time.is_null()) {
      report->set_warning_shown_timestamp_msec(
          warning_first_shown_time.InMillisecondsSinceUnixEpoch());
    }
  }
  return report;
}
#endif

void OnGotCookies(
    std::unique_ptr<mojo::Remote<network::mojom::CookieManager>> remote,
    const std::vector<net::CanonicalCookie>& cookies) {
  base::UmaHistogramBoolean("SafeBrowsing.HasCookieAtStartup2",
                            !cookies.empty());
  if (!cookies.empty()) {
    base::TimeDelta age = base::Time::Now() - cookies.front().CreationDate();
    // Cookies can be up to 6 months old. Using millisecond precision over such
    // a long time period overflows numeric limits. Instead, use a counts
    // histogram and lower granularity.
    base::UmaHistogramCounts10000("SafeBrowsing.CookieAgeHours2",
                                  age.InHours());
  }
}

}  // namespace

// static
base::FilePath SafeBrowsingServiceImpl::GetCookieFilePathForTesting() {
  return base::FilePath(SafeBrowsingServiceImpl::GetBaseFilename().value() +
                        safe_browsing::kCookiesFile);
}

// static
base::FilePath SafeBrowsingServiceImpl::GetBaseFilename() {
  base::FilePath path;
  bool result = base::PathService::Get(chrome::DIR_USER_DATA, &path);
  DCHECK(result);
  return path.Append(safe_browsing::kSafeBrowsingBaseFilename);
}

// static
bool SafeBrowsingServiceImpl::IsUserEligibleForESBPromo(Profile* profile) {
  if (IsSafeBrowsingPolicyManaged(*profile->GetPrefs()) ||
      profile->IsOffTheRecord()) {
    return false;
  }
  return GetSafeBrowsingState(*profile->GetPrefs()) ==
         SafeBrowsingState::STANDARD_PROTECTION;
}

SafeBrowsingServiceImpl::SafeBrowsingServiceImpl()
    : services_delegate_(ServicesDelegate::Create(this)),
      estimated_extended_reporting_by_prefs_(SBER_LEVEL_OFF),
      shutdown_(false),
      enabled_(false),
      enabled_by_prefs_(false) {}

SafeBrowsingServiceImpl::~SafeBrowsingServiceImpl() {
  // We should have already been shut down. If we're still enabled, then the
  // database isn't going to be closed properly, which could lead to corruption.
  DCHECK(!enabled_);
}

void SafeBrowsingServiceImpl::Initialize() {
  // Ensure FileTypePolicies's Singleton is instantiated during startup.
  // This guarantees we'll log UMA metrics about its state.
  FileTypePolicies::GetInstance();

  base::FilePath user_data_dir;
  bool result = base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(result);

  WebUIInfoSingleton::GetInstance()->set_safe_browsing_service(this);

  ui_manager_ = CreateUIManager();

  services_delegate_->Initialize();

  // Needs to happen after |ui_manager_| is created.
  CreateTriggerManager();

  // Track profile creation and destruction.
  if (g_browser_process->profile_manager()) {
    g_browser_process->profile_manager()->AddObserver(this);
    DCHECK_EQ(0U,
              g_browser_process->profile_manager()->GetLoadedProfiles().size());
  }

  // Register all the delayed analysis to the incident reporting service.
  RegisterAllDelayedAnalysis();
}

void SafeBrowsingServiceImpl::ShutDown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  shutdown_ = true;

  // Remove Profile creation/destruction observers.
  if (g_browser_process->profile_manager()) {
    g_browser_process->profile_manager()->RemoveObserver(this);
  }
  observed_profiles_.RemoveAllObservations();

  // Delete the PrefChangeRegistrars, whose dtors also unregister |this| as an
  // observer of the preferences.
  prefs_map_.clear();
  user_population_prefs_.clear();

  Stop(true);

  services_delegate_->ShutdownServices();

  WebUIInfoSingleton::GetInstance()->set_safe_browsing_service(nullptr);

  proxy_config_monitor_.reset();
}

network::mojom::NetworkContext* SafeBrowsingServiceImpl::GetNetworkContext(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NetworkContextService* service =
      NetworkContextServiceFactory::GetForBrowserContext(browser_context);
  if (!service) {
    return nullptr;
  }

  return service->GetNetworkContext();
}

scoped_refptr<network::SharedURLLoaderFactory>
SafeBrowsingServiceImpl::GetURLLoaderFactory(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (url_loader_factory_for_testing_) {
    return url_loader_factory_for_testing_;
  }

  NetworkContextService* service =
      NetworkContextServiceFactory::GetForBrowserContext(browser_context);
  if (!service) {
    return nullptr;
  }

  return service->GetURLLoaderFactory();
}

void SafeBrowsingServiceImpl::FlushNetworkInterfaceForTesting(
    content::BrowserContext* browser_context) {
  NetworkContextService* service =
      NetworkContextServiceFactory::GetForBrowserContext(browser_context);
  if (!service) {
    return;
  }

  service->FlushNetworkInterfaceForTesting();
}

const scoped_refptr<SafeBrowsingUIManager>&
SafeBrowsingServiceImpl::ui_manager() const {
  return ui_manager_;
}

const scoped_refptr<SafeBrowsingDatabaseManager>&
SafeBrowsingServiceImpl::database_manager() const {
  return services_delegate_->database_manager();
}

ReferrerChainProvider*
SafeBrowsingServiceImpl::GetReferrerChainProviderFromBrowserContext(
    content::BrowserContext* browser_context) {
  return SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
      browser_context);
}

#if BUILDFLAG(IS_ANDROID)
ReferringAppInfo SafeBrowsingServiceImpl::GetReferringAppInfo(
    content::WebContents* web_contents) {
  return safe_browsing::GetReferringAppInfo(web_contents);
}
#endif

TriggerManager* SafeBrowsingServiceImpl::trigger_manager() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return trigger_manager_.get();
}

PasswordProtectionService*
SafeBrowsingServiceImpl::GetPasswordProtectionService(Profile* profile) const {
  if (IsSafeBrowsingEnabled(*profile->GetPrefs())) {
    return ChromePasswordProtectionServiceFactory::GetForProfile(profile);
  }
  return nullptr;
}

std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>
SafeBrowsingServiceImpl::CreatePreferenceValidationDelegate(
    Profile* profile) const {
  return services_delegate_->CreatePreferenceValidationDelegate(profile);
}

void SafeBrowsingServiceImpl::RegisterDelayedAnalysisCallback(
    DelayedAnalysisCallback callback) {
  services_delegate_->RegisterDelayedAnalysisCallback(std::move(callback));
}

void SafeBrowsingServiceImpl::AddDownloadManager(
    content::DownloadManager* download_manager) {
  services_delegate_->AddDownloadManager(download_manager);
}

HashRealTimeService* SafeBrowsingServiceImpl::GetHashRealTimeService(
    Profile* profile) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  return safe_browsing::HashRealTimeServiceFactory::GetForProfile(profile);
#else
  return nullptr;
#endif
}

SafeBrowsingUIManager* SafeBrowsingServiceImpl::CreateUIManager() {
  return new SafeBrowsingUIManager(
      std::make_unique<ChromeSafeBrowsingUIManagerDelegate>(),
      std::make_unique<ChromeSafeBrowsingBlockingPageFactory>(),
      GURL(chrome::kChromeUINewTabURL));
}

void SafeBrowsingServiceImpl::RegisterAllDelayedAnalysis() {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  RegisterBinaryIntegrityAnalysis();
#endif
}

V4ProtocolConfig SafeBrowsingServiceImpl::GetV4ProtocolConfig() const {
  return safe_browsing::GetV4ProtocolConfig();
}

void SafeBrowsingServiceImpl::SetDatabaseManagerForTest(
    SafeBrowsingDatabaseManager* database_manager) {
  services_delegate_->SetDatabaseManagerForTest(database_manager);
}

void SafeBrowsingServiceImpl::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!enabled_) {
    enabled_ = true;
    services_delegate_->StartOnUIThread(
        g_browser_process->shared_url_loader_factory(), GetV4ProtocolConfig());
  }
}

void SafeBrowsingServiceImpl::Stop(bool shutdown) {
  ui_manager_->Stop(shutdown);

  services_delegate_->StopOnUIThread(shutdown);

  enabled_ = false;
}

void SafeBrowsingServiceImpl::OnProfileAdded(Profile* profile) {
  // Some services are disabled by default based on the profile type, e.g. the
  // System Profile, in which Safe browsing is not needed.
  if (AreKeyedServicesDisabledForProfileByDefault(profile)) {
    return;
  }

  // Start following the safe browsing preference on |pref_service|.
  PrefService* pref_service = profile->GetPrefs();
  DCHECK(prefs_map_.find(pref_service) == prefs_map_.end());
  std::unique_ptr<PrefChangeRegistrar> registrar =
      std::make_unique<PrefChangeRegistrar>();
  registrar->Init(pref_service);
  registrar->Add(prefs::kSafeBrowsingEnabled,
                 base::BindRepeating(&SafeBrowsingServiceImpl::RefreshState,
                                     base::Unretained(this)));
  // ClientSideDetectionService will need to be refresh the models
  // renderers have if extended-reporting changes.
  registrar->Add(prefs::kSafeBrowsingScoutReportingEnabled,
                 base::BindRepeating(&SafeBrowsingServiceImpl::RefreshState,
                                     base::Unretained(this)));
  registrar->Add(prefs::kSafeBrowsingEnhanced,
                 base::BindRepeating(&SafeBrowsingServiceImpl::RefreshState,
                                     base::Unretained(this)));
  prefs_map_[pref_service] = std::move(registrar);
  RefreshState();

  registrar = std::make_unique<PrefChangeRegistrar>();
  registrar->Init(pref_service);
  registrar->Add(prefs::kSafeBrowsingEnabled,
                 base::BindRepeating(&ClearCachedUserPopulation, profile,
                                     NoCachedPopulationReason::kChangeSbPref));
  registrar->Add(prefs::kSafeBrowsingScoutReportingEnabled,
                 base::BindRepeating(&ClearCachedUserPopulation, profile,
                                     NoCachedPopulationReason::kChangeSbPref));
  registrar->Add(prefs::kSafeBrowsingEnhanced,
                 base::BindRepeating(&ClearCachedUserPopulation, profile,
                                     NoCachedPopulationReason::kChangeSbPref));
  registrar->Add(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      base::BindRepeating(&ClearCachedUserPopulation, profile,
                          NoCachedPopulationReason::kChangeMbbPref));
  user_population_prefs_[pref_service] = std::move(registrar);

  // Record the current pref state for standard protection.
  UMA_HISTOGRAM_BOOLEAN(kSafeBrowsingEnabledHistogramName,
                        pref_service->GetBoolean(prefs::kSafeBrowsingEnabled));
  // Record the current pref state for enhanced protection. Enhanced protection
  // is a subset of the standard protection. Thus, |kSafeBrowsingEnabled| count
  // should always be more than the count of enhanced protection.
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Enhanced",
                        pref_service->GetBoolean(prefs::kSafeBrowsingEnhanced));

  // Record the current enhanced protection pref state for regular profiles only
  if (profiles::IsRegularUserProfile(profile)) {
    UMA_HISTOGRAM_BOOLEAN(
        "SafeBrowsing.Pref.Enhanced.RegularProfile",
        pref_service->GetBoolean(prefs::kSafeBrowsingEnhanced));
  }

  // Extended Reporting metrics are handled together elsewhere.
  RecordExtendedReportingMetrics(*pref_service);

  // TODO(crbug.com/339468572): Set the new pref value in iOS and WebView
  // For users in the extended reporting deprecation experiment group, save the
  // extended reporting preference value.
  if (base::FeatureList::IsEnabled(kExtendedReportingRemovePrefDependency)) {
    pref_service->SetBoolean(
        prefs::kSafeBrowsingScoutReportingEnabledWhenDeprecated,
        pref_service->GetBoolean(prefs::kSafeBrowsingScoutReportingEnabled));
  } else {
    // Set the pref value to false as this feature is not deprecated when the
    // feature flag is off.
    pref_service->SetBoolean(
        prefs::kSafeBrowsingScoutReportingEnabledWhenDeprecated, false);
  }

  SafeBrowsingMetricsCollectorFactory::GetForProfile(profile)->StartLogging();

  CreateServicesForProfile(profile);

  RecordStartupCookieMetrics(profile);
}

void SafeBrowsingServiceImpl::OnOffTheRecordProfileCreated(
    Profile* off_the_record) {
  CreateServicesForProfile(off_the_record);
}

void SafeBrowsingServiceImpl::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.RemoveObservation(profile);
  services_delegate_->RemoveTelemetryService(profile);
  services_delegate_->OnProfileWillBeDestroyed(profile);

  PrefService* pref_service = profile->GetPrefs();
  DCHECK(pref_service);
  prefs_map_.erase(pref_service);
  user_population_prefs_.erase(pref_service);
}

void SafeBrowsingServiceImpl::CreateServicesForProfile(Profile* profile) {
  services_delegate_->CreateTelemetryService(profile);
  observed_profiles_.AddObservation(profile);
}

base::CallbackListSubscription SafeBrowsingServiceImpl::RegisterStateCallback(
    const base::RepeatingClosure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return state_callback_list_.Add(callback);
}

void SafeBrowsingServiceImpl::RefreshState() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check if any profile requires the service to be active.
  enabled_by_prefs_ = false;
  estimated_extended_reporting_by_prefs_ = SBER_LEVEL_OFF;
  for (const auto& pref : prefs_map_) {
    if (IsSafeBrowsingEnabled(*pref.first)) {
      enabled_by_prefs_ = true;

      ExtendedReportingLevel erl =
          safe_browsing::GetExtendedReportingLevel(*pref.first);
      if (erl != SBER_LEVEL_OFF) {
        estimated_extended_reporting_by_prefs_ = erl;
      }
    }
  }

  if (enabled_by_prefs_) {
    Start();
  } else {
    Stop(false);
  }

  state_callback_list_.Notify();

  services_delegate_->RefreshState(enabled_by_prefs_);
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
void SafeBrowsingServiceImpl::SendDownloadReport(
    download::DownloadItem* download,
    ClientSafeBrowsingReportRequest::ReportType report_type,
    bool did_proceed,
    std::optional<bool> show_download_in_folder) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ShouldSendDangerousDownloadReport(download, report_type)) {
    return;
  }
  auto report = CreateDownloadReport(download, report_type, did_proceed,
                                     show_download_in_folder);
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(download));
  PingManager::ReportThreatDetailsResult result =
      ChromePingManagerFactory::GetForBrowserContext(profile)
          ->ReportThreatDetails(std::move(report));
  base::UmaHistogramEnumeration(
      "SafeBrowsing.ClientSafeBrowsingReport.SendDownloadReportResult", result);
  return;
}

void SafeBrowsingServiceImpl::PersistDownloadReportAndSendOnNextStartup(
    download::DownloadItem* download,
    ClientSafeBrowsingReportRequest::ReportType report_type,
    bool did_proceed,
    std::optional<bool> show_download_in_folder) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ShouldSendDangerousDownloadReport(download, report_type)) {
    return;
  }
  auto report = CreateDownloadReport(download, report_type, did_proceed,
                                     show_download_in_folder);
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(download));
  PingManager::PersistThreatDetailsResult result =
      ChromePingManagerFactory::GetForBrowserContext(profile)
          ->PersistThreatDetailsAndReportOnNextStartup(std::move(report));
  base::UmaHistogramEnumeration(
      "SafeBrowsing.ClientSafeBrowsingReport.PersistDownloadReportResult",
      result);
  return;
}

bool SafeBrowsingServiceImpl::SendPhishyInteractionsReport(
    Profile* profile,
    const GURL& url,
    const GURL& page_url,
    const PhishySiteInteractionMap& phishy_interaction_data) {
  if (!profile || !IsExtendedReportingEnabled(*profile->GetPrefs()) ||
      profile->IsOffTheRecord()) {
    return false;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto report = std::make_unique<ClientSafeBrowsingReportRequest>();
  report->set_type(ClientSafeBrowsingReportRequest::PHISHY_SITE_INTERACTIONS);
  report->set_url(url.spec());
  report->set_page_url(page_url.spec());
  for (auto const& interaction_type : phishy_interaction_data) {
    if (interaction_type.second.occurrence_count > 0) {
      // Create PhishySiteInteraction object and add to report.
      ClientSafeBrowsingReportRequest::PhishySiteInteraction
          new_phishy_site_interaction;
      new_phishy_site_interaction.set_phishy_site_interaction_type(
          interaction_type.first);
      new_phishy_site_interaction.set_occurrence_count(
          interaction_type.second.occurrence_count);
      new_phishy_site_interaction.set_first_interaction_timestamp_msec(
          interaction_type.second.first_timestamp);
      new_phishy_site_interaction.set_last_interaction_timestamp_msec(
          interaction_type.second.last_timestamp);
      report->mutable_phishy_site_interactions()->Add()->Swap(
          &new_phishy_site_interaction);
    }
  }
  auto* ping_manager = ChromePingManagerFactory::GetForBrowserContext(profile);
  DCHECK(ping_manager);
  return ping_manager->ReportThreatDetails(std::move(report)) ==
         PingManager::ReportThreatDetailsResult::SUCCESS;
}
#endif

bool SafeBrowsingServiceImpl::MaybeSendNotificationsAcceptedReport(
    content::RenderFrameHost* render_frame_host,
    Profile* profile,
    const GURL& url,
    const GURL& page_url,
    const GURL& permission_prompt_origin,
    base::TimeDelta permission_prompt_display_duration_sec) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!profile || !IsExtendedReportingEnabled(*profile->GetPrefs()) ||
      !base::FeatureList::IsEnabled(
          kCreateNotificationsAcceptedClientSafeBrowsingReports) ||
      profile->IsOffTheRecord()) {
    return false;
  }
  // Only send report if the UnsafeResource was allowlisted, due to the user
  // bypassing an interstitial. Note that we want to check the
  // permission_prompt_origin URL because if an interstitial was shown, then it
  // was warning the user about the URL where the permission request originated.
  if (!IsURLAllowlisted(permission_prompt_origin, render_frame_host)) {
    return false;
  }

  auto report = std::make_unique<ClientSafeBrowsingReportRequest>();
  report->set_type(
      ClientSafeBrowsingReportRequest::NOTIFICATION_PERMISSION_ACCEPTED);
  report->set_url(url.spec());
  report->set_page_url(page_url.spec());
  report->mutable_permission_prompt_info()->set_origin(
      permission_prompt_origin.spec());
  report->mutable_permission_prompt_info()->set_display_duration_sec(
      permission_prompt_display_duration_sec.InSeconds());
  FillReferrerChain(profile, render_frame_host,
                    report->mutable_referrer_chain());
  return ChromePingManagerFactory::GetForBrowserContext(profile)
             ->ReportThreatDetails(std::move(report)) ==
         PingManager::ReportThreatDetailsResult::SUCCESS;
}

void SafeBrowsingServiceImpl::CreateTriggerManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  trigger_manager_ = std::make_unique<TriggerManager>(
      ui_manager_.get(), g_browser_process->local_state());
}

network::mojom::NetworkContextParamsPtr
SafeBrowsingServiceImpl::CreateNetworkContextParams() {
  auto params = SystemNetworkContextManager::GetInstance()
                    ->CreateDefaultNetworkContextParams();
  // |proxy_config_monitor_| should be deleted after shutdown, so don't
  // re-create it.
  if (shutdown_) {
    return params;
  }
  if (!proxy_config_monitor_) {
    proxy_config_monitor_ =
        std::make_unique<ProxyConfigMonitor>(g_browser_process->local_state());
  }
  proxy_config_monitor_->AddToNetworkContextParams(params.get());
  return params;
}

void SafeBrowsingServiceImpl::RecordStartupCookieMetrics(Profile* profile) {
  // Exclude system profiles.
  if (!profile->IsRegularProfile() && !profile->IsIncognitoProfile()) {
    return;
  }
  network::mojom::NetworkContext* network_context = GetNetworkContext(profile);
  if (!network_context) {
    return;
  }
  auto cookie_manager_remote =
      std::make_unique<mojo::Remote<network::mojom::CookieManager>>();
  network_context->GetCookieManager(
      cookie_manager_remote->BindNewPipeAndPassReceiver());

  mojo::Remote<network::mojom::CookieManager>* cookie_manager_raw =
      cookie_manager_remote.get();
  (*cookie_manager_raw)
      ->GetAllCookies(
          base::BindOnce(&OnGotCookies, std::move(cookie_manager_remote)));
}

void SafeBrowsingServiceImpl::FillReferrerChain(
    Profile* profile,
    content::RenderFrameHost* render_frame_host,
    google::protobuf::RepeatedPtrField<ReferrerChainEntry>*
        out_referrer_chain) {
  ReferrerChainProvider* provider =
      GetReferrerChainProviderFromBrowserContext(profile);
  if (!provider) {
    return;
  }
  provider->IdentifyReferrerChainByRenderFrameHost(
      render_frame_host, kReferrerChainUserGestureLimit, out_referrer_chain);
}

bool SafeBrowsingServiceImpl::IsURLAllowlisted(
    const GURL& url,
    content::RenderFrameHost* primary_main_frame) {
  if (url_is_allowlisted_for_testing_) {
    return true;
  }

  security_interstitials::UnsafeResource resource;
  resource.url = url;
  resource.original_url = url;
  resource.threat_type = SBThreatType::SB_THREAT_TYPE_URL_PHISHING;
  const content::GlobalRenderFrameHostId primary_main_frame_id =
      primary_main_frame->GetGlobalId();
  resource.render_process_id = primary_main_frame_id.child_id;
  resource.render_frame_token = primary_main_frame->GetFrameToken().value();
  return ui_manager_->IsAllowlisted(resource);
}

// The default SafeBrowsingServiceFactory.  Global, made a singleton so we
// don't leak it.
class SafeBrowsingServiceFactoryImpl : public SafeBrowsingServiceFactory {
 public:
  // TODO(crbug.com/41437292): Once callers of this function are no longer
  // downcasting it to the SafeBrowsingService, we can make this a
  // scoped_refptr.
  SafeBrowsingServiceInterface* CreateSafeBrowsingService() override {
    return new SafeBrowsingService();
  }

  SafeBrowsingServiceFactoryImpl(const SafeBrowsingServiceFactoryImpl&) =
      delete;
  SafeBrowsingServiceFactoryImpl& operator=(
      const SafeBrowsingServiceFactoryImpl&) = delete;

 private:
  friend class base::NoDestructor<SafeBrowsingServiceFactoryImpl>;

  SafeBrowsingServiceFactoryImpl() {}
};

SafeBrowsingServiceFactory* GetSafeBrowsingServiceFactory() {
  static base::NoDestructor<SafeBrowsingServiceFactoryImpl> factory;
  return factory.get();
}

}  // namespace safe_browsing
