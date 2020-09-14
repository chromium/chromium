// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor_renderer_warmup_client.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
const base::Feature kNavigationPredictorRendererWarmup{
    "NavigationPredictorRendererWarmup", base::FEATURE_DISABLED_BY_DEFAULT};
}

NavigationPredictorRendererWarmupClient::PredictionMetrics::
    PredictionMetrics() = default;
NavigationPredictorRendererWarmupClient::PredictionMetrics::
    ~PredictionMetrics() = default;

NavigationPredictorRendererWarmupClient::
    ~NavigationPredictorRendererWarmupClient() = default;
NavigationPredictorRendererWarmupClient::
    NavigationPredictorRendererWarmupClient(Profile* profile,
                                            const base::TickClock* clock)
    : profile_(profile),
      counterfactual_(base::GetFieldTrialParamByFeatureAsBool(
          kNavigationPredictorRendererWarmup,
          "counterfactual",
          false)),
      mem_threshold_mb_(base::GetFieldTrialParamByFeatureAsInt(
          kNavigationPredictorRendererWarmup,
          "mem_threshold_mb",
          1024)),
      warmup_on_dse_(base::GetFieldTrialParamByFeatureAsBool(
          kNavigationPredictorRendererWarmup,
          "warmup_on_dse",
          true)),
      use_navigation_predictions_(base::GetFieldTrialParamByFeatureAsBool(
          kNavigationPredictorRendererWarmup,
          "use_navigation_predictions",
          true)),
      examine_top_n_predictions_(base::GetFieldTrialParamByFeatureAsInt(
          kNavigationPredictorRendererWarmup,
          "examine_top_n_predictions",
          10)),
      prediction_crosss_origin_threshold_(
          base::GetFieldTrialParamByFeatureAsDouble(
              kNavigationPredictorRendererWarmup,
              "prediction_crosss_origin_threshold",
              0.5)),
      cooldown_duration_(base::TimeDelta::FromMilliseconds(
          base::GetFieldTrialParamByFeatureAsInt(
              kNavigationPredictorRendererWarmup,
              "cooldown_duration_ms",
              60 * 1000))),
      renderer_warmup_delay_(base::TimeDelta::FromMilliseconds(
          base::GetFieldTrialParamByFeatureAsInt(
              kNavigationPredictorRendererWarmup,
              "renderer_warmup_delay_ms",
              0))) {
  if (clock) {
    tick_clock_ = clock;
  } else {
    tick_clock_ = base::DefaultTickClock::GetInstance();
  }
}

void NavigationPredictorRendererWarmupClient::OnPredictionUpdated(
    const base::Optional<NavigationPredictorKeyedService::Prediction>
        prediction) {
  DCHECK(!metrics_);

  if (!prediction) {
    return;
  }

  if (prediction->prediction_source() !=
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage) {
    return;
  }

  if (!prediction->web_contents()) {
    return;
  }

  if (!prediction->source_document_url()) {
    return;
  }

  if (!prediction->source_document_url()->is_valid()) {
    return;
  }

  if (!base::FeatureList::IsEnabled(kNavigationPredictorRendererWarmup)) {
    return;
  }

  metrics_ = std::make_unique<PredictionMetrics>();

  // Each of these methods will set some state in |metrics_| which is used in
  // |RecordMetricsAndMaybeDoWarmup|.
  CheckIsEligibleForWarmupOnCommonCriteria();
  CheckIsEligibleForCrossNavigationWarmup(*prediction);
  CheckIsEligibleForDSEWarmup(*prediction);

  RecordMetricsAndMaybeDoWarmup(prediction->web_contents());
}

void NavigationPredictorRendererWarmupClient::DoRendererWarmpup() {
  content::RenderProcessHost::WarmupSpareRenderProcessHost(profile_);
}

bool NavigationPredictorRendererWarmupClient::BrowserHasSpareRenderer() const {
  for (content::RenderProcessHost::iterator iter(
           content::RenderProcessHost::AllHostsIterator());
       !iter.IsAtEnd(); iter.Advance()) {
    if (iter.GetCurrentValue()->IsUnused()) {
      return true;
    }
  }
  return false;
}

void NavigationPredictorRendererWarmupClient::
    CheckIsEligibleForWarmupOnCommonCriteria() {
  base::TimeDelta duration_since_last_warmup =
      tick_clock_->NowTicks() - last_warmup_time_;
  if (cooldown_duration_ >= duration_since_last_warmup) {
    metrics_->page_independent_status |= 1 << 0;
  }

  if (BrowserHasSpareRenderer()) {
    metrics_->page_independent_status |= 1 << 1;
  }

  if (mem_threshold_mb_ >= base::SysInfo::AmountOfPhysicalMemoryMB()) {
    metrics_->page_independent_status |= 1 << 2;
  }
}

void NavigationPredictorRendererWarmupClient::
    CheckIsEligibleForCrossNavigationWarmup(
        const NavigationPredictorKeyedService::Prediction& prediction) {
  url::Origin src_origin =
      url::Origin::Create(prediction.source_document_url().value());

  const std::vector<GURL> urls = prediction.sorted_predicted_urls();

  size_t examine_n_urls =
      std::min(urls.size(), static_cast<size_t>(examine_top_n_predictions_));
  if (examine_n_urls == 0) {
    return;
  }

  int cross_origin_count = 0;
  for (size_t i = 0; i < examine_n_urls; ++i) {
    const GURL& url = urls[i];

    if (!url.is_valid()) {
      continue;
    }

    if (!url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }

    url::Origin url_origin = url::Origin::Create(url);
    if (!url_origin.IsSameOriginWith(src_origin)) {
      cross_origin_count++;
    }
  }

  // Just in case there's very few links on a page, use a ratio. This may be
  // helpful on redirector sites, like Cloudflare's DDoS checker.
  metrics_->cross_origin_links_ratio = static_cast<double>(cross_origin_count) /
                                       static_cast<double>(examine_n_urls);
}

void NavigationPredictorRendererWarmupClient::CheckIsEligibleForDSEWarmup(
    const NavigationPredictorKeyedService::Prediction& prediction) {
  metrics_->was_dse_srp = TemplateURLServiceFactory::GetForProfile(profile_)
                              ->IsSearchResultsPageFromDefaultSearchProvider(
                                  prediction.source_document_url().value());
}

void NavigationPredictorRendererWarmupClient::RecordMetricsAndMaybeDoWarmup(
    content::WebContents* web_contents) {
  bool eligible_on_common_criteria = metrics_->page_independent_status == 0;

  bool eligible_for_cross_origin_warmup =
      use_navigation_predictions_ &&
      metrics_->cross_origin_links_ratio.has_value() &&
      metrics_->cross_origin_links_ratio.value() >=
          prediction_crosss_origin_threshold_;

  bool eligible_for_dse_warmup = warmup_on_dse_ && metrics_->was_dse_srp;

  bool do_warmup =
      eligible_on_common_criteria &&
      (eligible_for_cross_origin_warmup || eligible_for_dse_warmup);
  metrics_->did_warmup = do_warmup;

  // Record metrics in UKM.
  ukm::builders::NavigationPredictorRendererWarmup builder(
      web_contents->GetMainFrame()->GetPageUkmSourceId());
  if (metrics_->cross_origin_links_ratio) {
    builder.SetCrossOriginLinksRatio(
        static_cast<int>(100.0 * metrics_->cross_origin_links_ratio.value()));
  }
  builder.SetDidWarmup(metrics_->did_warmup);
  builder.SetPageIndependentStatusBitMask(metrics_->page_independent_status);
  builder.SetWasDSESRP(metrics_->was_dse_srp);
  builder.Record(ukm::UkmRecorder::Get());

  metrics_.reset();

  if (!do_warmup) {
    return;
  }

  last_warmup_time_ = tick_clock_->NowTicks();

  if (counterfactual_) {
    return;
  }

  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &NavigationPredictorRendererWarmupClient::DoRendererWarmpup,
          weak_factory_.GetWeakPtr()),
      renderer_warmup_delay_);
}
