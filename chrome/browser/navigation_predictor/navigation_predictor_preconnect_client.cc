// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor_preconnect_client.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/features.h"

namespace {

// A holdback that prevents the preconnect to measure benefit of the feature.
const base::Feature kNavigationPredictorPreconnectHoldback {
  "NavigationPredictorPreconnectHoldback",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Experiment with which event triggers the preconnect after commit.
const base::Feature kPreconnectOnDidFinishNavigation{
    "PreconnectOnDidFinishNavigation", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace

NavigationPredictorPreconnectClient::NavigationPredictorPreconnectClient(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      browser_context_(web_contents->GetBrowserContext()),
      current_visibility_(web_contents->GetVisibility()) {}

NavigationPredictorPreconnectClient::~NavigationPredictorPreconnectClient() =
    default;

void NavigationPredictorPreconnectClient::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsSameDocument())
    return;

  // New page, so stop the preconnect timer.
  timer_.Stop();

  if (base::FeatureList::IsEnabled(kPreconnectOnDidFinishNavigation)) {
    int delay_ms = base::GetFieldTrialParamByFeatureAsInt(
        kPreconnectOnDidFinishNavigation, "delay_after_commit_in_ms", 3000);
    if (delay_ms <= 0) {
      MaybePreconnectNow();
      return;
    }

    timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(delay_ms),
        base::BindOnce(&NavigationPredictorPreconnectClient::MaybePreconnectNow,
                       base::Unretained(this)));
  }
}

void NavigationPredictorPreconnectClient::OnVisibilityChanged(
    content::Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (current_visibility_ == visibility)
    return;

  // Check if the visibility changed from VISIBLE to HIDDEN. Since navigation
  // predictor is currently restricted to Android, it is okay to disregard the
  // occluded state.
  if (current_visibility_ != content::Visibility::HIDDEN ||
      visibility != content::Visibility::VISIBLE) {
    current_visibility_ = visibility;

    // Stop any future preconnects while hidden.
    timer_.Stop();
    return;
  }

  current_visibility_ = visibility;

  // Previously, the visibility was HIDDEN, and now it is VISIBLE implying that
  // the web contents that was fully hidden is now fully visible.
  MaybePreconnectNow();
}

void NavigationPredictorPreconnectClient::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // Ignore sub-frame loads.
  if (render_frame_host->GetParent())
    return;

  MaybePreconnectNow();
}

void NavigationPredictorPreconnectClient::MaybePreconnectNow() {
  if (base::FeatureList::IsEnabled(kNavigationPredictorPreconnectHoldback))
    return;

  if (browser_context_->IsOffTheRecord())
    return;

  // Only preconnect foreground tab.
  if (current_visibility_ != content::Visibility::VISIBLE)
    return;

  // On search engine results page, next navigation is likely to be a different
  // origin. Currently, the preconnect is only allowed for same origins. Hence,
  // preconnect is currently disabled on search engine results page.
  if (IsSearchEnginePage())
    return;

  url::Origin preconnect_origin =
      url::Origin::Create(web_contents()->GetLastCommittedURL());
  if (preconnect_origin.scheme() != url::kHttpScheme &&
      preconnect_origin.scheme() != url::kHttpsScheme) {
    return;
  }

  auto* loading_predictor = predictors::LoadingPredictorFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
  GURL preconnect_url_serialized(preconnect_origin.Serialize());
  DCHECK(preconnect_url_serialized.is_valid());

  loading_predictor->PrepareForPageLoad(
      preconnect_url_serialized, predictors::HintOrigin::NAVIGATION_PREDICTOR,
      true);

  // The delay beyond the idle socket timeout that net uses when
  // re-preconnecting. If negative, no retries occur.
  constexpr int retry_delay_ms = 50;

  // Set/Reset the timer to fire after the preconnect times out. Add an extra
  // delay to make sure the preconnect has expired if it wasn't used.
  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          net::features::kNetUnusedIdleSocketTimeout,
          "unused_idle_socket_timeout_seconds", 60)) +
          base::TimeDelta::FromMilliseconds(retry_delay_ms),
      base::BindOnce(&NavigationPredictorPreconnectClient::MaybePreconnectNow,
                     base::Unretained(this)));
}

bool NavigationPredictorPreconnectClient::IsSearchEnginePage() const {
  auto* template_service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
  if (!template_service)
    return false;
  return template_service->IsSearchResultsPageFromDefaultSearchProvider(
      web_contents()->GetLastCommittedURL());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(NavigationPredictorPreconnectClient)
