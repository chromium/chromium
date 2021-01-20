// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_RENDERER_WARMUP_CLIENT_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_RENDERER_WARMUP_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"

class Profile;

namespace content {
class WebContents;
}

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
  // Metrics per-prediction about how this class acts, for recording in UKM.
  struct PredictionMetrics {
    PredictionMetrics();
    ~PredictionMetrics();

    // A bitmask that records common criteria.
    //   1 << 0 = Set if the feature is in a cooldown period following the last
    //            renderer warmup.
    //   1 << 1 = Set if the browser already had a spare renderer.
    //   1 << 2 = Set if the device memory is below the experiment threshold.
    size_t page_independent_status = 0;

    // Represents the ratio of the links on a page that are cross-origin to the
    // source document. Set when there are 1 or more links on a page.
    base::Optional<double> cross_origin_links_ratio;

    // Whether the source document was a search result page for the default
    // search engine.
    bool was_dse_srp = false;

    // Set when all eligibility criteria are met, regardless of
    // |counterfactual_|.
    bool did_warmup = false;
  };
  std::unique_ptr<PredictionMetrics> metrics_;

  // Checks if there is already a spare renderer or we requested a spare
  // renderer too recently.
  void CheckIsEligibleForWarmupOnCommonCriteria();

  // Checks if the |prediction| is eligible to trigger a renderer warmup based
  // on the number of predicted origins.
  void CheckIsEligibleForCrossNavigationWarmup(
      const NavigationPredictorKeyedService::Prediction& prediction);

  // Checks if the |prediction| is eligible to trigger a renderer warmup based
  // on the current page being search results for the default search engine.
  void CheckIsEligibleForDSEWarmup(
      const NavigationPredictorKeyedService::Prediction& prediction);

  // Records class state and metrics before checking |counterfactual_| and then
  // calling |DoRendererWarmpup| if |counterfactual_| is false.
  void RecordMetricsAndMaybeDoWarmup(content::WebContents* web_contents);

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
