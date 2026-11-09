// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/search_engine_dictionary_prewarmer.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/features.h"
#include "url/url_constants.h"

namespace features {

BASE_FEATURE(kSearchEnginePrewarmDictionaryOnPreconnect,
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<SearchEnginePrewarmDictionaryCleanupMode>::Option
    kSearchEnginePrewarmDictionaryCleanupModeOptions[] = {
        {SearchEnginePrewarmDictionaryCleanupMode::kEventsOnly, "events_only"},
        {SearchEnginePrewarmDictionaryCleanupMode::kTimerOnly, "timer_only"},
        {SearchEnginePrewarmDictionaryCleanupMode::kHybrid, "hybrid"},
};

BASE_FEATURE_ENUM_PARAM(SearchEnginePrewarmDictionaryCleanupMode,
                        kSearchEnginePrewarmDictionaryCleanupMode,
                        &kSearchEnginePrewarmDictionaryOnPreconnect,
                        SearchEnginePrewarmDictionaryCleanupMode::kHybrid,
                        &kSearchEnginePrewarmDictionaryCleanupModeOptions);

constexpr base::TimeDelta kDefaultPrewarmDictionaryTimeout = base::Seconds(30);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kSearchEnginePrewarmDictionaryTimeout,
                   &kSearchEnginePrewarmDictionaryOnPreconnect,
                   kDefaultPrewarmDictionaryTimeout);

}  // namespace features

namespace {

void RecordPrewarmDictionaryEvent(
    SearchEngineDictionaryPrewarmer::SearchEnginePrewarmDictionaryEvent event) {
  base::UmaHistogramEnumeration(
      "NavigationPredictor.SearchEnginePreconnector.PrewarmDictionary.Event",
      event);
}

}  // namespace

SearchEngineDictionaryPrewarmer::SearchEngineDictionaryPrewarmer(
    const GURL& prewarm_url,
    content::BrowserContext* browser_context)
    : prewarm_url_(prewarm_url),
      browser_context_(browser_context),
      cleanup_mode_(features::kSearchEnginePrewarmDictionaryCleanupMode.Get()),
      timeout_(features::kSearchEnginePrewarmDictionaryTimeout.Get()) {
  CHECK(browser_context_);
  CHECK(base::FeatureList::IsEnabled(net::features::kSearchEnginePreconnect2));
  CHECK(base::FeatureList::IsEnabled(
      features::kSearchEnginePrewarmDictionaryOnPreconnect));
  CHECK(prewarm_url_.is_valid());
  CHECK(prewarm_url_.SchemeIs(url::kHttpsScheme));
}

SearchEngineDictionaryPrewarmer::~SearchEngineDictionaryPrewarmer() {
  CleanupPrewarmedDictionary(SearchEnginePrewarmDictionaryEvent::kDestroyed);
}

void SearchEngineDictionaryPrewarmer::Prewarm() {
  CHECK(!prewarmed_dictionary_handle_.is_bound());
  CHECK(!prewarmed_dictionary_timer_.IsRunning());

  RecordPrewarmDictionaryEvent(SearchEnginePrewarmDictionaryEvent::kPrewarmed);

  browser_context_->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->PreloadSharedDictionaryInfoForDocument(
          {prewarm_url_},
          prewarmed_dictionary_handle_.BindNewPipeAndPassReceiver());
  prewarmed_dictionary_handle_.set_disconnect_handler(base::BindOnce(
      &SearchEngineDictionaryPrewarmer::OnPrewarmedDictionaryDisconnected,
      base::Unretained(this)));

  MaybeStartExpiryTimer();
}

void SearchEngineDictionaryPrewarmer::Refresh() {
  if (!prewarmed_dictionary_handle_.is_bound()) {
    Prewarm();
    return;
  }

  RecordPrewarmDictionaryEvent(SearchEnginePrewarmDictionaryEvent::kRefreshed);
  MaybeStartExpiryTimer();
}

void SearchEngineDictionaryPrewarmer::MaybeStartExpiryTimer() {
  if (cleanup_mode_ ==
          features::SearchEnginePrewarmDictionaryCleanupMode::kTimerOnly ||
      cleanup_mode_ ==
          features::SearchEnginePrewarmDictionaryCleanupMode::kHybrid) {
    prewarmed_dictionary_timer_.Start(
        FROM_HERE, timeout_,
        base::BindOnce(
            &SearchEngineDictionaryPrewarmer::OnPrewarmedDictionaryExpired,
            base::Unretained(this)));
  }
}

void SearchEngineDictionaryPrewarmer::OnSessionClosed() {
  MaybeCleanupPrewarmedDictionary(/*is_session_closed=*/true);
}

void SearchEngineDictionaryPrewarmer::OnConnectionFailed() {
  MaybeCleanupPrewarmedDictionary(/*is_session_closed=*/false);
}

void SearchEngineDictionaryPrewarmer::MaybeCleanupPrewarmedDictionary(
    bool is_session_closed) {
  if (!prewarmed_dictionary_handle_.is_bound()) {
    return;
  }
  if (cleanup_mode_ ==
          features::SearchEnginePrewarmDictionaryCleanupMode::kEventsOnly ||
      cleanup_mode_ ==
          features::SearchEnginePrewarmDictionaryCleanupMode::kHybrid) {
    CleanupPrewarmedDictionary(
        is_session_closed
            ? SearchEnginePrewarmDictionaryEvent::kClosedSessionClosed
            : SearchEnginePrewarmDictionaryEvent::kClosedConnectionFailed);
  }
}

void SearchEngineDictionaryPrewarmer::CleanupPrewarmedDictionary(
    SearchEnginePrewarmDictionaryEvent event) {
  if (!prewarmed_dictionary_handle_.is_bound()) {
    return;
  }
  RecordPrewarmDictionaryEvent(event);
  prewarmed_dictionary_handle_.reset();
  prewarmed_dictionary_timer_.Stop();
}

void SearchEngineDictionaryPrewarmer::OnPrewarmedDictionaryExpired() {
  CleanupPrewarmedDictionary(SearchEnginePrewarmDictionaryEvent::kExpired);
}

void SearchEngineDictionaryPrewarmer::OnPrewarmedDictionaryDisconnected() {
  CleanupPrewarmedDictionary(SearchEnginePrewarmDictionaryEvent::kDisconnected);
}
