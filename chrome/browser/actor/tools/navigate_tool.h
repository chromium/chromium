// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/tools/tool.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace actor {

// Navigates a tab to the given URL.
class NavigateTool : public Tool, content::WebContentsObserver {
 public:
  NavigateTool(tabs::TabInterface& tab, const GURL& url);
  ~NavigateTool() override;

  // actor::Tool
  void Validate(ValidateCallback callback) override;
  void Invoke(InvokeCallback callback) override;
  std::string DebugString() const override;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnFirstContentfulPaintInPrimaryMainFrame() override;
  void DidStopLoading() override;

 private:
  void NavigationHandleCallback(content::NavigationHandle& handle);
  void Timeout();

  GURL url_;

  // Holds the callback to the Invoke method. Null before invoke is called.
  InvokeCallback invoke_callback_;

  // The ID of the navigation to `url_`, unset until the navigation is started,
  // after which this is set (asynchronously). Once set, this class observes the
  // WebContents until this handle completes and the above callback is invoked.
  std::optional<int64_t> pending_navigation_handle_id_;

  // Set after the navigation is finished and we're waiting for the page to be
  // ready sufficiently before marking the tool call finished.
  struct PostNavigationState {
    bool waiting_for_fcp = true;
    bool waiting_for_load = true;
    bool Done() const { return !waiting_for_fcp && !waiting_for_load; }
  };
  std::optional<PostNavigationState> post_navigation_state_;

  base::WeakPtrFactory<NavigateTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_H_
