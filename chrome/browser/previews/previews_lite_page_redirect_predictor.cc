// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_redirect_predictor.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/previews/previews_lite_page_redirect_url_loader_interceptor.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/previews/content/previews_decider_impl.h"
#include "components/previews/content/previews_optimization_guide.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_lite_page_redirect.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

PreviewsLitePageRedirectPredictor::~PreviewsLitePageRedirectPredictor() {
  if (g_browser_process->network_quality_tracker()) {
    g_browser_process->network_quality_tracker()
        ->RemoveEffectiveConnectionTypeObserver(this);
  }
}

PreviewsLitePageRedirectPredictor::PreviewsLitePageRedirectPredictor(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  drp_settings_ = DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());

  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (previews_service && previews_service->previews_ui_service() &&
      previews_service->previews_ui_service()->previews_decider_impl()) {
    opt_guide_ = previews_service->previews_ui_service()
                     ->previews_decider_impl()
                     ->previews_opt_guide();
  }

  if (g_browser_process->network_quality_tracker()) {
    g_browser_process->network_quality_tracker()
        ->AddEffectiveConnectionTypeObserver(this);
  }
}

bool PreviewsLitePageRedirectPredictor::DataSaverIsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return drp_settings_ && drp_settings_->IsDataReductionProxyEnabled();
}

bool PreviewsLitePageRedirectPredictor::ECTIsSlow() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!g_browser_process->network_quality_tracker())
    return false;

  net::EffectiveConnectionType ect =
      g_browser_process->network_quality_tracker()
          ->GetEffectiveConnectionType();

  if (ect == net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN ||
      ect == net::EFFECTIVE_CONNECTION_TYPE_OFFLINE) {
    return false;
  }

  return ect <= previews::params::
                    LitePageRedirectPreviewPreresolvePreconnectECTThreshold();
}

bool PreviewsLitePageRedirectPredictor::PageIsBlacklisted(
    content::NavigationHandle* navigation_handle) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Assume that if this is called without a navigation handle, that the URL
  // associated with the navigation handle has already been checked before and
  // had already passed this check.
  if (!navigation_handle)
    return false;

  // Assume the page is blacklisted if there is no optimization guide available.
  // This matches the behavior of the preview triggering itself.
  if (!opt_guide_)
    return true;

  return !opt_guide_->CanApplyPreview(
      /*previews_user_data=*/nullptr, navigation_handle,
      previews::PreviewsType::LITE_PAGE_REDIRECT);
}

bool PreviewsLitePageRedirectPredictor::IsVisible() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return web_contents()->GetVisibility() == content::Visibility::VISIBLE;
}

base::Optional<GURL> PreviewsLitePageRedirectPredictor::ShouldActOnPage(
    content::NavigationHandle* navigation_handle) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!previews::params::LitePageRedirectPreviewShouldPresolve() &&
      !previews::params::LitePageRedirectPreviewShouldPreconnect()) {
    return base::nullopt;
  }

  if (!web_contents()->GetController().GetLastCommittedEntry())
    return base::nullopt;

  if (web_contents()->GetController().GetPendingEntry())
    return base::nullopt;

  if (!DataSaverIsEnabled())
    return base::nullopt;

  if (!previews::params::IsLitePageServerPreviewsEnabled())
    return base::nullopt;

  GURL url = web_contents()->GetController().GetLastCommittedEntry()->GetURL();

  if (!url.SchemeIs(url::kHttpsScheme))
    return base::nullopt;

  // Only check if the url is blacklisted if it is not a preview page.
  if (!previews::IsLitePageRedirectPreviewDomain(url) &&
      PageIsBlacklisted(navigation_handle)) {
    return base::nullopt;
  }

  // Only check ECT on pages that aren't previews.
  if (!previews::IsLitePageRedirectPreviewDomain(url) && !ECTIsSlow())
    return base::nullopt;

  if (!IsVisible())
    return base::nullopt;

  // If a preview is currently being shown, act on the original page. Otherwise,
  // act on the preview.
  std::string original_url;
  if (previews::ExtractOriginalURLFromLitePageRedirectURL(url, &original_url))
    return GURL(original_url);

  return previews::GetLitePageRedirectURLForURL(url);
}

void PreviewsLitePageRedirectPredictor::MaybeToggleTimer(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the timer is not null, it should be running.
  DCHECK(!timer_ || timer_->IsRunning());

  url_ = ShouldActOnPage(navigation_handle);
  if (url_.has_value() == bool(timer_))
    return;

  UMA_HISTOGRAM_BOOLEAN("Previews.ServerLitePage.PredictorToggled",
                        url_.has_value());

  if (url_.has_value()) {
    timer_.reset(new base::RepeatingTimer());
    // base::Unretained is safe because the timer will stop firing once deleted,
    // and |timer_| is owned by this.
    timer_->Start(
        FROM_HERE,
        previews::params::LitePageRedirectPreviewPreresolvePreconnectInterval(),
        base::BindRepeating(
            &PreviewsLitePageRedirectPredictor::PreresolveOrPreconnect,
            base::Unretained(this)));
    PreresolveOrPreconnect();
  } else {
    // Resetting the unique_ptr will delete the timer itself, causing it to stop
    // calling its callback.
    timer_.reset();
  }
}

void PreviewsLitePageRedirectPredictor::PreresolveOrPreconnect() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(timer_);

  predictors::LoadingPredictor* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

  if (!loading_predictor || !loading_predictor->preconnect_manager())
    return;

  if (previews::params::LitePageRedirectPreviewShouldPresolve() &&
      !previews::params::LitePageRedirectPreviewShouldPreconnect()) {
    UMA_HISTOGRAM_BOOLEAN(
        "Previews.ServerLitePage.PreresolvedToPreviewServer",
        previews::IsLitePageRedirectPreviewDomain(url_.value()));
    loading_predictor->preconnect_manager()->StartPreresolveHost(url_.value());
  }

  if (previews::params::LitePageRedirectPreviewShouldPreconnect() &&
      !previews::params::LitePageRedirectPreviewShouldPresolve()) {
    UMA_HISTOGRAM_BOOLEAN(
        "Previews.ServerLitePage.PreconnectedToPreviewServer",
        previews::IsLitePageRedirectPreviewDomain(url_.value()));
    loading_predictor->preconnect_manager()->StartPreconnectUrl(
        url_.value(), true /* allow_credentials */,
        net::NetworkIsolationKey(url::Origin::Create(url_.value()),
                                 url::Origin::Create(url_.value())));
  }
}

void PreviewsLitePageRedirectPredictor::DidStartNavigation(
    content::NavigationHandle* handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!handle->IsInMainFrame())
    return;
  MaybeToggleTimer(handle);
}

void PreviewsLitePageRedirectPredictor::DidFinishNavigation(
    content::NavigationHandle* handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!handle->IsInMainFrame())
    return;
  MaybeToggleTimer(handle);
}

void PreviewsLitePageRedirectPredictor::OnVisibilityChanged(
    content::Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeToggleTimer(/*navigation_handle=*/nullptr);
}

void PreviewsLitePageRedirectPredictor::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType ect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeToggleTimer(/*navigation_handle=*/nullptr);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PreviewsLitePageRedirectPredictor)
