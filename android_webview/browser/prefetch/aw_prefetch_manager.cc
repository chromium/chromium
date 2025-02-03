// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/prefetch/aw_prefetch_manager.h"

#include "content/public/browser/browser_context.h"

namespace android_webview {

AwPrefetchManager::AwPrefetchManager(content::BrowserContext* browser_context)
    : browser_context_(*browser_context) {}

AwPrefetchManager::~AwPrefetchManager() = default;

void AwPrefetchManager::StartBrowserPrefetchRequest(
    const GURL& url,
    bool javascript_enabled,
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    const net::HttpRequestHeaders& additional_headers,
    std::unique_ptr<content::PrefetchRequestStatusListener>
        request_status_listener) {
  // TODO(elabadysayed): Handle TTL and maxPrefetches.

  browser_context_->StartBrowserPrefetchRequest(
      url, javascript_enabled, std::move(no_vary_search_hint),
      additional_headers, std::move(request_status_listener));
}

}  // namespace android_webview
