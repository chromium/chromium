// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_invocation.h"

#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tab_collections/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

using content::RenderFrameHost;
using optimization_guide::proto::ActionInformation;
using optimization_guide::proto::ActionTarget;
using tabs::TabInterface;

namespace actor {

ToolInvocation::ToolInvocation(const ActionInformation& action_information,
                               TabInterface& target_tab)
    : action_information_(action_information), target_tab_(target_tab) {}

RenderFrameHost* ToolInvocation::FindTargetFrame() const {
  CHECK(IsTargetingPage());

  // A foreground tab must have a web contents. When backgrounded, it is the
  // caller's responsibility to ensure contents aren't discarded.
  CHECK(target_tab_.GetContents());

  // TODO(crbug.com/402086380): action_target.frame_info() is currently empty.
  // This should be:
  // auto* rfh = RenderFrameHost::FromID(frame_info.process, frame_info.frame);
  // CHECK_EQ(rfh, target_tab_.GetPrimaryMainFrame())
  // return rfh;
  return target_tab_.GetContents()->GetPrimaryMainFrame();
}

TabInterface* ToolInvocation::FindTargetTab() const {
  // TODO(crbug.com/398849001): We should look-up the tab from the action_target
  // but since we can't yet find frames (see above TODO) always return the
  // focused web_contents (they should be the same for now anyway).
  return &target_tab_;
}

int ToolInvocation::GetTargetDOMNodeId() const {
  CHECK(IsTargetingPage());
  return GetActionTarget().content_node_id();
}

const ActionTarget& ToolInvocation::GetActionTarget() const {
  switch (action_information_.action_info_case()) {
    case ActionInformation::ActionInfoCase::kClick:
      return action_information_.click().target();
    case ActionInformation::ActionInfoCase::kType:
      return action_information_.type().target();
    case ActionInformation::ActionInfoCase::kScroll:
      return action_information_.scroll().target();
    case ActionInformation::ActionInfoCase::kMoveMouse:
      return action_information_.move_mouse().target();
    case ActionInformation::ActionInfoCase::kDragAndRelease:
      // TODO(crbug.com/398849001): if from and to can differ we'll need
      // something something more sophisticated (this becomes tab-targeting).
      return action_information_.drag_and_release().from_target();
    case ActionInformation::ActionInfoCase::kSelect:
      return action_information_.select().target();
    case ActionInformation::ActionInfoCase::kNavigate:
    case ActionInformation::ActionInfoCase::kBack:
    case ActionInformation::ActionInfoCase::kForward:
    case ActionInformation::ActionInfoCase::ACTION_INFO_NOT_SET:
      NOTREACHED();
  }
}

bool ToolInvocation::IsTargetingPage() const {
  return !IsTargetingTab();
}

bool ToolInvocation::IsTargetingTab() const {
  switch (action_information_.action_info_case()) {
    case ActionInformation::ActionInfoCase::kClick:
    case ActionInformation::ActionInfoCase::kType:
    case ActionInformation::ActionInfoCase::kScroll:
    case ActionInformation::ActionInfoCase::kMoveMouse:
    case ActionInformation::ActionInfoCase::kDragAndRelease:
    case ActionInformation::ActionInfoCase::kSelect:
      return false;
    case ActionInformation::ActionInfoCase::kNavigate:
    case ActionInformation::ActionInfoCase::kBack:
    case ActionInformation::ActionInfoCase::kForward:
      return true;
    case ActionInformation::ActionInfoCase::ACTION_INFO_NOT_SET:
      NOTREACHED();
  }
}

}  // namespace actor
