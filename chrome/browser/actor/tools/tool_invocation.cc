// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_invocation.h"

#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

using content::RenderFrameHost;
using optimization_guide::DocumentIdentifierUserData;
using optimization_guide::proto::ActionInformation;
using optimization_guide::proto::ActionTarget;
using tabs::TabInterface;

namespace actor {

namespace {

const ActionTarget* ExtractTarget(const ActionInformation& action_information) {
  switch (action_information.action_info_case()) {
    case ActionInformation::kClick:
      return &action_information.click().target();
    case ActionInformation::kType:
      return &action_information.type().target();
    case ActionInformation::kScroll:
      return &action_information.scroll().target();
    case ActionInformation::kMoveMouse:
      return &action_information.move_mouse().target();
    case ActionInformation::kDragAndRelease:
      return &action_information.drag_and_release().from_target();
    case ActionInformation::kSelect:
      return &action_information.select().target();
    case ActionInformation::kNavigate:
    case ActionInformation::kBack:
    case ActionInformation::kForward:
    case ActionInformation::kWait:
    case ActionInformation::ACTION_INFO_NOT_SET:
      return nullptr;
  }
}

}  // namespace

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

  const ActionTarget* target = ExtractTarget(action_information_);
  CHECK(target);

  std::string serialized_token =
      target->document_identifier().serialized_token();

  RenderFrameHost* target_frame = nullptr;
  target_tab_->GetContents()
      ->GetPrimaryMainFrame()
      ->ForEachRenderFrameHostWithAction([&serialized_token,
                                          &target_frame](RenderFrameHost* rfh) {
        auto* user_data =
            DocumentIdentifierUserData::GetForCurrentDocument(rfh);
        if (user_data && user_data->serialized_token() == serialized_token) {
          // If the frame is inactive it shouldn't be targeted for tool use.
          if (rfh->IsActive()) {
            target_frame = rfh;
          }
          return RenderFrameHost::FrameIterationAction::kStop;
        }
        return RenderFrameHost::FrameIterationAction::kContinue;
      });

  return target_frame;
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
