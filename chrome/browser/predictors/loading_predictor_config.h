// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_CONFIG_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_CONFIG_H_

#include <cstddef>

#include "base/feature_list.h"

class Profile;

namespace predictors {

extern const char kSpeculativePreconnectFeatureName[];
extern const base::Feature kSpeculativePreconnectFeature;

// Returns whether the speculative preconnect feature is enabled.
bool IsPreconnectFeatureEnabled();

// Returns whether the Loading Predictor is enabled for the given |profile|. If
// true, the predictor can observe page load events, build historical database
// and perform allowed speculative actions based on this database.
bool IsLoadingPredictorEnabled(Profile* profile);

// Returns whether the current |profile| settings allow to perform preresolve
// and preconnect actions. This setting is controlled by the user, so the return
// value shouldn't be cached.
bool IsPreconnectAllowed(Profile* profile);

// Indicates what caused the page load hint.
enum class HintOrigin {
  // Triggered at the start of each navigation.
  NAVIGATION,

  // Used when a preconnect is triggered by an external Android app.
  EXTERNAL,

  // Triggered by omnibox.
  OMNIBOX,

  // Triggered by navigation predictor service.
  NAVIGATION_PREDICTOR,

  // Used when a prerender initiated by Omnibox is unsuccessful, and instead a
  // preconnect is initiated. Preconnect triggered by
  // OMNIBOX_PRERENDER_FALLBACK may be handled differently than preconnects
  // triggered by OMNIBOX since the former are triggered at higher confidence.
  OMNIBOX_PRERENDER_FALLBACK
};

// Represents the config for the Loading predictor.
struct LoadingPredictorConfig {
  // Initializes the config with default values.
  LoadingPredictorConfig();
  LoadingPredictorConfig(const LoadingPredictorConfig& other);
  ~LoadingPredictorConfig();

  bool IsSmallDBEnabledForTest() const;

  // If a navigation hasn't seen a load complete event in this much time, it
  // is considered abandoned.
  size_t max_navigation_lifetime_seconds;

  // Size of LRU caches for the host data.
  size_t max_hosts_to_track;

  // The maximum number of origins to store per entry.
  size_t max_origins_per_entry;
  // The number of consecutive misses after which we stop tracking a resource
  // URL.
  size_t max_consecutive_misses;
  // The number of consecutive misses after which we stop tracking a redirect
  // endpoint.
  size_t max_redirect_consecutive_misses;

  // Delay between writing data to the predictors database memory cache and
  // flushing it to disk.
  size_t flush_data_to_disk_delay_seconds;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_CONFIG_H_
