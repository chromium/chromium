// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_PRECONNECTOR_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_PRECONNECTOR_H_

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/timer/timer.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace features {
BASE_DECLARE_FEATURE(kPreconnectFromKeyedService);
BASE_DECLARE_FEATURE(kPreconnectToSearch);
}  // namespace features

// Class to keep track of the current visibility. It is used to determine if the
// app is currently in the foreground or not.
class WebContentVisibilityManager {
 public:
  WebContentVisibilityManager();
  ~WebContentVisibilityManager();
  // Notifies |this| that the visibility of web contents tracked by |client| has
  // changed or if user starts a new navigation corresponding to |web_contents|.
  // Might be called more than once with the same |is_in_foreground| and
  // |web_contents| in case user starts a new navigation with same
  // |web_contents|.
  virtual void OnWebContentsVisibilityChanged(
      content::WebContents* web_contents,
      bool is_in_foreground);

  // Notifies |this| that the web contents tracked by |client| has destroyed.
  void OnWebContentsDestroyed(content::WebContents* web_contents);

  // Returns true if the browser app is likely to be in foreground and being
  // interacted by the user. This is heuristically computed by observing loading
  // and visibility of web contents.
  bool IsBrowserAppLikelyInForeground() const;

  // Overrides the tick clock used by |this| for testing.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

 private:
  std::unordered_set<raw_ptr<content::WebContents, CtnExperimental>>
      visible_web_contents_;

  base::TimeTicks last_web_contents_state_change_time_;

  raw_ptr<const base::TickClock> tick_clock_;
};

// Class to preconnect to the user's default search engine at regular intervals.
// Preconnects are made by |this| if the browser app is likely in foreground.
class SearchEnginePreconnector
    : public predictors::PreconnectManager::Delegate,
      public WebContentVisibilityManager,
      public KeyedService,
      public network::mojom::ConnectionChangeObserverClient {
 public:
  static bool ShouldBeEnabledAsKeyedService();
  static bool ShouldBeEnabledForOffTheRecord();
  static bool SearchEnginePreconnect2Enabled();

  explicit SearchEnginePreconnector(content::BrowserContext* browser_context);
  ~SearchEnginePreconnector() override;

  SearchEnginePreconnector(const SearchEnginePreconnector&) = delete;
  SearchEnginePreconnector& operator=(const SearchEnginePreconnector&) = delete;

  // Start the process of preconnecting to the default search engine.
  // |with_startup_delay| adds a delay to the preconnect, and should be true
  // only during app start up.
  void StartPreconnecting(bool with_startup_delay);

  // Stops preconnecting to the DSE. Called on app background.
  void StopPreconnecting();

  // PreconnectManager::Delegate:
  void PreconnectInitiated(const GURL& url,
                           const GURL& preconnect_url) override {}
  void PreconnectFinished(
      std::unique_ptr<predictors::PreconnectStats> stats) override {}

  // network::mojom::ConnectionChangeObserverClient
  void OnSessionClosed() override;
  void OnNetworkEvent(net::NetworkChangeEvent event) override;
  void OnConnectionFailed() override;

  // Lazily creates the PreconnectManager instance.
  predictors::PreconnectManager& GetPreconnectManager();

  // WebContentVisibilityManager methods
  // TODO(crbug.com/406022435): Update to observe the
  // `WebContentVisibilityManager` and not override them.
  void OnWebContentsVisibilityChanged(content::WebContents* web_contents,
                                      bool is_in_foreground) override;

  // Calculate the backoff multiplier to exponentially backoff when preconnects
  // are consecutively failing. The returned value should be within the range
  // of int32_t's binary digits.
  int32_t CalculateBackoffMultiplier() const;

  void SetConsecutiveFailureForTesting(int consecutive_failure) {
    consecutive_connection_failure_ = consecutive_failure;
  }

  int GetConsecutiveConnectionFailureForTesting() {
    return consecutive_connection_failure_;
  }

  void SetIsShortSessionForTesting(bool is_short_session) {
    is_short_session_for_testing_ = is_short_session;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(
      SearchEnginePreconnectorWithPreconnect2FeatureBrowserTest,
      PreconnectSearchAfterOnCloseWithShortSession);
  FRIEND_TEST_ALL_PREFIXES(
      SearchEnginePreconnectorWithPreconnect2FeatureBrowserTest,
      PreconnectSearchAfterOnFailure);
  FRIEND_TEST_ALL_PREFIXES(
      SearchEnginePreconnectorWithPreconnect2FeatureBrowserTest,
      PreconnectSearchAfterOnConnect);

  // Enum to represent the preconnect triggering event. This is used to record
  // the histogram.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(PreconnectTriggerEvent)
  enum class PreconnectTriggerEvent {
    kInitialPreconnect = 0,
    kPeriodicPreconnect = 1,  // This represents non SearchEnginePreconnect2
                              // preconnects.
    kSessionClosed = 2,
    kNetworkEvent = 3,
    kConnectionFailed = 4,
    kMaxValue = kConnectionFailed
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:PreconnectTriggerEvent)

  // Preconnects to the default search engine synchronously. Preconnects in
  // uncredentialed mode.
  void PreconnectDSE();

  // Runs `PreconnectDSE` after the `delay`.
  void StartPreconnectWithDelay(base::TimeDelta delay,
                                PreconnectTriggerEvent event);

  // Queries template service for the current DSE URL.
  GURL GetDefaultSearchEngineOriginURL() const;

  base::TimeDelta GetPreconnectInterval() const;

  // Determines if the closed session was short. This is used to calculate
  // whether we need to backoff a bit more when a session is closed to avoid
  // back-to-back connections.
  bool IsShortSession() const;

  // Invoked when the mojo pipe to the reconnect observer is disconnected.
  void OnReconnectObserverPipeDisconnected();

  void RecordPreconnectAttemptHistogram(base::TimeDelta delay,
                                        PreconnectTriggerEvent event);

  base::WeakPtr<SearchEnginePreconnector> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Determines whether the preconnector is started or not.
  bool preconnector_started_ = false;

  // Used to get keyed services.
  const raw_ptr<content::BrowserContext> browser_context_;

  // Used to preconnect regularly.
  base::OneShotTimer timer_;

  std::unique_ptr<predictors::PreconnectManager> preconnect_manager_;

  std::optional<base::TimeTicks> last_preconnect_attempt_time_;

  // Receives and dispatches method calls to this implementation of the
  // `network::mojom::ConnectionChangeObserverClient` interface.
  mojo::Receiver<network::mojom::ConnectionChangeObserverClient> receiver_{
      this};

  // How many times the connection has consecutively failed. This is used for
  // exponential backoff the preconnect retries.
  base::ClampedNumeric<int32_t> consecutive_connection_failure_ = 0;

  // Used for testing. Override the short session value.
  std::optional<bool> is_short_session_for_testing_ = std::nullopt;

  base::WeakPtrFactory<SearchEnginePreconnector> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_PRECONNECTOR_H_
