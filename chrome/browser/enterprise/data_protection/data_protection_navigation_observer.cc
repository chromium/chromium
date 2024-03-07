// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_navigation_observer.h"

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/data_protection/data_protection_page_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service.h"
#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service_factory.h"
#include "components/safe_browsing/core/browser/realtime/policy_engine.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"

namespace enterprise_data_protection {

namespace {

content::Page& GetPageFromWebContents(content::WebContents* web_contents) {
  return web_contents->GetPrimaryMainFrame()->GetPage();
}

void UpdateDataProtectionState(
    base::WeakPtr<content::WebContents> web_contents,
    DataProtectionNavigationObserver::Callback callback,
    const std::string& watermark_text) {
  if (!web_contents) {
    return;
  }

  DataProtectionPageUserData::UpdateDataProtectionState(
      GetPageFromWebContents(web_contents.get()), watermark_text);
  std::move(callback).Run(watermark_text);
}

bool SkipUrl(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) ||
         url.SchemeIs(extensions::kExtensionScheme);
}

base::Time TimestampToTime(safe_browsing::Timestamp timestamp) {
  return base::Time::UnixEpoch() + base::Seconds(timestamp.seconds()) +
         base::Nanoseconds(timestamp.nanos());
}

void OnRealTimeLookupComplete(
    DataProtectionNavigationObserver::Callback callback,
    bool is_success,
    bool is_cached,
    std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string watermark_text;
  if (is_success && rt_lookup_response &&
      rt_lookup_response->threat_info_size() > 0) {
    watermark_text = GetWatermarkString(rt_lookup_response->threat_info(0));
  }
  std::move(callback).Run(watermark_text);
}

void DoLookup(safe_browsing::RealTimeUrlLookupServiceBase* lookup_service,
              const GURL& url,
              DataProtectionNavigationObserver::Callback callback,
              content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  lookup_service->StartLookup(
      url, base::BindOnce(&OnRealTimeLookupComplete, std::move(callback)),
      base::SequencedTaskRunner::GetCurrentDefault(),
      sessions::SessionTabHelper::IdForTab(web_contents));
}

}  // namespace

std::string GetWatermarkString(
    const safe_browsing::RTLookupResponse::ThreatInfo& threat_info) {
  if (!threat_info.has_matched_url_navigation_rule()) {
    return std::string();
  }
  const safe_browsing::MatchedUrlNavigationRule& rule =
      threat_info.matched_url_navigation_rule();
  if (!rule.has_watermark_message()) {
    return std::string();
  }
  const safe_browsing::MatchedUrlNavigationRule::WatermarkMessage& watermark =
      rule.watermark_message();

  base::Time timestamp = TimestampToTime(watermark.timestamp());
  std::string watermark_text =
      (watermark.user_email().empty() ? watermark.obfuscated_device_id()
                                      : watermark.user_email()) +
      "\n" + base::TimeFormatAsIso8601(timestamp);
  if (!watermark.watermark_message().empty()) {
    watermark_text += "\n";
    watermark_text += watermark.watermark_message();
  }
  return watermark_text;
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
      profile->GetPrefs(), has_valid_dm_token, profile->IsOffTheRecord());
}

// static
void DataProtectionNavigationObserver::CreateForNavigationIfNeeded(
    Profile* profile,
    content::NavigationHandle* navigation_handle,
    Callback callback) {
  if (navigation_handle->IsSameDocument() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      SkipUrl(navigation_handle->GetURL()) ||
      !IsEnterpriseLookupEnabled(profile)) {
    return;
  }

  // GetForProfile() return nullptr if enterprise policies are not set.
  auto* lookup_service = safe_browsing::
      ChromeEnterpriseRealTimeUrlLookupServiceFactory::GetForProfile(profile);
  if (lookup_service) {
    enterprise_data_protection::DataProtectionNavigationObserver::
        CreateForNavigationHandle(*navigation_handle, lookup_service,
                                  navigation_handle->GetWebContents(),
                                  std::move(callback));
  }
}

// static
void DataProtectionNavigationObserver::GetDataProtectionSettings(
    Profile* profile,
    content::WebContents* web_contents,
    Callback callback) {
  auto* ud = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents));
  if (ud) {
    std::move(callback).Run(ud->watermark_text());
    return;
  }

  if (!IsEnterpriseLookupEnabled(profile)) {
    return;
  }

  auto* lookup_service = safe_browsing::
      ChromeEnterpriseRealTimeUrlLookupServiceFactory::GetForProfile(profile);
  if (lookup_service && web_contents->GetLastCommittedURL().is_valid()) {
    DoLookup(lookup_service, web_contents->GetLastCommittedURL(),
             std::move(callback), web_contents);
  } else {
    std::move(callback).Run(std::string());
  }
}

DataProtectionNavigationObserver::DataProtectionNavigationObserver(
    content::NavigationHandle& navigation_handle,
    safe_browsing::RealTimeUrlLookupServiceBase* lookup_service,
    content::WebContents* web_contents,
    Callback callback)
    : content::WebContentsObserver(web_contents),
      lookup_service_(lookup_service),
      pending_navigation_callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!pending_navigation_callback_.is_null());
  DCHECK(lookup_service_);

  // When serving from cache, we expect to find a page user data. So this code
  // skips the call to DoLookup() to prevent an unneeded network request.
  // This check is speculative however, although a good heuristic, because we'll
  // only know if a page user data exists once DidFinishNavigation() is called.
  // We can't check for the page user data here because the page of the primary
  // main frame still points to the existing page before the navigation, not the
  // ultimate destination page of the navigation.
  is_from_cache_ = navigation_handle.IsServedFromBackForwardCache();
  if (!is_from_cache_) {
    DoLookup(lookup_service_, navigation_handle.GetURL(),
             base::BindOnce(&DataProtectionNavigationObserver::OnLookupComplete,
                            weak_factory_.GetWeakPtr()),
             navigation_handle.GetWebContents());
  }
}

DataProtectionNavigationObserver::~DataProtectionNavigationObserver() = default;

void DataProtectionNavigationObserver::OnLookupComplete(
    const std::string& watermark_text) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!is_from_cache_);
  watermark_text_ = watermark_text;
}

void DataProtectionNavigationObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!is_from_cache_);
  DoLookup(lookup_service_, navigation_handle->GetURL(),
           base::BindOnce(&DataProtectionNavigationObserver::OnLookupComplete,
                          weak_factory_.GetWeakPtr()),
           navigation_handle->GetWebContents());
}

void DataProtectionNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // If the page already has cached data protection information, use that first.
  // Otherwise if `watermark_text_` has been set then use the specified value.
  // Finally, ask the the lookup service right now for a lookup.
  //
  // The third case could imply a delay between finishing the navigation and
  // setting the screenshot state correctly.  This should only happen when
  // the navigation happens from the bfcache, the page itself is located in
  // the browser's cache, and the lookup service's cache TTL has expired.
  // Will need to see if in practice this is a problem.
  auto* ud = DataProtectionPageUserData::GetForPage(
      GetPageFromWebContents(web_contents()));
  if (ud) {
    UpdateDataProtectionState(web_contents()->GetWeakPtr(),
                              std::move(pending_navigation_callback_),
                              ud->watermark_text());
  } else if (watermark_text_.has_value()) {
    UpdateDataProtectionState(web_contents()->GetWeakPtr(),
                              std::move(pending_navigation_callback_),
                              watermark_text_.value());
  } else {
    DoLookup(
        lookup_service_, navigation_handle->GetURL(),
        base::BindOnce(&UpdateDataProtectionState, web_contents()->GetWeakPtr(),
                       std::move(pending_navigation_callback_)),
        web_contents());
  }
}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(DataProtectionNavigationObserver);

}  // namespace enterprise_data_protection
