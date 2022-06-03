// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_url_loader_request_interceptor.h"

#include "base/bind.h"
#include "chrome/browser/offline_pages/offline_page_url_loader.h"
#include "content/public/browser/browser_thread.h"

namespace offline_pages {

OfflinePageURLLoaderRequestInterceptor::OfflinePageURLLoaderRequestInterceptor(
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id)
    : navigation_ui_data_(navigation_ui_data),
      frame_tree_node_id_(frame_tree_node_id) {}

OfflinePageURLLoaderRequestInterceptor::
    ~OfflinePageURLLoaderRequestInterceptor() {}

void OfflinePageURLLoaderRequestInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  url_loader_ = OfflinePageURLLoader::Create(
      navigation_ui_data_, frame_tree_node_id_, tentative_resource_request,
      base::BindOnce(&OfflinePageURLLoaderRequestInterceptor::OnRequestHandled,
                     base::Unretained(this), std::move(callback)));
}

void OfflinePageURLLoaderRequestInterceptor::OnRequestHandled(
    content::URLLoaderRequestInterceptor::LoaderCallback callback,
    content::URLLoaderRequestInterceptor::RequestHandler handler) {
  // OfflinePageURLLoader decides to handle the request as offline page. Since
  // now, OfflinePageURLLoader will own itself and live as long as its URLLoader
  // and URLLoaderClient are alive.
  url_loader_.release();

  std::move(callback).Run(std::move(handler));
}

}  // namespace offline_pages
