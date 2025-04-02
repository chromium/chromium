// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_PRECONNECTOR_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_PRECONNECTOR_H_

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace features {
BASE_DECLARE_FEATURE(kPreconnectFromKeyedService);
BASE_DECLARE_FEATURE(kPreconnectToSearch);
BASE_DECLARE_FEATURE(kPreconnectToSearchNonGoogle);
BASE_DECLARE_FEATURE(kPreconnectToSearchWithPrivacyModeEnabled);
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
  void OnWebContentsVisibilityChanged(content::WebContents* web_contents,
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
class SearchEnginePreconnector : public predictors::PreconnectManager::Delegate,
                                 public WebContentVisibilityManager,
                                 public KeyedService {
 public:
  static bool ShouldBeEnabledAsKeyedService();
  static bool ShouldBeEnabledForOffTheRecord();

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

  // Lazily creates the PreconnectManager instance.
  predictors::PreconnectManager& GetPreconnectManager();

 private:
  // Preconnects to the default search engine synchronously. Preconnects in
  // credentialed and uncredentialed mode.
  void PreconnectDSE();

  // Queries template service for the current DSE URL.
  GURL GetDefaultSearchEngineOriginURL() const;

  int GetPreconnectIntervalSec() const;

  base::WeakPtr<SearchEnginePreconnector> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Used to get keyed services.
  const raw_ptr<content::BrowserContext> browser_context_;

  // Used to preconnect regularly.
  base::OneShotTimer timer_;

  std::unique_ptr<predictors::PreconnectManager> preconnect_manager_;

  base::WeakPtrFactory<SearchEnginePreconnector> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_PRECONNECTOR_H_
