// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/anchor_element_preloader.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/page_transition_types.h"
#include "url/scheme_host_port.h"

namespace {
bool is_match_for_preconnect(const url::SchemeHostPort& preconnected_origin,
                             const GURL& visited_url) {
  return preconnected_origin == url::SchemeHostPort(visited_url);
}
}  // anonymous namespace

const char kPreloadingAnchorElementPreloaderPreloadingTriggered[] =
    "Preloading.AnchorElementPreloader.PreloadingTriggered";

content::PreloadingFailureReason ToFailureReason(
    AnchorPreloadingFailureReason reason) {
  return static_cast<content::PreloadingFailureReason>(reason);
}

AnchorElementPreloader::~AnchorElementPreloader() = default;

AnchorElementPreloader::AnchorElementPreloader(
    content::RenderFrameHost& render_frame_host)
    : render_frame_host_(render_frame_host) {
  content::PreloadingData* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(
          content::WebContents::FromRenderFrameHost(&*render_frame_host_));
  preloading_data->SetIsNavigationInDomainCallback(
      chrome_preloading_predictor::kPointerDownOnAnchor,
      base::BindRepeating([](content::NavigationHandle* navigation_handle)
                              -> bool {
        auto page_transition = navigation_handle->GetPageTransition();
        return ui::PageTransitionCoreTypeIs(
                   page_transition, ui::PageTransition::PAGE_TRANSITION_LINK) &&
               (page_transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT) == 0 &&
               ui::PageTransitionIsNewNavigation(page_transition);
      }));
}

void AnchorElementPreloader::MaybePreconnect(const GURL& target) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&*render_frame_host_);
  content::PreloadingData* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(web_contents);
  url::SchemeHostPort scheme_host_port(target);
  content::PreloadingURLMatchCallback match_callback =
      base::BindRepeating(is_match_for_preconnect, scheme_host_port);

  // For now we add a prediction with a confidence of 100. In the future we will
  // likely compute the confidence by looking at different factors (e.g. anchor
  // element dimensions, last time since scroll, etc.).
  ukm::SourceId triggered_primary_page_source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  preloading_data->AddPreloadingPrediction(
      chrome_preloading_predictor::kPointerDownOnAnchor,
      /*confidence=*/100, match_callback, triggered_primary_page_source_id);
  content::PreloadingAttempt* attempt = preloading_data->AddPreloadingAttempt(
      chrome_preloading_predictor::kPointerDownOnAnchor,
      content::PreloadingType::kPreconnect, match_callback,
      /*planned_max_preloading_type=*/std::nullopt,
      triggered_primary_page_source_id);

  if (content::PreloadingEligibility eligibility =
          prefetch::IsSomePreloadingEnabled(
              *Profile::FromBrowserContext(
                   render_frame_host_->GetBrowserContext())
                   ->GetPrefs());
      eligibility != content::PreloadingEligibility::kEligible) {
    attempt->SetEligibility(eligibility);
    return;
  }

  auto* loading_predictor = predictors::LoadingPredictorFactory::GetForProfile(
      Profile::FromBrowserContext(render_frame_host_->GetBrowserContext()));
  if (!loading_predictor) {
    attempt->SetEligibility(ToPreloadingEligibility(
        ChromePreloadingEligibility::kUnableToGetLoadingPredictor));
    return;
  }

  attempt->SetEligibility(content::PreloadingEligibility::kEligible);
  RecordUmaPreloadedTriggered(AnchorElementPreloaderType::kPreconnect);

  // In addition to the globally-controlled preloading config, check for the
  // feature-specific holdback. We disable the feature if the user is in either
  // of those holdbacks.
  if (base::GetFieldTrialParamByFeatureAsBool(
          blink::features::kAnchorElementInteraction, "preconnect_holdback",
          false)) {
    attempt->SetHoldbackStatus(content::PreloadingHoldbackStatus::kHoldback);
  }
  if (attempt->ShouldHoldback()) {
    return;
  }

  if (preconnected_targets_.find(scheme_host_port) !=
      preconnected_targets_.end()) {
    // We've already preconnected to that origin.
    attempt->SetTriggeringOutcome(
        content::PreloadingTriggeringOutcome::kDuplicate);
    return;
  }
  int max_preloading_attempts = base::GetFieldTrialParamByFeatureAsInt(
      blink::features::kAnchorElementInteraction, "max_preloading_attempts",
      -1);
  if (max_preloading_attempts >= 0 &&
      preconnected_targets_.size() >=
          static_cast<size_t>(max_preloading_attempts)) {
    attempt->SetFailureReason(
        ToFailureReason(AnchorPreloadingFailureReason::kLimitExceeded));
    return;
  }
  preconnected_targets_.insert(scheme_host_port);
  attempt->SetTriggeringOutcome(
      content::PreloadingTriggeringOutcome::kTriggeredButOutcomeUnknown);

  net::SchemefulSite schemeful_site(target);
  auto network_anonymization_key =
      net::NetworkAnonymizationKey::CreateSameSite(schemeful_site);
  loading_predictor->PreconnectURLIfAllowed(target, /*allow_credentials=*/true,
                                            network_anonymization_key);
}

void AnchorElementPreloader::RecordUmaPreloadedTriggered(
    AnchorElementPreloaderType preload) {
  base::UmaHistogramEnumeration(
      kPreloadingAnchorElementPreloaderPreloadingTriggered, preload);
}
