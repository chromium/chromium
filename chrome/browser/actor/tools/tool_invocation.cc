// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_invocation.h"

#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

using content::RenderFrameHost;
using optimization_guide::proto::ActionInformation;
using tabs::TabInterface;

namespace actor {

ToolInvocation::ToolInvocation(const ActionInformation& action_information,
                               TabInterface& target_tab)
    : action_information_(action_information), target_tab_(target_tab) {}

RenderFrameHost* ToolInvocation::FindTargetFrame() const {
  // A foreground tab must have a web contents. When backgrounded, it is the
  // caller's responsibility to ensure contents aren't discarded.
  CHECK(target_tab_->GetContents());

  if (IsTargetingTab()) {
    return target_tab_->GetContents()->GetPrimaryMainFrame();
  }

  // TODO(crbug.com/402086380): action_target.frame_info() is currently empty.
  // This should be:
  // auto* rfh = RenderFrameHost::FromID(frame_info.process, frame_info.frame);
  // CHECK_EQ(rfh, target_tab_.GetPrimaryMainFrame())
  // return rfh;
  return target_tab_->GetContents()->GetPrimaryMainFrame();
}

TabInterface* ToolInvocation::FindTargetTab() const {
  // TODO(crbug.com/398849001): We should look-up the tab from the action_target
  // but since we can't yet find frames (see above TODO) always return the
  // focused web_contents (they should be the same for now anyway).
  return &target_tab_.get();
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
    case ActionInformation::ActionInfoCase::kWait:
      return true;
    case ActionInformation::ActionInfoCase::ACTION_INFO_NOT_SET:
      NOTREACHED();
  }
}

const ActionInformation& ToolInvocation::GetActionInfo() const {
  return action_information_;
}

}  // namespace actor
