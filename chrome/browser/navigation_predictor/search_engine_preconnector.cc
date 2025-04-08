// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"

#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_features.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/predictors/predictors_traffic_annotations.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "net/base/features.h"

namespace {

#if BUILDFLAG(IS_ANDROID)
const int kDefaultStartupDelayMs = 0;
const bool kDefaultSkipInBackground = false;
#else
const int kDefaultStartupDelayMs = 5000;
const bool kDefaultSkipInBackground = true;
#endif

constexpr int kPreconnectIntervalSec = 60;
constexpr int kPreconnectRetryDelayMs = 50;

}  // namespace

namespace features {
// Feature to control preconnect to search.

BASE_FEATURE(kPreconnectFromKeyedService,
             "PreconnectFromKeyedService",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPreconnectToSearch,
             "PreconnectToSearch",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace features

WebContentVisibilityManager::WebContentVisibilityManager()
    : tick_clock_(base::DefaultTickClock::GetInstance()) {}

WebContentVisibilityManager::~WebContentVisibilityManager() = default;

void WebContentVisibilityManager::OnWebContentsVisibilityChanged(
    content::WebContents* web_contents,
    bool is_in_foreground) {
  visible_web_contents_.erase(web_contents);
  last_web_contents_state_change_time_ = tick_clock_->NowTicks();
  if (is_in_foreground) {
    visible_web_contents_.insert(web_contents);
  }
}

void WebContentVisibilityManager::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  visible_web_contents_.erase(web_contents);
  last_web_contents_state_change_time_ = tick_clock_->NowTicks();
}

bool WebContentVisibilityManager::IsBrowserAppLikelyInForeground() const {
  // If no web contents is in foreground, then allow a very short cool down
  // period before considering app in background. This cooldown period is
  // needed since when switching between the tabs, none of the web contents is
  // in foreground for a very short period.
  if (visible_web_contents_.empty() &&
      tick_clock_->NowTicks() - last_web_contents_state_change_time_ >
          base::Seconds(1)) {
    return false;
  }

  return tick_clock_->NowTicks() - last_web_contents_state_change_time_ <=
         base::Seconds(120);
}

void WebContentVisibilityManager::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

bool SearchEnginePreconnector::ShouldBeEnabledAsKeyedService() {
  static bool preconnect_from_keyed_service =
      base::FeatureList::IsEnabled(features::kPreconnectFromKeyedService);
  return preconnect_from_keyed_service;
}

bool SearchEnginePreconnector::ShouldBeEnabledForOffTheRecord() {
  static bool enabled_for_otr = base::GetFieldTrialParamByFeatureAsBool(
      features::kPreconnectFromKeyedService, "run_on_otr", false);
  return enabled_for_otr;
}

bool SearchEnginePreconnector::SearchEnginePreconnect2Enabled() {
  static bool preconnect2_enabled =
      base::FeatureList::IsEnabled(net::features::kSearchEnginePreconnect2);
  return preconnect2_enabled;
}

SearchEnginePreconnector::SearchEnginePreconnector(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK(ShouldBeEnabledForOffTheRecord() ||
         !browser_context_->IsOffTheRecord());
}

SearchEnginePreconnector::~SearchEnginePreconnector() = default;

void SearchEnginePreconnector::StopPreconnecting() {
  preconnector_started_ = false;
  timer_.Stop();
}

void SearchEnginePreconnector::StartPreconnecting(bool with_startup_delay) {
  preconnector_started_ = true;
  timer_.Stop();
  if (with_startup_delay) {
    StartPreconnectWithDelay(
        base::Milliseconds(base::GetFieldTrialParamByFeatureAsInt(
            features::kPreconnectToSearch, "startup_delay_ms",
            kDefaultStartupDelayMs)));
    return;
  }

  PreconnectDSE();
}

void SearchEnginePreconnector::PreconnectDSE() {
  DCHECK(ShouldBeEnabledForOffTheRecord() ||
         !browser_context_->IsOffTheRecord());
  DCHECK(!timer_.IsRunning());
  if (!base::FeatureList::IsEnabled(features::kPreconnectToSearch))
    return;

  // Don't preconnect unless the user allows search suggestions.
  if (!Profile::FromBrowserContext(browser_context_)
           ->GetPrefs()
           ->GetBoolean(prefs::kSearchSuggestEnabled))
    return;

  GURL preconnect_url = GetDefaultSearchEngineOriginURL();
  if (preconnect_url.scheme() != url::kHttpScheme &&
      preconnect_url.scheme() != url::kHttpsScheme) {
    return;
  }

  if (!preconnect_url.is_valid() || !preconnect_url.has_host()) {
    return;
  }

  if (!predictors::IsPreconnectAllowed(
          Profile::FromBrowserContext(browser_context_))) {
    return;
  }

  const bool is_browser_app_likely_in_foreground =
      IsBrowserAppLikelyInForeground();
  base::UmaHistogramBoolean(
      "NavigationPredictor.SearchEnginePreconnector."
      "IsBrowserAppLikelyInForeground",
      is_browser_app_likely_in_foreground);

  if (!base::GetFieldTrialParamByFeatureAsBool(features::kPreconnectToSearch,
                                               "skip_in_background",
                                               kDefaultSkipInBackground) ||
      is_browser_app_likely_in_foreground) {
    net::SchemefulSite schemeful_site(preconnect_url);
    auto network_anonymziation_key =
        net::NetworkAnonymizationKey::CreateSameSite(schemeful_site);
    GetPreconnectManager().StartPreconnectUrl(
        preconnect_url, /*allow_credentials=*/true, network_anonymziation_key,
        predictors::kSearchEnginePreconnectTrafficAnnotation,
        /*storage_partition_config=*/nullptr);
  }

  // Periodically preconnect to the DSE. If the browser app is likely in
  // background, we will reattempt preconnect later.
  if (!SearchEnginePreconnect2Enabled()) {
    StartPreconnectWithDelay(GetPreconnectInterval());
  }
}

GURL SearchEnginePreconnector::GetDefaultSearchEngineOriginURL() const {
  auto* template_service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
  if (!template_service)
    return GURL();
  const auto* search_provider = template_service->GetDefaultSearchProvider();
  if (!search_provider || !search_provider->data().preconnect_to_search_url)
    return GURL();
  return search_provider->GenerateSearchURL({}).DeprecatedGetOriginAsURL();
}

base::TimeDelta SearchEnginePreconnector::GetPreconnectInterval() const {
  if (!SearchEnginePreconnect2Enabled()) {
    int preconnect_interval = base::GetFieldTrialParamByFeatureAsInt(
        net::features::kSearchEnginePreconnectInterval, "preconnect_interval",
        kPreconnectIntervalSec);

    // Add an extra delay to make sure the preconnect has expired if it wasn't
    // used.
    return base::Seconds(preconnect_interval) +
           base::Milliseconds(kPreconnectRetryDelayMs);
  }

  // TODO(crbug.com/406022435): Update the logic to use exponential backoff.
  return base::Seconds(net::features::kMaxPreconnectRetryInterval.Get());
}

void SearchEnginePreconnector::StartPreconnectWithDelay(base::TimeDelta delay) {
  //  Set/Reset the timer to fire after the specified `delay`.
  timer_.Start(FROM_HERE, delay,
               base::BindOnce(&SearchEnginePreconnector::PreconnectDSE,
                              base::Unretained(this)));
}

predictors::PreconnectManager&
SearchEnginePreconnector::GetPreconnectManager() {
  if (!preconnect_manager_) {
    preconnect_manager_ = std::make_unique<predictors::PreconnectManager>(
        GetWeakPtr(), Profile::FromBrowserContext(browser_context_));
  }

  return *preconnect_manager_.get();
}

void SearchEnginePreconnector::OnWebContentsVisibilityChanged(
    content::WebContents* web_contents,
    bool is_in_foreground) {
  WebContentVisibilityManager::OnWebContentsVisibilityChanged(web_contents,
                                                              is_in_foreground);

  if (!SearchEnginePreconnect2Enabled()) {
    return;
  }

  // Early stop when we know that the visibility change did not trigger
  // foregrounding of the app and also when the preconnector is not started.
  if (!IsBrowserAppLikelyInForeground() || !preconnector_started_) {
    return;
  }

  // Stop the timer explicitly here so that we do not have any duplicate
  // attempts.
  timer_.Stop();

  // Attempt reconnect again in case the visibility has changed after the last
  // preconnect attempt so that we will preconnect sooner.
  PreconnectDSE();
}
