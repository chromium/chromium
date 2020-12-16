// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/resource_loading_hints/resource_loading_hints_web_contents_observer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/previews/previews_content_util.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/previews_resource_loading_hints.mojom.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

ResourceLoadingHintsWebContentsObserver::
    ~ResourceLoadingHintsWebContentsObserver() {}

ResourceLoadingHintsWebContentsObserver::
    ResourceLoadingHintsWebContentsObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());
}

void ResourceLoadingHintsWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ReportRedirects(navigation_handle);
}

void ResourceLoadingHintsWebContentsObserver::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
}

void ResourceLoadingHintsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() || navigation_handle->IsErrorPage()) {
    return;
  }

  ReportRedirects(navigation_handle);
}

mojo::AssociatedRemote<previews::mojom::PreviewsResourceLoadingHintsReceiver>
ResourceLoadingHintsWebContentsObserver::GetResourceLoadingHintsReceiver(
    content::NavigationHandle* navigation_handle) {
  mojo::AssociatedRemote<previews::mojom::PreviewsResourceLoadingHintsReceiver>
      loading_hints_agent;

  if (navigation_handle->GetRenderFrameHost()
          ->GetRemoteAssociatedInterfaces()) {
    navigation_handle->GetRenderFrameHost()
        ->GetRemoteAssociatedInterfaces()
        ->GetInterface(&loading_hints_agent);
  }
  return loading_hints_agent;
}

void ResourceLoadingHintsWebContentsObserver::ReportRedirects(
    content::NavigationHandle* navigation_handle) {
  if (!previews::params::IsDeferAllScriptPreviewsEnabled())
    return;

  if (!previews::params::DetectDeferRedirectLoopsUsingCache())
    return;

  if (!navigation_handle)
    return;

  if (navigation_handle->GetRedirectChain().size() < 2)
    return;

  GURL url_front = navigation_handle->GetRedirectChain().front();
  GURL url_back = navigation_handle->GetRedirectChain().back();

  if (!url_front.is_valid() || !url_back.is_valid())
    return;

  PreviewsService* previews_service =
      PreviewsServiceFactory::GetForProfile(profile_);
  if (!previews_service)
    return;

  previews_service->ReportObservedRedirectWithDeferAllScriptPreview(url_front,
                                                                    url_back);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ResourceLoadingHintsWebContentsObserver)
