// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_RENDERER_WARMUP_CLIENT_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_RENDERER_WARMUP_CLIENT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"

class Profile;

// A client of Navigation Predictor that uses predictions to initiate a renderer
// warmup (in the form of starting a spare renderer process) when it is likely
// the user will soon do a cross-origin navigation.
class NavigationPredictorRendererWarmupClient
    : public NavigationPredictorKeyedService::Observer {
 public:
  // If |clock| is null, then the default clock will be used.
  explicit NavigationPredictorRendererWarmupClient(
      Profile* profile,
      const base::TickClock* clock = nullptr);
  ~NavigationPredictorRendererWarmupClient() override;

  // NavigationPredictorKeyedService::Observer:
  void OnPredictionUpdated(
      const base::Optional<NavigationPredictorKeyedService::Prediction>
          prediction) override;

 protected:
  // Virtual for testing.
  virtual void DoRendererWarmpup();

  // Returns true if there is a spare renderer in the browser. Virtual for
  // testing.
  virtual bool BrowserHasSpareRenderer() const;

 private:
  // Checks if there is already a spare renderer or we requested a spare
  // renderer too recently.
  bool IsEligibleForWarmupOnCommonCriteria() const;

  // Checks if the |prediction| is eligible to trigger a renderer warmup based
  // on the number of predicted origins.
  bool IsEligibleForCrossNavigationWarmup(
      const NavigationPredictorKeyedService::Prediction& prediction) const;

  // Checks if the |prediction| is eligible to trigger a renderer warmup based
  // on the current page being search results for the default search engine.
  bool IsEligibleForDSEWarmup(
      const NavigationPredictorKeyedService::Prediction& prediction) const;

  // Records class state and metrics before checking |counterfactual_| and then
  // calling |DoRendererWarmpup| if |counterfactual_| is false.
  void RecordMetricsAndMaybeDoWarmup();

  Profile* profile_;

  // Whether we are in a counterfactual experiment and so the renderer warmup
  // should not be done.
  const bool counterfactual_;

  // The minimum amount of memory the devices is required to have to enable
  // renderer warmup.
  const int mem_threshold_mb_;

  // Whether to initiate a renderer warmup on a search result page for the
  // default search engine.
  const bool warmup_on_dse_;

  // Whether to initiate a renderer warmup based on the top N predictions being
  // cross origin.
  const bool use_navigation_predictions_;
  // How many prediction urls to examine.
  const int examine_top_n_predictions_;
  // The threshold ratio of how many of the top urls need to be cross-origin.
  const double prediction_crosss_origin_threshold_;

  // The tick clock used within this class.
  const base::TickClock* tick_clock_;

  // The timestamp of the last renderer warmup.
  base::TimeTicks last_warmup_time_;

  // The amount of time to wait in-between doing a renderer warmup.
  const base::TimeDelta cooldown_duration_;

  // The amount of time to delay starting a spare renderer.
  const base::TimeDelta renderer_warmup_delay_;

  base::WeakPtrFactory<NavigationPredictorRendererWarmupClient> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(NavigationPredictorRendererWarmupClient);
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_RENDERER_WARMUP_CLIENT_H_
