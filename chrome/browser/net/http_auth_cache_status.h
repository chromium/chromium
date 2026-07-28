// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_HTTP_AUTH_CACHE_STATUS_H_
#define CHROME_BROWSER_NET_HTTP_AUTH_CACHE_STATUS_H_

#include "base/observer_list.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-forward.h"

namespace content {
class RenderFrameHost;
class WebContents;
struct GlobalRequestID;
}  // namespace content

// HttpAuthCacheStatus is used to help determine the status of HTTP Auth on a
// given top-level frame. It is owned by the tab's TabFeatures.
class HttpAuthCacheStatus : public content::WebContentsObserver {
 public:
  explicit HttpAuthCacheStatus(content::WebContents* web_contents);

  HttpAuthCacheStatus(const HttpAuthCacheStatus&) = delete;
  HttpAuthCacheStatus& operator=(const HttpAuthCacheStatus&) = delete;
  ~HttpAuthCacheStatus() override;

  // content::WebContentsObserver:
  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const content::GlobalRequestID& request_id,
      const GURL& original_url,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override;
};
#endif  // CHROME_BROWSER_NET_HTTP_AUTH_CACHE_STATUS_H_
