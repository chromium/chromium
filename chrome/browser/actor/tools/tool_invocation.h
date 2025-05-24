// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_INVOCATION_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_INVOCATION_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chrome/common/actor.mojom-forward.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace actor {

// A request to invoke a tool in the actor tool framework. Currently this just
// wraps an ActionInformation unpacked proto and provides some convenience
// methods.
class ToolInvocation {
 public:
  using ResultCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;

  explicit ToolInvocation(
      const optimization_guide::proto::ActionInformation& action_information,
      tabs::TabInterface& target_tab);

  ToolInvocation(const ToolInvocation& other) = default;

  // Not assignable because of the target_tab_ reference.
  ToolInvocation& operator=(const ToolInvocation& other) = delete;

  content::RenderFrameHost* FindTargetFrame() const;
  tabs::TabInterface* FindTargetTab() const;

  // Returns the target DOMNodeId for the requested action. Can only be called
  // if `IsTargetingPage` is true.
  int GetTargetDOMNodeId() const;

  // Whether the tool is a tab-level action.
  bool IsTargetingTab() const;

  // Whether the tool is a page-level action.
  bool IsTargetingPage() const;

  const optimization_guide::proto::ActionInformation& GetActionInfo() const;

 private:
  optimization_guide::proto::ActionInformation action_information_;

  // TODO(crbug.com/398849001): It'd be better if ActionInformation provided a
  // FrameInfo for non-page targeting actions but it currently doesn't so we
  // have to include the tab to use for tab-targeting actions.
  base::raw_ref<tabs::TabInterface> target_tab_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_INVOCATION_H_
