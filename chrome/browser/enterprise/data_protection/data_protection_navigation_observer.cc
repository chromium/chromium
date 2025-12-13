// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_navigation_observer.h"

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/data_controls/chrome_rules_service.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/browser/enterprise/data_protection/data_protection_url_lookup_service.h"
#include "chrome/browser/interstitials/enterprise_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/realtime/chrome_enterprise_url_lookup_service.h"
#include "components/safe_browsing/core/browser/realtime/policy_engine.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_private_event_router.h"
#endif

namespace enterprise_data_protection {

namespace {

constexpr char kURLVerdictSourceHistogram[] =
    "Enterprise.DataProtection.URLVerdictSource";

// This is non-null in tests to install a fake service.
safe_browsing::RealTimeUrlLookupServiceBase* g_lookup_service = nullptr;

bool IsWatermarkWebUIURL(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host() == chrome::kChromeUIWatermarkHost;
}

content::Page& GetPageFromWebContents(content::WebContents* web_contents) {
  return web_contents->GetPrimaryMainFrame()->GetPage();
}

DataProtectionPageUserData* GetUserData(content::WebContents* web_contents) {
  return DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents));
}

// Returns whether a URL filtering event should be reported for safe verdicts.
// For warn/block+watermark verdicts, a security event is reported as part
// of the interstitial page appearing, so we only need to report in this class
// for SAFE verdicts where no interstitial was shown, only if a rule was
// triggered.
bool ShouldReportSafeUrlFilteringEvents(DataProtectionPageUserData* user_data) {
  DCHECK(user_data);
  return user_data->rt_lookup_response() &&
         !user_data->rt_lookup_response()->threat_info().empty() &&
         user_data->rt_lookup_response()->threat_info(0).verdict_type() ==
             safe_browsing::RTLookupResponse::ThreatInfo::SAFE &&
         user_data->rt_lookup_response()
             ->threat_info(0)
             .has_matched_url_navigation_rule();
}

void RunPendingNavigationCallback(
    content::WebContents* web_contents,
    DataProtectionNavigationObserver::Callback callback) {
  DCHECK(web_contents);

  auto* user_data = GetUserData(web_contents);
  DCHECK(user_data);

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  if (ShouldReportSafeUrlFilteringEvents(user_data)) {
    MaybeTriggerUrlFilteringInterstitialEvent(
        web_contents, web_contents->GetLastCommittedURL(),
        /*threat_type=*/"", *user_data->rt_lookup_response());
  }
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto* router =
      extensions::EnterpriseReportingPrivateEventRouterFactory::GetInstance()
          ->GetForProfile(web_contents->GetBrowserContext());
  if (user_data->rt_lookup_response() && router) {
    router->OnUrlFilteringVerdict(web_contents->GetLastCommittedURL(),
                                  *user_data->rt_lookup_response());
  }
#endif

  std::move(callback).Run(user_data->settings());
}

void OnDoLookupComplete(
    base::WeakPtr<content::WebContents> web_contents,
    DataProtectionNavigationObserver::Callback callback,
    const std::string& identifier,
    std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response) {
  if (!web_contents) {
    return;
  }

  // TODO: This function runs after data protections that come from data
  // controls have already been saved in page user data.  This means RT UTL
  // lookup results will override data controls when the protections they
  // control overlap.  Is that right?
  DataProtectionPageUserData::UpdateRTLookupResponse(
      GetPageFromWebContents(web_contents.get()), identifier,
      std::move(rt_lookup_response));
  RunPendingNavigationCallback(web_contents.get(), std::move(callback));
}

bool SkipUrl(const GURL& url) {
  return !url.is_valid() || url.SchemeIs(content::kChromeUIScheme) ||
         url.SchemeIs(extensions::kExtensionScheme);
}

bool IsEnterpriseLookupEnabled(Profile* profile) {
  // Some tests return a non-null pointer for the enterprise lookup service,
  // so we need to defensively check if enterprise lookup is enabled.
  auto* connectors_service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile);
  bool has_valid_dm_token =
      connectors_service &&
      connectors_service->GetDMTokenForRealTimeUrlCheck().has_value();
  return safe_browsing::RealTimePolicyEngine::CanPerformEnterpriseFullURLLookup(
      profile->GetPrefs(), has_valid_dm_token, profile->IsOffTheRecord(),
      profile->IsGuestSession());
}

bool IsEnterpriseLookupEnabled(content::BrowserContext* context) {
  DCHECK(context);
  return IsEnterpriseLookupEnabled(Profile::FromBrowserContext(context));
}

void DoLookup(safe_browsing::RealTimeUrlLookupServiceBase* lookup_service,
              const GURL& url,
              const std::string& identifier,
              LookupCallback callback,
              content::WebContents* web_contents) {
  DCHECK(IsEnterpriseLookupEnabled(web_contents->GetBrowserContext()));

  auto* url_lookup_service =
      DataProtectionUrlLookupServiceFactory::GetInstance()
          ->GetForBrowserContext(web_contents->GetBrowserContext());

  if (!url_lookup_service) {
    return;
  }

  url_lookup_service->DoLookup(lookup_service, url, identifier,
                               std::move(callback), web_contents);
}

std::string GetIdentifier(content::BrowserContext* browser_context) {
  return enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
             browser_context)
      ->GetRealTimeUrlCheckIdentifier();
}

void LogVerdictSource(
    DataProtectionNavigationObserver::URLVerdictSource verdict_source) {
  VLOG(1) << "enterprise.watermark: verdict source: "
          << static_cast<int>(verdict_source);
  base::UmaHistogramEnumeration(kURLVerdictSourceHistogram, verdict_source);
}

bool IsScreenshotAllowedByDataControls(content::BrowserContext* context,
                                       const GURL& url) {
  auto* rules = data_controls::ChromeRulesServiceFactory::GetInstance()
                    ->GetForBrowserContext(context);
  return rules ? !rules->BlockScreenshots(url) : true;
}

}  // namespace

// static
std::unique_ptr<DataProtectionNavigationObserver>
DataProtectionNavigationObserver::CreateForNavigationIfNeeded(
    DataProtectionNavigationDelegate* delegate,
    Profile* profile,
    content::NavigationHandle* navigation_handle,
    Callback callback) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      (!base::FeatureList::IsEnabled(kEnableSinglePageAppDataProtection) &&
       navigation_handle->IsSameDocument())) {
    return nullptr;
  }

  if (IsWatermarkWebUIURL(navigation_handle->GetURL())) {
    UrlSettings settings;
    // TODO(crbug.com/434714853): Replace with i18n string
    settings.watermark_text = "Watermark Test Page";
    std::move(callback).Run(settings);
    return nullptr;
  }

  VLOG(1) << "enterprise.data_protection: same document navigation: "
          << navigation_handle->IsSameDocument();
  VLOG(1) << "enterprise.data_protection: URL to scan: "
          << navigation_handle->GetURL();

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  // The Data protection settings need to be cleared if:
  // 1. This is a skipped URL. This is needed to handle for example navigating
  // from a watermarked page to the NTP.
  // 2. Data protection is disabled. This is needed to prevent stale data
  // protection settings if the enabled state is changed mid session.
  if (SkipUrl(navigation_handle->GetURL())) {
    std::move(callback).Run(UrlSettings::None());
    return nullptr;
  }

  // ChromeEnterpriseRealTimeUrlLookupServiceFactory::GetForProfile() return
  // nullptr if enterprise policies are not set.  In this case data protections
  // will be based on data controls alone,
  return std::make_unique<
      enterprise_data_protection::DataProtectionNavigationObserver>(
      *navigation_handle,
      safe_browsing::ChromeEnterpriseRealTimeUrlLookupServiceFactory::
          GetForProfile(profile),
      navigation_handle->GetWebContents(), delegate, std::move(callback));
#else
  std::move(callback).Run(UrlSettings::None());
  return nullptr;
#endif
}

// static
void DataProtectionNavigationObserver::ApplyDataProtectionSettings(
    Profile* profile,
    content::WebContents* web_contents,
    Callback callback) {
  if (IsWatermarkWebUIURL(web_contents->GetLastCommittedURL())) {
    UrlSettings settings;
    // TODO(crbug.com/434714853): Replace with i18n string
    settings.watermark_text = "Watermark Test Page";
    std::move(callback).Run(settings);
    return;
  }
  auto* ud = GetUserData(web_contents);
  if (ud) {
    std::move(callback).Run(ud->settings());
    return;
  }

  // If this is a skipped URL, force the view to clear any data protections if
  // present.  This is needed to handle for example navigating from a
  // protected page to the NTP.
  if (SkipUrl(web_contents->GetLastCommittedURL())) {
    std::move(callback).Run(UrlSettings::None());
    return;
  }

  std::string identifier = GetIdentifier(profile);

  DataProtectionPageUserData::UpdateDataControlsScreenshotState(
      GetPageFromWebContents(web_contents), identifier,
      IsScreenshotAllowedByDataControls(profile,
                                        web_contents->GetLastCommittedURL()));

  auto* lookup_service =
      g_lookup_service
          ? g_lookup_service
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
          : safe_browsing::ChromeEnterpriseRealTimeUrlLookupServiceFactory::
                GetForProfile(profile);
#else
          : nullptr;
#endif
  if (lookup_service && IsEnterpriseLookupEnabled(profile)) {
    auto lookup_callback = base::BindOnce(
        [](const std::string& identifier,
           DataProtectionNavigationObserver::Callback callback,
           base::WeakPtr<content::WebContents> web_contents,
           std::unique_ptr<safe_browsing::RTLookupResponse> response) {
          if (web_contents) {
            DataProtectionPageUserData::UpdateRTLookupResponse(
                GetPageFromWebContents(web_contents.get()), identifier,
                std::move(response));
            auto* user_data = GetUserData(web_contents.get());
            DCHECK(user_data);
            std::move(callback).Run(user_data->settings());
          }
        },
        std::move(identifier), std::move(callback), web_contents->GetWeakPtr());

    DoLookup(lookup_service, web_contents->GetLastCommittedURL(),
             GetIdentifier(profile), std::move(lookup_callback), web_contents);
  } else {
    ud = GetUserData(web_contents);
    DCHECK(ud);
    std::move(callback).Run(ud->settings());
  }
}

// static
void DataProtectionNavigationObserver::SetLookupServiceForTesting(
    safe_browsing::RealTimeUrlLookupServiceBase* lookup_service) {
  g_lookup_service = lookup_service;
}

DataProtectionNavigationObserver::DataProtectionNavigationObserver(
    content::NavigationHandle& navigation_handle,
    safe_browsing::RealTimeUrlLookupServiceBase* lookup_service,
    content::WebContents* web_contents,
    DataProtectionNavigationDelegate* delegate,
    Callback callback)
    : content::WebContentsObserver(web_contents),
      navigation_id_(navigation_handle.GetNavigationId()),
      lookup_service_(lookup_service),
      delegate_(delegate),
      pending_navigation_callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!pending_navigation_callback_.is_null());

  identifier_ = GetIdentifier(web_contents->GetBrowserContext());
  allow_screenshot_ = IsScreenshotAllowedByDataControls(
      web_contents->GetBrowserContext(), navigation_handle.GetURL());

  // When serving from cache, we expect to find a page user data. So this code
  // skips the call to DoLookup() to prevent an unneeded network request.
  // This check is speculative however, although a good heuristic, because we'll
  // only know if a page user data exists once DidFinishNavigation() is called.
  // We can't check for the page user data here because the page of the primary
  // main frame still points to the existing page before the navigation, not the
  // ultimate destination page of the navigation.
  is_from_cache_ = navigation_handle.IsServedFromBackForwardCache();
  if (!is_from_cache_ &&
      ShouldPerformRealTimeUrlCheck(web_contents->GetBrowserContext())) {
    DoLookup(lookup_service_, navigation_handle.GetURL(), identifier_,
             base::BindOnce(&DataProtectionNavigationObserver::OnLookupComplete,
                            weak_factory_.GetWeakPtr()),
             navigation_handle.GetWebContents());
  } else {
    is_verdict_received_ = true;
  }
}

DataProtectionNavigationObserver::~DataProtectionNavigationObserver() = default;

void DataProtectionNavigationObserver::OnLookupComplete(
    std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!is_from_cache_);
  base::ScopedClosureRunner done(
      base::BindOnce(&DataProtectionNavigationObserver::MaybeCleanup,
                     weak_factory_.GetWeakPtr()));
  is_verdict_received_ = true;
  if (!web_contents()) {
    return;
  }
  if (is_navigation_finished_) {
    OnDoLookupComplete(web_contents()->GetWeakPtr(),
                       std::move(pending_navigation_callback_), identifier_,
                       std::move(rt_lookup_response));
  } else {
    rt_lookup_response_ = std::move(rt_lookup_response);
  }
}

bool DataProtectionNavigationObserver::ShouldPerformRealTimeUrlCheck(
    content::BrowserContext* browser_context) const {
  return lookup_service_ && IsEnterpriseLookupEnabled(browser_context);
}

void DataProtectionNavigationObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!is_from_cache_);

  allow_screenshot_ = allow_screenshot_ && IsScreenshotAllowedByDataControls(
      navigation_handle->GetWebContents()->GetBrowserContext(),
      navigation_handle->GetURL());

  if (ShouldPerformRealTimeUrlCheck(
          navigation_handle->GetWebContents()->GetBrowserContext())) {
    DoLookup(
        lookup_service_, navigation_handle->GetURL(),
        GetIdentifier(navigation_handle->GetWebContents()->GetBrowserContext()),
        base::BindOnce(&DataProtectionNavigationObserver::OnLookupComplete,
                       weak_factory_.GetWeakPtr()),
        navigation_handle->GetWebContents());
  }
}

void DataProtectionNavigationObserver::MaybeCleanup() {
  if (is_navigation_finished_ && is_verdict_received_) {
    DCHECK(delegate_);
    delegate_->Cleanup(navigation_id_);
  }
}

void DataProtectionNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  is_navigation_finished_ = true;
  base::ScopedClosureRunner done(
      base::BindOnce(&DataProtectionNavigationObserver::MaybeCleanup,
                     weak_factory_.GetWeakPtr()));

  // Only consider primary main frame commits, which will come eventually.
  // Even though some of these checks where already performed in
  // CreateForNavigationIfNeeded(), they still need to checked again here
  // to handle pages with iframes.
  //
  // `pending_navigation_callback_` being null implies `DidFinishNavigation`
  // has already been called, so further lookups/metrics code need to run.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() || !pending_navigation_callback_ ||
      !is_verdict_received_) {
    return;
  }

  // If the page already has cached data protection information, use that first.
  // Otherwise if `rt_lookup_response_` has been set then use the specified
  // value. Finally, ask the the lookup service right now for a lookup.
  //
  // The third case could imply a delay between finishing the navigation and
  // setting the screenshot state correctly.  This should only happen when
  // the navigation happens from the bfcache, the page itself is located in
  // the browser's cache, and the lookup service's cache TTL has expired.
  // Will need to see if in practice this is a problem.
  auto* ud = GetUserData(web_contents());
  if (ud) {
    LogVerdictSource(URLVerdictSource::kPageUserData);
    RunPendingNavigationCallback(web_contents(),
                                 std::move(pending_navigation_callback_));
    return;
  }

  DataProtectionPageUserData::UpdateDataControlsScreenshotState(
      GetPageFromWebContents(navigation_handle->GetWebContents()), identifier_,
      allow_screenshot_);

  if (rt_lookup_response_.get()) {
    LogVerdictSource(URLVerdictSource::kCachedLookupResult);
    OnDoLookupComplete(web_contents()->GetWeakPtr(),
                       std::move(pending_navigation_callback_), identifier_,
                       std::move(rt_lookup_response_));
  } else if (ShouldPerformRealTimeUrlCheck(
                 web_contents()->GetBrowserContext())) {
    LogVerdictSource(URLVerdictSource::kPostNavigationLookup);
    DoLookup(
        lookup_service_, navigation_handle->GetURL(), identifier_,
        base::BindOnce(&OnDoLookupComplete, web_contents()->GetWeakPtr(),
                       std::move(pending_navigation_callback_), identifier_),
        web_contents());
  } else if (web_contents()) {
    RunPendingNavigationCallback(web_contents(),
                                 std::move(pending_navigation_callback_));
  }

  DCHECK(pending_navigation_callback_.is_null());
}

// static
size_t DataProtectionNavigationObserver::GetVerdictCacheMaxSize() {
  size_t max_value = enterprise_data_protection::kVerdictCacheMaxSize.Get();

  // Defensive check to ensure a valid size for the verdict cache.
  return max_value > 0
             ? max_value
             : enterprise_data_protection::kVerdictCacheMaxSize.default_value;
}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(DataProtectionNavigationObserver);

}  // namespace enterprise_data_protection
