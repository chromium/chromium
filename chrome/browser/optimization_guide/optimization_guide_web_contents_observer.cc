// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/optimization_guide/chrome_hints_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/google/core/common/google_util.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/optimization_guide/core/hints_fetcher.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

bool IsValidOptimizationGuideNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame())
    return false;

  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return false;

  // Now check if this is a NSP navigation. NSP is not a valid navigation.
  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
  if (!no_state_prefetch_manager) {
    // Not a NSP navigation if there is no NSP manager.
    return true;
  }
  return !(no_state_prefetch_manager->IsWebContentsPrefetching(
      navigation_handle->GetWebContents()));
}

}  // namespace

OptimizationGuideWebContentsObserver::OptimizationGuideWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<OptimizationGuideWebContentsObserver>(
          *web_contents) {
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
  DCHECK_EQ(web_contents(), navigation_handle->GetWebContents());

  NavigationHandleData* navigation_handle_data =
      NavigationHandleData::GetOrCreateForNavigationHandle(*navigation_handle);
  OptimizationGuideNavigationData* navigation_data =
      navigation_handle_data->GetOptimizationGuideNavigationData();
  if (!navigation_data) {
    // We do not have one already - create one.
    navigation_handle_data->SetOptimizationGuideNavigationData(
        std::make_unique<OptimizationGuideNavigationData>(
            navigation_handle->GetNavigationId(),
            navigation_handle->NavigationStart()));
    navigation_data =
        navigation_handle_data->GetOptimizationGuideNavigationData();
  }

  DCHECK(navigation_data);
  return navigation_data;
}

void OptimizationGuideWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!IsValidOptimizationGuideNavigation(navigation_handle))
    return;

  if (!optimization_guide_keyed_service_)
    return;

  OptimizationGuideNavigationData* navigation_data =
      GetOrCreateOptimizationGuideNavigationData(navigation_handle);
  navigation_data->set_navigation_url(navigation_handle->GetURL());
  optimization_guide_keyed_service_->OnNavigationStartOrRedirect(
      navigation_data);
}

void OptimizationGuideWebContentsObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!IsValidOptimizationGuideNavigation(navigation_handle))
    return;

  if (!optimization_guide_keyed_service_)
    return;

  OptimizationGuideNavigationData* navigation_data =
      GetOrCreateOptimizationGuideNavigationData(navigation_handle);
  navigation_data->set_navigation_url(navigation_handle->GetURL());
  optimization_guide_keyed_service_->OnNavigationStartOrRedirect(
      navigation_data);
}

void OptimizationGuideWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!IsValidOptimizationGuideNavigation(navigation_handle))
    return;

  // Note that a lot of Navigations (same document, non-committed, etc.) might
  // not have navigation data associated with them, but we reduce likelihood of
  // future leaks by always trying to remove the data.
  NavigationHandleData* navigation_handle_data =
      NavigationHandleData::GetForNavigationHandle(*navigation_handle);
  if (!navigation_handle_data)
    return;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &OptimizationGuideWebContentsObserver::NotifyNavigationFinish,
          weak_factory_.GetWeakPtr(),
          navigation_handle_data->TakeOptimizationGuideNavigationData(),
          navigation_handle->GetRedirectChain()));
}

void OptimizationGuideWebContentsObserver::WebContentsDestroyed() {
  // The web contents are being destroyed. Stop observing.
  Observe(/*web_contents=*/nullptr);
}

void OptimizationGuideWebContentsObserver::
    DocumentOnLoadCompletedInPrimaryMainFrame() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents() || !web_contents()
                              ->GetPrimaryMainFrame()
                              ->GetLastCommittedURL()
                              .SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (!optimization_guide_keyed_service_)
    return;

  if (optimization_guide::features::IsSRPFetchingEnabled() &&
      google_util::IsGoogleSearchUrl(
          web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL())) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &OptimizationGuideWebContentsObserver::FetchHintsUsingManager,
            weak_factory_.GetWeakPtr(),
            optimization_guide_keyed_service_->GetHintsManager(),
            web_contents()->GetPrimaryPage().GetWeakPtr()),
        optimization_guide::features::GetOnloadDelayForHintsFetching());
  }
}

void OptimizationGuideWebContentsObserver::FetchHintsUsingManager(
    optimization_guide::ChromeHintsManager* hints_manager,
    base::WeakPtr<content::Page> page) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(hints_manager);
  if (!page)
    return;

  CHECK(optimization_guide::features::IsSRPFetchingEnabled());
  PageData& page_data = GetPageData(*page);

  std::vector<GURL> top_urls = page_data.GetHintsTargetUrls();

  if (!top_urls.empty()) {
    page_data.set_sent_batched_hints_request();
    top_urls.resize(
        std::min(top_urls.size(),
                 optimization_guide::features::MaxResultsForSRPFetch()));
    hints_manager->FetchHintsForURLs(
        top_urls, optimization_guide::proto::CONTEXT_BATCH_UPDATE_GOOGLE_SRP);
  }
}

void OptimizationGuideWebContentsObserver::NotifyNavigationFinish(
    std::unique_ptr<OptimizationGuideNavigationData> navigation_data,
    const std::vector<GURL>& navigation_redirect_chain) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (optimization_guide_keyed_service_) {
    optimization_guide_keyed_service_->OnNavigationFinish(
        navigation_redirect_chain);
  }

  // We keep the navigation data in the PageData around to keep track of events
  // happening for the navigation that can happen after commit, such as a fetch
  // for the navigation successfully completing (which is not guaranteed to come
  // back before commit, if at all).
  if (!web_contents()) {
    return;
  }
  PageData& page_data = GetPageData(web_contents()->GetPrimaryPage());
  page_data.SetNavigationData(std::move(navigation_data));
}

void OptimizationGuideWebContentsObserver::FlushLastNavigationData() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents()) {
    return;
  }
  PageData& page_data = GetPageData(web_contents()->GetPrimaryPage());
  page_data.SetNavigationData(nullptr);
}

void OptimizationGuideWebContentsObserver::AddURLsToBatchFetchBasedOnPrediction(
    std::vector<GURL> urls,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!this->web_contents())
    return;
  DCHECK_EQ(this->web_contents(), web_contents);

  PageData& page_data = GetPageData(web_contents->GetPrimaryPage());
  if (page_data.is_sent_batched_hints_request())
    return;
  page_data.InsertHintTargetUrls(urls);

  // In rare cases (such as some in browsertests), the onload event could come
  // earlier than the first predictions, in which case we should attempt the
  // fetch as prediction URLs are received.
  if (web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &OptimizationGuideWebContentsObserver::FetchHintsUsingManager,
            weak_factory_.GetWeakPtr(),
            optimization_guide_keyed_service_->GetHintsManager(),
            web_contents->GetPrimaryPage().GetWeakPtr()));
  }
}

OptimizationGuideWebContentsObserver::PageData&
OptimizationGuideWebContentsObserver::GetPageData(content::Page& page) {
  return *PageData::GetOrCreateForPage(page);
}

OptimizationGuideWebContentsObserver::PageData::PageData(content::Page& page)
    : PageUserData(page) {}
OptimizationGuideWebContentsObserver::PageData::~PageData() = default;

void OptimizationGuideWebContentsObserver::PageData::InsertHintTargetUrls(
    const std::vector<GURL>& urls) {
  DCHECK(!sent_batched_hints_request_);
  for (const GURL& url : urls)
    hints_target_urls_.insert(url);
}

std::vector<GURL>
OptimizationGuideWebContentsObserver::PageData::GetHintsTargetUrls() {
  std::vector<GURL> target_urls = std::move(hints_target_urls_.vector());
  hints_target_urls_.clear();
  return target_urls;
}

OptimizationGuideWebContentsObserver::NavigationHandleData::
    NavigationHandleData(content::NavigationHandle&) {}
OptimizationGuideWebContentsObserver::NavigationHandleData::
    ~NavigationHandleData() = default;

PAGE_USER_DATA_KEY_IMPL(OptimizationGuideWebContentsObserver::PageData);
NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(
    OptimizationGuideWebContentsObserver::NavigationHandleData);
WEB_CONTENTS_USER_DATA_KEY_IMPL(OptimizationGuideWebContentsObserver);
