// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_PRECONNECTOR_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_PRECONNECTOR_H_

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace features {
BASE_DECLARE_FEATURE(kPreconnectToSearch);
BASE_DECLARE_FEATURE(kPreconnectToSearchNonGoogle);
BASE_DECLARE_FEATURE(kPreconnectToSearchWithPrivacyModeEnabled);
}  // namespace features

// Class to preconnect to the user's default search engine at regular intervals.
// Preconnects are made by |this| if the browser app is likely in foreground.
class SearchEnginePreconnector {
 public:
  explicit SearchEnginePreconnector(content::BrowserContext* browser_context);
  ~SearchEnginePreconnector();

  SearchEnginePreconnector(const SearchEnginePreconnector&) = delete;
  SearchEnginePreconnector& operator=(const SearchEnginePreconnector&) = delete;

  // Start the process of preconnecting to the default search engine.
  // |with_startup_delay| adds a delay to the preconnect, and should be true
  // only during app start up.
  void StartPreconnecting(bool with_startup_delay);

  // Stops preconnecting to the DSE. Called on app background.
  void StopPreconnecting();

 private:
  // Preconnects to the default search engine synchronously. Preconnects in
  // credentialed and uncredentialed mode.
  void PreconnectDSE();

  // Queries template service for the current DSE URL.
  GURL GetDefaultSearchEngineOriginURL() const;

  // Returns true if the browser app is likely to be in foreground and being
  // interacted by the user.
  bool IsBrowserAppLikelyInForeground() const;

  // Used to get keyed services.
  const raw_ptr<content::BrowserContext> browser_context_;

  // Used to preconnect regularly.
  base::OneShotTimer timer_;
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_PRECONNECTOR_H_
