// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_service.h"

#include "base/bind.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_origin_decider.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_origin_prober.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_params.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_proxy_configurator.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_subresource_manager.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetched_mainframe_response_container.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"

PrefetchProxyService::PrefetchProxyService(Profile* profile)
    : profile_(profile),
      proxy_configurator_(std::make_unique<PrefetchProxyProxyConfigurator>()),
      origin_prober_(std::make_unique<PrefetchProxyOriginProber>(profile)),
      origin_decider_(
          std::make_unique<PrefetchProxyOriginDecider>(profile->GetPrefs())) {}

PrefetchProxyService::~PrefetchProxyService() = default;

bool PrefetchProxyService::MaybeProxyURLLoaderFactory(
    content::RenderFrameHost* frame,
    int render_process_id,
    content::ContentBrowserClient::URLLoaderFactoryType type,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver) {
  for (const auto& manager_pair : subresource_managers_) {
    if (manager_pair.second->MaybeProxyURLLoaderFactory(
            frame, render_process_id, type, factory_receiver)) {
      return true;
    }
  }
  return false;
}

PrefetchProxySubresourceManager* PrefetchProxyService::OnAboutToNoStatePrefetch(
    const GURL& url,
    std::unique_ptr<PrefetchedMainframeResponseContainer> response) {
  std::unique_ptr<PrefetchProxySubresourceManager> manager =
      std::make_unique<PrefetchProxySubresourceManager>(url,
                                                        std::move(response));
  PrefetchProxySubresourceManager* manager_ptr = manager.get();
  subresource_managers_.emplace(url, std::move(manager));
  return manager_ptr;
}

PrefetchProxySubresourceManager*
PrefetchProxyService::GetSubresourceManagerForURL(const GURL& url) const {
  auto iter = subresource_managers_.find(url);
  if (iter == subresource_managers_.end())
    return nullptr;
  return iter->second.get();
}

std::unique_ptr<PrefetchProxySubresourceManager>
PrefetchProxyService::TakeSubresourceManagerForURL(const GURL& url) {
  auto iter = subresource_managers_.find(url);
  if (iter == subresource_managers_.end())
    return nullptr;
  std::unique_ptr<PrefetchProxySubresourceManager> manager =
      std::move(iter->second);
  subresource_managers_.erase(iter);
  return manager;
}

void PrefetchProxyService::DestroySubresourceManagerForURL(const GURL& url) {
  auto iter = subresource_managers_.find(url);
  if (iter != subresource_managers_.end()) {
    subresource_managers_.erase(iter);
  }
}
