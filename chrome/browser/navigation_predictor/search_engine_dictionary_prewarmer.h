// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_DICTIONARY_PREWARMER_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_DICTIONARY_PREWARMER_H_

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace features {

BASE_DECLARE_FEATURE(kSearchEnginePrewarmDictionaryOnPreconnect);

enum class SearchEnginePrewarmDictionaryCleanupMode {
  kEventsOnly = 0,
  kTimerOnly = 1,
  kHybrid = 2,
  kMaxValue = kHybrid,
};

BASE_DECLARE_FEATURE_PARAM(SearchEnginePrewarmDictionaryCleanupMode,
                           kSearchEnginePrewarmDictionaryCleanupMode);

BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kSearchEnginePrewarmDictionaryTimeout);

}  // namespace features

// SearchEngineDictionaryPrewarmer manages prewarming of compression dictionary
// metadata for the Default Search Engine via the Network Service, holding a
// pinned handle to retain dictionary metadata in memory while preconnected
// sockets are active or until a fallback timer expires.
class SearchEngineDictionaryPrewarmer {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // Note: When prewarming is initiated, SearchEngineDictionaryPrewarmer
  // speculatively requests and pins a dictionary preload handle in the Network
  // Service via Mojo before knowing whether a shared dictionary actually
  // exists on disk for that URL. Therefore, `kPrewarmed` is recorded regardless
  // of whether a matching dictionary is found in SQLite.
  //
  // LINT.IfChange(SearchEnginePrewarmDictionaryEvent)
  enum class SearchEnginePrewarmDictionaryEvent {
    // A Mojo request was issued to NetworkContext to prewarm shared dictionary
    // metadata for the Default Search Engine, and a handle was pinned.
    kPrewarmed = 0,
    // The DSE URL was already pinned; its expiry timer was refreshed.
    kRefreshed = 1,
    // The fallback timer fired without connection closure, releasing
    // the pinned dictionary handle.
    kExpired = 2,
    // The active preconnect session for the DSE closed, releasing the pinned
    // dictionary handle (in `events_only` or `hybrid` cleanup mode).
    kClosedSessionClosed = 3,
    // The preconnected socket failed, releasing the pinned dictionary handle
    // (in `events_only` or `hybrid` cleanup mode).
    kClosedConnectionFailed = 4,
    // The Mojo remote pipe to NetworkContext was disconnected (e.g. on
    // memory pressure or Network Service crash), causing immediate cleanup.
    kDisconnected = 5,
    // The prewarmer was destroyed while the dictionary handle was still pinned
    // (e.g. on StopPreconnecting(), DSE change, or early bailout).
    kDestroyed = 6,
    kMaxValue = kDestroyed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:SearchEnginePrewarmDictionaryEvent)

  SearchEngineDictionaryPrewarmer(const GURL& prewarm_url,
                                  content::BrowserContext* browser_context);
  SearchEngineDictionaryPrewarmer(const SearchEngineDictionaryPrewarmer&) =
      delete;
  SearchEngineDictionaryPrewarmer& operator=(
      const SearchEngineDictionaryPrewarmer&) = delete;
  ~SearchEngineDictionaryPrewarmer();

  // Initiates prewarming of dictionary metadata for `prewarm_url_`.
  void Prewarm();

  // If already prewarmed for the same URL, refreshes the expiry timer.
  void Refresh();

  // Called when the active preconnect session closes or fails.
  void OnSessionClosed();
  void OnConnectionFailed();

  const GURL& prewarm_url() const { return prewarm_url_; }
  bool HasPrewarmedDictionaryForTesting() const {
    return prewarmed_dictionary_handle_.is_bound();
  }
  bool IsTimerRunningForTesting() const {
    return prewarmed_dictionary_timer_.IsRunning();
  }
  void SimulateDisconnectForTesting() { OnPrewarmedDictionaryDisconnected(); }

 private:
  void MaybeStartExpiryTimer();
  void OnPrewarmedDictionaryExpired();
  void OnPrewarmedDictionaryDisconnected();
  void MaybeCleanupPrewarmedDictionary(bool is_session_closed);
  void CleanupPrewarmedDictionary(SearchEnginePrewarmDictionaryEvent event);

  const GURL prewarm_url_;
  const raw_ptr<content::BrowserContext> browser_context_;
  const features::SearchEnginePrewarmDictionaryCleanupMode cleanup_mode_;
  const base::TimeDelta timeout_;

  mojo::Remote<network::mojom::PreloadedSharedDictionaryInfoHandle>
      prewarmed_dictionary_handle_;
  base::OneShotTimer prewarmed_dictionary_timer_;
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_DICTIONARY_PREWARMER_H_
