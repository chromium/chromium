// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_top_host_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/hints_fetcher.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"

namespace {

bool IsValidOptimizationGuideNavigation(
    content::NavigationHandle* navigation_handle) {
  return navigation_handle->IsInMainFrame() &&
         navigation_handle->GetURL().SchemeIsHTTPOrHTTPS();
}

}  // namespace

OptimizationGuideWebContentsObserver::OptimizationGuideWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  optimization_guide_keyed_service_ =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
}

OptimizationGuideWebContentsObserver::~OptimizationGuideWebContentsObserver() =
    default;

OptimizationGuideNavigationData* OptimizationGuideWebContentsObserver::
    GetOrCreateOptimizationGuideNavigationData(
        content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int64_t navigation_id = navigation_handle->GetNavigationId();
  if (inflight_optimization_guide_navigation_datas_.find(navigation_id) ==
      inflight_optimization_guide_navigation_datas_.end()) {
    // We do not have one already - create one.
    inflight_optimization_guide_navigation_datas_[navigation_id] =
        std::make_unique<OptimizationGuideNavigationData>(navigation_id);
  }

  DCHECK(inflight_optimization_guide_navigation_datas_.find(navigation_id) !=
         inflight_optimization_guide_navigation_datas_.end());
  return inflight_optimization_guide_navigation_datas_.find(navigation_id)
      ->second.get();
}

void OptimizationGuideWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsValidOptimizationGuideNavigation(navigation_handle))
    return;

  OptimizationGuideTopHostProvider::MaybeUpdateTopHostBlocklist(
      navigation_handle);

  if (!optimization_guide_keyed_service_)
    return;

  optimization_guide_keyed_service_->OnNavigationStartOrRedirect(
      navigation_handle);
}

void OptimizationGuideWebContentsObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!IsValidOptimizationGuideNavigation(navigation_handle))
    return;

  if (!optimization_guide_keyed_service_)
    return;

  optimization_guide_keyed_service_->OnNavigationStartOrRedirect(
      navigation_handle);
}

void OptimizationGuideWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!IsValidOptimizationGuideNavigation(navigation_handle))
    return;

  // Delete Optimization Guide information later, so that other
  // DidFinishNavigation methods can reliably use
  // GetOptimizationGuideNavigationData regardless of order of
  // WebContentsObservers.
  // Note that a lot of Navigations (sub-frames, same document, non-committed,
  // etc.) might not have navigation data associated with them, but we reduce
  // likelihood of future leaks by always trying to remove the data.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &OptimizationGuideWebContentsObserver::NotifyNavigationFinish,
          weak_factory_.GetWeakPtr(), navigation_handle->GetNavigationId(),
          navigation_handle->GetRedirectChain()));
}

void OptimizationGuideWebContentsObserver::NotifyNavigationFinish(
    int64_t navigation_id,
    const std::vector<GURL>& navigation_redirect_chain) {
  auto nav_data_iter =
      inflight_optimization_guide_navigation_datas_.find(navigation_id);
  if (nav_data_iter == inflight_optimization_guide_navigation_datas_.end())
    return;

  if (optimization_guide_keyed_service_) {
    optimization_guide_keyed_service_->OnNavigationFinish(
        navigation_redirect_chain);
  }

  // We keep the last navigation data around to keep track of events happening
  // for the navigation that can happen after commit, such as a fetch for the
  // navigation successfully completing (which is not guaranteed to come back
  // before commit, if at all).
  last_navigation_data_ = std::move(nav_data_iter->second);
  inflight_optimization_guide_navigation_datas_.erase(navigation_id);
}

void OptimizationGuideWebContentsObserver::FlushLastNavigationData() {
  if (last_navigation_data_)
    last_navigation_data_.reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OptimizationGuideWebContentsObserver)
