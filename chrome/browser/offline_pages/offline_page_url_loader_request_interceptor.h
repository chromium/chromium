// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_URL_LOADER_REQUEST_INTERCEPTOR_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_URL_LOADER_REQUEST_INTERCEPTOR_H_

#include "content/public/browser/url_loader_request_interceptor.h"

namespace content {
class NavigationUIData;
}

namespace offline_pages {

class OfflinePageURLLoader;

class OfflinePageURLLoaderRequestInterceptor
    : public content::URLLoaderRequestInterceptor {
 public:
  OfflinePageURLLoaderRequestInterceptor(
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id);
  ~OfflinePageURLLoaderRequestInterceptor() override;

  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback) override;

 private:
  void OnRequestHandled(
      content::URLLoaderRequestInterceptor::LoaderCallback callback,
      content::URLLoaderRequestInterceptor::RequestHandler handler);

  // Not owned. The owner of this should outlive this class instance.
  content::NavigationUIData* navigation_ui_data_;

  int frame_tree_node_id_;
  std::unique_ptr<OfflinePageURLLoader> url_loader_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageURLLoaderRequestInterceptor);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_URL_LOADER_REQUEST_INTERCEPTOR_H_
