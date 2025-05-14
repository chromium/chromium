// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_HISTORY_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_HISTORY_TOOL_H_

#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/common/actor.mojom-forward.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

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
  void FinishToolInvocationIfNeeded(mojom::ActionResultPtr result);

  void PurgePendingNavigations();

  bool IsInvokeInProgress() const;

  // Whether the navigation is backwards or forwards in session history.
  Direction direction_;

  // This class tracks all navigation handles created as a result of the history
  // traversal in `pending_navigations_`. However, these navigations may or may
  // not start. When they are started (or canceled), they are removed from this
  // vector. Started navigations that are added to `in_flight_navigation_ids_`.
  // Note: these are vector/set since a history traversal can navigate multiple
  // frames.
  content::NavigationController::WeakNavigationHandleVector
      pending_navigations_;
  absl::flat_hash_set<uint64_t> in_flight_navigation_ids_;

  // Holds the callback to the Invoke method. Null before invoke is called.
  InvokeCallback invoke_callback_;

  base::WeakPtrFactory<HistoryTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_HISTORY_TOOL_H_
