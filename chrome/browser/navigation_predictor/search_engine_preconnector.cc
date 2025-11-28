// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"

#include <limits>

#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/battery/battery_saver.h"
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
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "net/base/reconnect_notifier.h"

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
constexpr int kPreconnectIntervalForLowPowerSec = 30;

bool RebindPreconnectReceiversEnabled() {
  return base::FeatureList::IsEnabled(features::kRebindPreconnectReceivers);
}

bool OnlyBindOnConnectionClosedOrFailed() {
  return RebindPreconnectReceiversEnabled() &&
         features::kRebindReceiverEvent.Get() ==
             features::RebindReceiverEvent::kOnlyOnConnectionClosedOrFailed;
}

}  // namespace

namespace features {
// Feature to control preconnect to search.

BASE_FEATURE(kPreconnectFromKeyedService, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPreconnectToSearch, base::FEATURE_ENABLED_BY_DEFAULT);

// Feature to control binding the receivers every time we attempt to
// preconnect. This will trigger the destruction of the previous receiver
// (and also the remote), and also the re-creation of the observer.
BASE_FEATURE(kRebindPreconnectReceivers, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<RebindReceiverEvent>::Option
    kRebindReceiverEventOptions[] = {
        {RebindReceiverEvent::kEverytime, "kEverytime"},
        {RebindReceiverEvent::kOnlyOnConnectionClosedOrFailed,
         "kOnlyOnConnectionClosedOrFailed"}};

BASE_FEATURE_ENUM_PARAM(RebindReceiverEvent,
                        kRebindReceiverEvent,
                        &kRebindPreconnectReceivers,
                        "kRebindReceiverEvent",
                        features::RebindReceiverEvent::kEverytime,
                        &kRebindReceiverEventOptions);
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
      features::kPreconnectFromKeyedService, "run_on_otr", true);
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
            kDefaultStartupDelayMs)),
        PreconnectTriggerEvent::kInitialPreconnect);
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
  if (preconnect_url.GetScheme() != url::kHttpScheme &&
      preconnect_url.GetScheme() != url::kHttpsScheme) {
    return;
  }

  if (!preconnect_url.is_valid() || !preconnect_url.has_host()) {
    return;
  }

  if (!IsPreconnectEnabled()) {
    return;
  }

  const bool is_browser_app_likely_in_foreground =
      IsBrowserAppLikelyInForeground();
  base::UmaHistogramBoolean(
      "NavigationPredictor.SearchEnginePreconnector."
      "IsBrowserAppLikelyInForeground",
      is_browser_app_likely_in_foreground);

  std::optional<net::ConnectionKeepAliveConfig> keepalive_config;
  mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient> observer;
  if (SearchEnginePreconnect2Enabled()) {
    keepalive_config = GetConnectionKeepAliveConfig();

    if (RebindPreconnectReceiversEnabled() &&
        !OnlyBindOnConnectionClosedOrFailed()) {
      ResetReceiver();
    }

    if (!receiver_.is_bound()) {
      observer = receiver_.BindNewPipeAndPassRemote();
      receiver_.set_disconnect_handler(base::BindOnce(
          &SearchEnginePreconnector::OnReconnectObserverPipeDisconnected,
          base::Unretained(this)));
    }
  }

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
        /*storage_partition_config=*/nullptr, std::move(keepalive_config),
        std::move(observer));
  }

  // Periodically preconnect to the DSE. If the browser app is likely in
  // background, we will reattempt preconnect later.
  if (!SearchEnginePreconnect2Enabled()) {
    StartPreconnectWithDelay(GetPreconnectInterval(),
                             PreconnectTriggerEvent::kPeriodicPreconnect);
  }

  last_preconnect_attempt_time_ = base::TimeTicks::Now();
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
  // If the feature is not enabled or the device is in low power mode, we will
  // use the old preconnect interval.
  if (!SearchEnginePreconnect2Enabled()) {
    int preconnect_interval = base::GetFieldTrialParamByFeatureAsInt(
        net::features::kSearchEnginePreconnectInterval, "preconnect_interval",
        kPreconnectIntervalSec);

    // Add an extra delay to make sure the preconnect has expired if it wasn't
    // used.
    return base::Seconds(preconnect_interval) +
           base::Milliseconds(kPreconnectRetryDelayMs);
  }

  // If the device is in low power mode, we will use a longer preconnect
  // interval. We do not add an extra delay which is added for the old
  // preconnect interval since the connection keepalive will handle the
  // expiration, and we already know that the connection is closed.
  if (ShouldSavePower()) {
    return base::Seconds(kPreconnectIntervalForLowPowerSec);
  }

  // If this is the first time failing, we should instantly retry, but we wait
  // a very small amount of time since a closed connection would likely mean
  // that there were something wrong in the connection.
  // Otherwise, we backoff `kPreconnectRetryDelayMs` (currently 50 ms for normal
  // mode) * 2^n for the next preconnect attempt.
  return std::min(
      base::Milliseconds(kPreconnectRetryDelayMs) *
          CalculateBackoffMultiplier(),
      base::Seconds(net::features::kMaxPreconnectRetryInterval.Get()));
}

int32_t SearchEnginePreconnector::CalculateBackoffMultiplier() const {
  return 1 << std::min(static_cast<int>(consecutive_connection_failure_),
                       std::numeric_limits<int32_t>::digits - 1);
}

bool SearchEnginePreconnector::IsShortSession() const {
  CHECK(last_preconnect_attempt_time_.has_value());
  if (is_short_session_for_testing_.has_value()) {
    return is_short_session_for_testing_.value();
  }

  base::TimeDelta session_time =
      base::TimeTicks::Now() - last_preconnect_attempt_time_.value();

  // If the current session duration is shorter than the idle timeout, we
  // consider the session to be short.
  return session_time < net::features::kShortSessionThreshold.Get();
}

bool SearchEnginePreconnector::ShouldSavePower() const {
  return net::features::kFallbackInLowPowerMode.Get() &&
         battery::IsBatterySaverEnabled();
}

void SearchEnginePreconnector::StartPreconnectWithDelay(
    base::TimeDelta delay,
    PreconnectTriggerEvent event) {
  RecordPreconnectAttemptHistogram(delay, event);
  //  Set/Reset the timer to fire after the specified `delay`.
  timer_.Start(FROM_HERE, delay,
               base::BindOnce(&SearchEnginePreconnector::PreconnectDSE,
                              base::Unretained(this)));
}

content::PreconnectManager& SearchEnginePreconnector::GetPreconnectManager() {
  if (!preconnect_manager_) {
    preconnect_manager_ = content::PreconnectManager::Create(
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

bool SearchEnginePreconnector::IsPreconnectEnabled() {
  return predictors::IsPreconnectAllowed(
      Profile::FromBrowserContext(browser_context_));
}

void SearchEnginePreconnector::OnSessionClosed() {
  if (IsShortSession()) {
    // If we have a short session, we consider that the session was closed due
    // to an error, and will consider as a failed connection as well.
    consecutive_connection_failure_++;
  } else {
    // If the last session was not short, then it must mean that the connection
    // was successful. Reset the failure count.
    base::UmaHistogramCounts1000(
        "NavigationPredictor.SearchEnginePreconnector.ConsecutiveFailures",
        consecutive_connection_failure_);
    consecutive_connection_failure_ = 0;
  }
  if (RebindPreconnectReceiversEnabled() &&
      OnlyBindOnConnectionClosedOrFailed()) {
    ResetReceiver();
  }
  StartPreconnectWithDelay(GetPreconnectInterval(),
                           PreconnectTriggerEvent::kSessionClosed);
}

void SearchEnginePreconnector::OnNetworkEvent(net::NetworkChangeEvent event) {
  // If the network event is `Connected`, we attempt preconnect. Otherwise,
  // we will ignore the events for now.
  if (event == net::NetworkChangeEvent::kConnected) {
    // We do not call `ResetReceiver` here since the network change event might
    // not actually close a connection. It is OK to not reset the receiver here
    // since network event will be followed by `OnSessionClosed` or
    // `OnConnectionFailed` anyway when the connection is actually closed.
    StartPreconnectWithDelay(base::Milliseconds(kPreconnectRetryDelayMs),
                             PreconnectTriggerEvent::kNetworkEvent);
  }
}

void SearchEnginePreconnector::OnConnectionFailed() {
  consecutive_connection_failure_++;
  if (RebindPreconnectReceiversEnabled() &&
      OnlyBindOnConnectionClosedOrFailed()) {
    ResetReceiver();
  }
  StartPreconnectWithDelay(GetPreconnectInterval(),
                           PreconnectTriggerEvent::kConnectionFailed);
}

void SearchEnginePreconnector::OnReconnectObserverPipeDisconnected() {
  receiver_.reset();
  // Only call `OnConnectionFailed` when the `timer_` is not running since we
  // might already be waiting for reconnect attempt from other reasons.
  if (!timer_.IsRunning()) {
    OnConnectionFailed();
  }
}

void SearchEnginePreconnector::ResetReceiver() {
  if (!receiver_.is_bound()) {
    return;
  }

  // We clear the disconnect handler to avoid calling
  // `OnReconnectObserverPipeDisconnected` when the pipe is intentionally
  // reset, so that we will not start a new preconnect attempt.
  receiver_.set_disconnect_handler(base::DoNothing());
  receiver_.reset();
}

void SearchEnginePreconnector::RecordPreconnectAttemptHistogram(
    base::TimeDelta delay,
    PreconnectTriggerEvent event) {
  base::UmaHistogramEnumeration(
      "NavigationPredictor.SearchEnginePreconnector.TriggerEvent", event);
  base::UmaHistogramLongTimes(
      "NavigationPredictor.SearchEnginePreconnector.PreconnectDelay", delay);
  if (last_preconnect_attempt_time_.has_value()) {
    base::UmaHistogramLongTimes(
        "NavigationPredictor.SearchEnginePreconnector."
        "PreconnectAttemptInterval",
        base::TimeTicks::Now() - last_preconnect_attempt_time_.value());
  }
}

net::ConnectionKeepAliveConfig
SearchEnginePreconnector::GetConnectionKeepAliveConfig() {
  CHECK(SearchEnginePreconnect2Enabled());
  net::ConnectionKeepAliveConfig config;
  config.quic_connection_options = net::features::kQuicConnectionOptions.Get();

  // If the device is in low power mode, we will fallback to the old preconnect
  // interval and disable connection keepalive. This is to avoid the battery
  // drain when the device is in low power mode.
  if (ShouldSavePower()) {
    config.idle_timeout_in_seconds = kPreconnectIntervalSec;
    config.ping_interval_in_seconds = 0;
    config.enable_connection_keep_alive = false;
    return config;
  }

  config.idle_timeout_in_seconds = net::features::kIdleTimeoutInSeconds.Get();
  config.ping_interval_in_seconds = net::features::kPingIntervalInSeconds.Get();
  config.enable_connection_keep_alive = true;
  return config;
}
