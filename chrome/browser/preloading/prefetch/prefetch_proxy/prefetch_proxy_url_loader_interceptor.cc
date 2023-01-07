// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_url_loader_interceptor.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_features.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_from_string_url_loader.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_origin_prober.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_params.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_service.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_service_factory.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_subresource_manager.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetched_mainframe_response_container.h"
#include "chrome/browser/profiles/profile.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

namespace {

Profile* ProfileFromFrameTreeNodeID(int frame_tree_node_id) {
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents)
    return nullptr;
  return Profile::FromBrowserContext(web_contents->GetBrowserContext());
}

void ReportProbeLatency(int frame_tree_node_id, base::TimeDelta probe_latency) {
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents)
    return;

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return;

  tab_helper->NotifyPrefetchProbeLatency(probe_latency);
}

void ReportProbeResult(int frame_tree_node_id,
                       const GURL& url,
                       PrefetchProxyProbeResult result) {
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents)
    return;

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return;

  tab_helper->ReportProbeResult(url, result);
}

void RecordCookieWaitTime(base::TimeDelta wait_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", wait_time,
      base::TimeDelta(), base::Seconds(5), 50);
}

void NotifySubresourceManagerOfBadProbe(int frame_tree_node_id,
                                        const GURL& url) {
  Profile* profile = ProfileFromFrameTreeNodeID(frame_tree_node_id);
  if (!profile)
    return;

  PrefetchProxyService* service =
      PrefetchProxyServiceFactory::GetForProfile(profile);
  if (!service)
    return;

  PrefetchProxySubresourceManager* subresource_manager =
      service->GetSubresourceManagerForURL(url);
  if (!subresource_manager)
    return;

  subresource_manager->NotifyProbeFailed();
}

}  // namespace

PrefetchProxyURLLoaderInterceptor::PrefetchProxyURLLoaderInterceptor(
    int frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id) {}

PrefetchProxyURLLoaderInterceptor::~PrefetchProxyURLLoaderInterceptor() =
    default;

bool PrefetchProxyURLLoaderInterceptor::MaybeInterceptNoStatePrefetchNavigation(
    const network::ResourceRequest& tentative_resource_request) {
  Profile* profile = ProfileFromFrameTreeNodeID(frame_tree_node_id_);
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);

  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(profile);
  if (!no_state_prefetch_manager)
    return false;

  if (!no_state_prefetch_manager->IsWebContentsPrefetching(web_contents))
    return false;

  PrefetchProxyService* service =
      PrefetchProxyServiceFactory::GetForProfile(profile);
  if (!service)
    return false;

  PrefetchProxySubresourceManager* manager =
      service->GetSubresourceManagerForURL(url_);
  if (!manager)
    return false;

  std::unique_ptr<PrefetchedMainframeResponseContainer> mainframe_response =
      manager->TakeMainframeResponse();
  if (!mainframe_response)
    return false;

  InterceptPrefetchedNavigation(tentative_resource_request,
                                std::move(mainframe_response));
  return true;
}

void PrefetchProxyURLLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!loader_callback_);
  loader_callback_ = std::move(callback);
  url_ = tentative_resource_request.url;

  // If this method returns true, the navigation has already been intercepted.
  if (MaybeInterceptNoStatePrefetchNavigation(tentative_resource_request))
    return;

  std::unique_ptr<PrefetchedMainframeResponseContainer> prefetch =
      GetPrefetchedResponse(url_);
  if (!prefetch) {
    DoNotInterceptNavigation();
    return;
  }

  Profile* profile = ProfileFromFrameTreeNodeID(frame_tree_node_id_);
  if (!profile) {
    DoNotInterceptNavigation();
    return;
  }
  PrefetchProxyService* service =
      PrefetchProxyServiceFactory::GetForProfile(profile);
  if (!service) {
    DoNotInterceptNavigation();
    return;
  }

  // If the cookies associated with |url_| have changed since the initial
  // eligibility check, then we shouldn't use the prefetched resources.
  PrefetchProxyTabHelper* tab_helper = PrefetchProxyTabHelper::FromWebContents(
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_));
  if (tab_helper && tab_helper->HaveCookiesChanged(url_)) {
    DoNotInterceptNavigation();
    return;
  }

  if (service->origin_prober()->ShouldProbeOrigins()) {
    probe_start_time_ = base::TimeTicks::Now();
    base::OnceClosure on_success_callback =
        base::BindOnce(&PrefetchProxyURLLoaderInterceptor::
                           EnsureCookiesCopiedAndInterceptPrefetchedNavigation,
                       weak_factory_.GetWeakPtr(), tentative_resource_request,
                       std::move(prefetch));

    service->origin_prober()->Probe(
        url_.DeprecatedGetOriginAsURL(),
        base::BindOnce(&PrefetchProxyURLLoaderInterceptor::OnProbeComplete,
                       weak_factory_.GetWeakPtr(),
                       std::move(on_success_callback)));
    return;
  }
  // Inform the metrics collector that the main frame HTML was used and probing
  // was disabled.
  ReportProbeResult(frame_tree_node_id_, url_,
                    PrefetchProxyProbeResult::kNoProbing);

  EnsureCookiesCopiedAndInterceptPrefetchedNavigation(
      tentative_resource_request, std::move(prefetch));
}

void PrefetchProxyURLLoaderInterceptor::
    EnsureCookiesCopiedAndInterceptPrefetchedNavigation(
        const network::ResourceRequest& tentative_resource_request,
        std::unique_ptr<PrefetchedMainframeResponseContainer> prefetch) {
  // The TabHelper needs to copy cookies over to the main profile's cookie jar
  // before we can commit the mainframe so that subresources have the cookies
  // they need before being put on the wire.
  PrefetchProxyTabHelper* tab_helper = PrefetchProxyTabHelper::FromWebContents(
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_));
  if (tab_helper) {
    tab_helper->OnInterceptorCheckCookieCopy();
  }
  if (tab_helper && tab_helper->IsWaitingForAfterSRPCookiesCopy()) {
    cookie_copy_start_time_ = base::TimeTicks::Now();
    tab_helper->SetOnAfterSRPCookieCopyCompleteCallback(base::BindOnce(
        &PrefetchProxyURLLoaderInterceptor::InterceptPrefetchedNavigation,
        weak_factory_.GetWeakPtr(), tentative_resource_request,
        std::move(prefetch)));
    return;
  }

  // Record that there was no wait time.
  RecordCookieWaitTime(base::TimeDelta());

  // If the cookies were already copied, commit now.
  InterceptPrefetchedNavigation(tentative_resource_request,
                                std::move(prefetch));
}

void PrefetchProxyURLLoaderInterceptor::InterceptPrefetchedNavigation(
    const network::ResourceRequest& tentative_resource_request,
    std::unique_ptr<PrefetchedMainframeResponseContainer> prefetch) {
  if (cookie_copy_start_time_) {
    base::TimeDelta wait_time =
        base::TimeTicks::Now() - *cookie_copy_start_time_;
    DCHECK_GT(wait_time, base::TimeDelta());
    RecordCookieWaitTime(wait_time);
  }

  NotifyPrefetchStatusUpdate(
      probe_start_time_.has_value()
          ? PrefetchProxyPrefetchStatus::kPrefetchUsedProbeSuccess
          : PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbe);

  std::unique_ptr<PrefetchProxyFromStringURLLoader> url_loader =
      std::make_unique<PrefetchProxyFromStringURLLoader>(
          std::move(prefetch), tentative_resource_request);
  std::move(loader_callback_).Run(url_loader->ServingResponseHandler());
  // url_loader manages its own lifetime once bound to the mojo pipes.
  url_loader.release();
}

void PrefetchProxyURLLoaderInterceptor::DoNotInterceptNavigation() {
  std::move(loader_callback_).Run({});
}

void PrefetchProxyURLLoaderInterceptor::OnProbeComplete(
    base::OnceClosure on_success_callback,
    PrefetchProxyProbeResult result) {
  DCHECK(probe_start_time_.has_value());
  ReportProbeLatency(frame_tree_node_id_,
                     base::TimeTicks::Now() - probe_start_time_.value());
  ReportProbeResult(frame_tree_node_id_, url_, result);

  if (PrefetchProxyProbeResultIsSuccess(result)) {
    std::move(on_success_callback).Run();
    return;
  }

  // Notify the SubresourceManager for this url so that subresources should not
  // be loaded from the prefetch cache.
  NotifySubresourceManagerOfBadProbe(frame_tree_node_id_, url_);

  NotifyPrefetchStatusUpdate(
      PrefetchProxyPrefetchStatus::kPrefetchNotUsedProbeFailed);
  DoNotInterceptNavigation();
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
PrefetchProxyURLLoaderInterceptor::GetPrefetchedResponse(const GURL& url) {
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  if (!web_contents)
    return nullptr;

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return nullptr;

  return tab_helper->TakePrefetchResponse(url);
}

void PrefetchProxyURLLoaderInterceptor::NotifyPrefetchStatusUpdate(
    PrefetchProxyPrefetchStatus status) const {
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  if (!web_contents) {
    return;
  }

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return;

  DCHECK(url_.is_valid());
  tab_helper->OnPrefetchStatusUpdate(url_, status);
}
