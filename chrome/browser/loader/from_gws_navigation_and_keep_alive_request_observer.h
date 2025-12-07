// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOADER_FROM_GWS_NAVIGATION_AND_KEEP_ALIVE_REQUEST_OBSERVER_H_
#define CHROME_BROWSER_LOADER_FROM_GWS_NAVIGATION_AND_KEEP_ALIVE_REQUEST_OBSERVER_H_

#include <stdint.h>

#include <optional>

#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace network {
struct ResourceRequest;
}  // namespace network

// FromGWSNavigationAndKeepAliveRequestObserver observes eligible navigations
// and fetch keepalive requests made from Google search result pages (SRP).
//
// For example, on an SRP, if any of the following happens, the observer will
// extract the category ID and stores the request & navigation for later UKM
// logging:
//   - fetch("https://a.com?category=...", {keepalive: true})
//   - navigate to https://b.com?category=...
class FromGWSNavigationAndKeepAliveRequestObserver
    : public content::WebContentsObserver {
 public:
  ~FromGWSNavigationAndKeepAliveRequestObserver() override;

  // Not copyable or movable.
  FromGWSNavigationAndKeepAliveRequestObserver(
      const FromGWSNavigationAndKeepAliveRequestObserver&) = delete;
  FromGWSNavigationAndKeepAliveRequestObserver& operator=(
      const FromGWSNavigationAndKeepAliveRequestObserver&) = delete;

  static std::unique_ptr<FromGWSNavigationAndKeepAliveRequestObserver>
  MaybeCreateForWebContents(content::WebContents* web_contents);

  // content::WebContentsObserver overrides:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnKeepAliveRequestCreated(
      const network::ResourceRequest& resource_request,
      content::RenderFrameHost* initiator_rfh) override;

 protected:
  explicit FromGWSNavigationAndKeepAliveRequestObserver(
      content::WebContents* web_contents);
};

#endif  // CHROME_BROWSER_LOADER_FROM_GWS_NAVIGATION_AND_KEEP_ALIVE_REQUEST_OBSERVER_H_
