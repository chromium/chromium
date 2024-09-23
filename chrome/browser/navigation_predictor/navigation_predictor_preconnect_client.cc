// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor_preconnect_client.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_features.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"
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
#include "net/base/ip_address.h"

namespace {

// Experiment with which event triggers the preconnect after commit.
BASE_FEATURE(kPreconnectOnDidFinishNavigation,
             "PreconnectOnDidFinishNavigation",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

NavigationPredictorPreconnectClient::NavigationPredictorPreconnectClient(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<NavigationPredictorPreconnectClient>(
          *web_contents),
      browser_context_(web_contents->GetBrowserContext()),
      current_visibility_(web_contents->GetVisibility()) {}

NavigationPredictorPreconnectClient::~NavigationPredictorPreconnectClient() {
  NavigationPredictorKeyedService* navigation_predictor_service =
      GetNavigationPredictorKeyedService();
  if (navigation_predictor_service) {
    navigation_predictor_service->OnWebContentsDestroyed(web_contents());
  }
}

NavigationPredictorKeyedService*
NavigationPredictorPreconnectClient::GetNavigationPredictorKeyedService()
    const {
  return NavigationPredictorKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
}

void NavigationPredictorPreconnectClient::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Notify navigation predictor service of any same-document navigations that
  // may be captured here. Same-document navigations imply that user is
  // interacting with the browser app.
  NavigationPredictorKeyedService* navigation_predictor_service =
      GetNavigationPredictorKeyedService();
  if (navigation_predictor_service) {
    navigation_predictor_service->OnWebContentsVisibilityChanged(
        web_contents(), current_visibility_ == content::Visibility::VISIBLE);
  }

  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  if (!navigation_handle->IsSameDocument()) {
    is_publicly_routable_ = false;

    std::optional<bool> is_publicly_routable =
        IsPubliclyRoutable(navigation_handle);

    if (is_publicly_routable) {
      is_publicly_routable_ = is_publicly_routable.value();
    }
  }

  if ((!base::FeatureList::IsEnabled(
           features::
               kNavigationPredictorEnablePreconnectOnSameDocumentNavigations) &&
       navigation_handle->IsSameDocument())) {
    return;
  }

  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return;

  // New page, so stop the preconnect timer.
  timer_.Stop();

  if (base::FeatureList::IsEnabled(kPreconnectOnDidFinishNavigation) ||
      navigation_handle->IsSameDocument()) {
    int delay_ms = base::GetFieldTrialParamByFeatureAsInt(
        kPreconnectOnDidFinishNavigation, "delay_after_commit_in_ms", 3000);
    if (delay_ms <= 0) {
      MaybePreconnectNow(/*preconnects_attempted=*/0u);
      return;
    }

    timer_.Start(
        FROM_HERE, base::Milliseconds(delay_ms),
        base::BindOnce(&NavigationPredictorPreconnectClient::MaybePreconnectNow,
                       base::Unretained(this), /*preconnects_attempted=*/0u));
  }
}

void NavigationPredictorPreconnectClient::OnVisibilityChanged(
    content::Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NavigationPredictorKeyedService* navigation_predictor_service =
      GetNavigationPredictorKeyedService();
  if (navigation_predictor_service) {
    navigation_predictor_service->OnWebContentsVisibilityChanged(
        web_contents(), visibility == content::Visibility::VISIBLE);
  }

  // Check for same state.
  if (current_visibility_ == visibility)
    return;

  // Check if the visibility is now visible, if not, cancel future preconnects.
  // If visible, we can begin preconnecting.
  if (visibility != content::Visibility::VISIBLE) {
    current_visibility_ = visibility;

    // Stop any future preconnects while hidden.
    timer_.Stop();
    return;
  }

  current_visibility_ = visibility;

  // Previously, the visibility was HIDDEN, and now it is VISIBLE implying that
  // the web contents that was fully hidden is now fully visible.
  MaybePreconnectNow(/*preconnects_attempted=*/0u);
}

void NavigationPredictorPreconnectClient::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // Ignore sub-frames and non-primary mainframes load.
  if (!render_frame_host->IsInPrimaryMainFrame())
    return;

  MaybePreconnectNow(/*preconnects_attempted=*/0u);
}

void NavigationPredictorPreconnectClient::MaybePreconnectNow(
    size_t preconnects_attempted) {
  if (base::FeatureList::IsEnabled(
          features::kNavigationPredictorPreconnectHoldback))
    return;

  if (browser_context_->IsOffTheRecord())
    return;

  // Only preconnect foreground tab.
  if (current_visibility_ != content::Visibility::VISIBLE)
    return;

  // Only allow 5 preconnects per foreground/load.
  if (preconnects_attempted >= 5u)
    return;

  // On search engine results page, next navigation is likely to be a
  // different origin. Currently, the preconnect is only allowed for same
  // origins. Hence, preconnect is currently disabled on search engine results
  // page. If preconnect to DSE is enabled, skip this check.
  if (!base::FeatureList::IsEnabled(features::kPreconnectToSearch) &&
      IsSearchEnginePage())
    return;

  url::Origin preconnect_origin =
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  if (preconnect_origin.scheme() != url::kHttpScheme &&
      preconnect_origin.scheme() != url::kHttpsScheme) {
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("NavigationPredictor.IsPubliclyRoutable",
                        is_publicly_routable_);

  // Disable preconnecting to servers that are not publicly routable. These
  // could likely be small IoT servers that may not support extra traffic.
  if (!is_publicly_routable_)
    return;

  auto* loading_predictor = predictors::LoadingPredictorFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
  GURL preconnect_url_serialized(preconnect_origin.Serialize());
  DCHECK(preconnect_url_serialized.is_valid());

  if (!loading_predictor)
    return;

  loading_predictor->PrepareForPageLoad(
      preconnect_origin, preconnect_url_serialized,
      predictors::HintOrigin::NAVIGATION_PREDICTOR, true);

  // The delay beyond the idle socket timeout that net uses when
  // re-preconnecting. If negative, no retries occur.
  const base::TimeDelta retry_delay = base::Milliseconds(50);

  // Set/Reset the timer to fire after the preconnect times out. Add an extra
  // delay to make sure the preconnect has expired if it wasn't used.
  timer_.Start(
      FROM_HERE,
      base::Seconds(base::GetFieldTrialParamByFeatureAsInt(
          net::features::kNetUnusedIdleSocketTimeout,
          "unused_idle_socket_timeout_seconds", 60)) +
          retry_delay,
      base::BindOnce(&NavigationPredictorPreconnectClient::MaybePreconnectNow,
                     base::Unretained(this), preconnects_attempted + 1));
}

bool NavigationPredictorPreconnectClient::IsSearchEnginePage() const {
  auto* template_service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
  if (!template_service)
    return false;
  return template_service->IsSearchResultsPageFromDefaultSearchProvider(
      web_contents()->GetLastCommittedURL());
}

std::optional<bool> NavigationPredictorPreconnectClient::IsPubliclyRoutable(
    content::NavigationHandle* navigation_handle) const {
  net::IPEndPoint remote_endpoint = navigation_handle->GetSocketAddress();
  net::IPAddress page_ip_address_ = remote_endpoint.address();

  // Sometimes the IP address may not be set (e.g., if the socket is being
  // reused).
  if (!page_ip_address_.IsValid()) {
    return std::nullopt;
  }

  if (!enable_preconnects_for_local_ips_for_testing_) {
    if (page_ip_address_.IsLoopback() ||
        !page_ip_address_.IsPubliclyRoutable()) {
      return false;
    }
  }
  return true;
}

bool NavigationPredictorPreconnectClient::
    enable_preconnects_for_local_ips_for_testing_ = false;

WEB_CONTENTS_USER_DATA_KEY_IMPL(NavigationPredictorPreconnectClient);
