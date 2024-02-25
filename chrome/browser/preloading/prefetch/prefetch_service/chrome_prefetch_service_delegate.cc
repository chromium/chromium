// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_service/chrome_prefetch_service_delegate.h"

#include "chrome/browser/battery/battery_saver.h"
#include "chrome/browser/data_saver/data_saver.h"
#include "chrome/browser/preloading/prefetch/prefetch_service/prefetch_origin_decider.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/google/core/common/google_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/google_api_keys.h"
#include "net/http/http_util.h"
#include "url/gurl.h"

ChromePrefetchServiceDelegate::ChromePrefetchServiceDelegate(
    content::BrowserContext* browser_context)
    : profile_(Profile::FromBrowserContext(browser_context)),
      origin_decider_(
          std::make_unique<PrefetchOriginDecider>(profile_->GetPrefs())) {}

ChromePrefetchServiceDelegate::~ChromePrefetchServiceDelegate() = default;

std::string ChromePrefetchServiceDelegate::GetMajorVersionNumber() {
  return version_info::GetMajorVersionNumber();
}

std::string ChromePrefetchServiceDelegate::GetAcceptLanguageHeader() {
  return net::HttpUtil::GenerateAcceptLanguageHeader(
      profile_->GetPrefs()->GetString(language::prefs::kAcceptLanguages));
}

GURL ChromePrefetchServiceDelegate::GetDefaultPrefetchProxyHost() {
  return GURL("https://tunnel.googlezip.net/");
}

std::string ChromePrefetchServiceDelegate::GetAPIKey() {
  return google_apis::GetAPIKey();
}

GURL ChromePrefetchServiceDelegate::GetDefaultDNSCanaryCheckURL() {
  return GURL("http://dns-tunnel-check.googlezip.net/connect");
}

GURL ChromePrefetchServiceDelegate::GetDefaultTLSCanaryCheckURL() {
  return GURL("http://tls-tunnel-check.googlezip.net/connect");
}

void ChromePrefetchServiceDelegate::ReportOriginRetryAfter(
    const GURL& url,
    base::TimeDelta retry_after) {
  return origin_decider_->ReportOriginRetryAfter(url, retry_after);
}

bool ChromePrefetchServiceDelegate::IsOriginOutsideRetryAfterWindow(
    const GURL& url) {
  return origin_decider_->IsOriginOutsideRetryAfterWindow(url);
}

void ChromePrefetchServiceDelegate::ClearData() {
  origin_decider_->OnBrowsingDataCleared();
}

bool ChromePrefetchServiceDelegate::DisableDecoysBasedOnUserSettings() {
  // If the user has opted-in to Make Search and Browsing Better, then there is
  // no need to send decoy requests.
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper> helper =
      unified_consent::UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(profile_->GetPrefs());
  return helper->IsEnabled();
}

content::PreloadingEligibility
ChromePrefetchServiceDelegate::IsSomePreloadingEnabled() {
  return prefetch::IsSomePreloadingEnabled(*profile_->GetPrefs());
}

bool ChromePrefetchServiceDelegate::IsPreloadingPrefEnabled() {
  return prefetch::IsSomePreloadingEnabled(*profile_->GetPrefs()) !=
         content::PreloadingEligibility::kPreloadingDisabled;
}

bool ChromePrefetchServiceDelegate::IsDataSaverEnabled() {
  return data_saver::IsDataSaverEnabled();
}

bool ChromePrefetchServiceDelegate::IsBatterySaverEnabled() {
  return battery::IsBatterySaverEnabled();
}

bool ChromePrefetchServiceDelegate::IsExtendedPreloadingEnabled() {
  return prefetch::GetPreloadPagesState(*profile_->GetPrefs()) ==
         prefetch::PreloadPagesState::kExtendedPreloading;
}

bool ChromePrefetchServiceDelegate::IsDomainInPrefetchAllowList(
    const GURL& referring_url) {
  return IsGoogleDomainUrl(referring_url, google_util::ALLOW_SUBDOMAIN,
                           google_util::ALLOW_NON_STANDARD_PORTS) ||
         IsYoutubeDomainUrl(referring_url, google_util::ALLOW_SUBDOMAIN,
                            google_util::ALLOW_NON_STANDARD_PORTS);
}

bool ChromePrefetchServiceDelegate::IsContaminationExempt(
    const GURL& referring_url) {
  // The default search engine has been chosen by the user and its cross-site
  // navigations have a significant performance impact.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  return template_url_service &&
         template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
             referring_url);
}

void ChromePrefetchServiceDelegate::OnPrefetchLikely(
    content::WebContents* web_contents) {
  page_load_metrics::MetricsWebContentsObserver* metrics_web_contents_observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents);
  if (!metrics_web_contents_observer) {
    return;
  }

  metrics_web_contents_observer->OnPrefetchLikely();
}
