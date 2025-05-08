// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_HISTORY_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_HISTORY_TOOL_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/tools/tool.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace actor {

// Performs a history navigation in a tab.
class HistoryTool : public Tool, content::WebContentsObserver {
 public:
  enum Direction {
    kBack,
    kForward,
  };

  HistoryTool(tabs::TabInterface& tab, Direction direction);
  ~HistoryTool() override;

  // actor::Tool
  void Validate(ValidateCallback callback) override;
  void Invoke(InvokeCallback callback) override;
  std::string DebugString() const override;

  // content::WebContentsObserver
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  void FinishToolInvocationIfNeeded(bool result);

  void LegacyBrowserBasedBeforeUnloadReplyComplete();

  // Whether the navigation is backwards or forwards in session history.
  Direction direction_;

  // When true, WebContentsObserver::DidStartNavigation will collect navigation
  // handles in navigation_handle_ids_.
  bool is_collecting_new_navigations_ = false;

  // IDs of navigations tracked as a result of invoking this tool. This is a set
  // because a history navigation can lead to multiple frames being navigated.
  base::flat_set<uint64_t> navigation_handle_ids_;

  // Holds the callback to the Invoke method. Null before invoke is called.
  InvokeCallback invoke_callback_;

  base::WeakPtrFactory<HistoryTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_HISTORY_TOOL_H_
