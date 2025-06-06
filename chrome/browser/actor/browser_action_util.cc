// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/browser_action_util.h"

#include "chrome/common/actor/actor_logging.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace actor {

using ::content::RenderFrameHost;
using ::content::WebContents;
using ::optimization_guide::DocumentIdentifierUserData;
using ::optimization_guide::proto::ActionInformation;
using ::optimization_guide::proto::ActionTarget;

namespace {

bool IsTargetingTab(const ActionInformation& action_information) {
  switch (action_information.action_info_case()) {
    case ActionInformation::ActionInfoCase::kClick:
    case ActionInformation::ActionInfoCase::kType:
    case ActionInformation::ActionInfoCase::kScroll:
    case ActionInformation::ActionInfoCase::kMoveMouse:
    case ActionInformation::ActionInfoCase::kDragAndRelease:
    case ActionInformation::ActionInfoCase::kSelect:
    // These actions target neither tabs nor frames.
    case ActionInformation::ActionInfoCase::kCreateTab:
    case ActionInformation::ActionInfoCase::kCloseTab:
    case ActionInformation::ActionInfoCase::kActivateTab:
    case ActionInformation::ActionInfoCase::kCreateWindow:
    case ActionInformation::ActionInfoCase::kCloseWindow:
    case ActionInformation::ActionInfoCase::kActivateWindow:
    case ActionInformation::ActionInfoCase::kYieldToUser:
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

}  // namespace

const ActionTarget* ExtractTarget(const ActionInformation& action_information) {
  switch (action_information.action_info_case()) {
    case ActionInformation::kClick:
      if (!action_information.click().has_target()) {
        return nullptr;
      }
      return &action_information.click().target();
    case ActionInformation::kType:
      if (!action_information.type().has_target()) {
        return nullptr;
      }
      return &action_information.type().target();
    case ActionInformation::kScroll:
      if (!action_information.scroll().has_target()) {
        return nullptr;
      }
      return &action_information.scroll().target();
    case ActionInformation::kMoveMouse:
      if (!action_information.move_mouse().has_target()) {
        return nullptr;
      }
      return &action_information.move_mouse().target();
    case ActionInformation::kDragAndRelease:
      if (!action_information.drag_and_release().has_from_target()) {
        return nullptr;
      }
      return &action_information.drag_and_release().from_target();
    case ActionInformation::kSelect:
      if (!action_information.select().has_target()) {
        return nullptr;
      }
      return &action_information.select().target();
    case ActionInformation::kNavigate:
    case ActionInformation::kBack:
    case ActionInformation::kForward:
    case ActionInformation::kWait:
    case ActionInformation::kCreateTab:
    case ActionInformation::kCloseTab:
    case ActionInformation::kActivateTab:
    case ActionInformation::kCreateWindow:
    case ActionInformation::kCloseWindow:
    case ActionInformation::kActivateWindow:
    case ActionInformation::kYieldToUser:
    case ActionInformation::ACTION_INFO_NOT_SET:
      return nullptr;
  }
}

RenderFrameHost* FindTargetFrame(WebContents& web_contents,
                                 const ActionInformation& action_information) {
  if (IsTargetingTab(action_information)) {
    return web_contents.GetPrimaryMainFrame();
  }

  const ActionTarget* target = ExtractTarget(action_information);

  if (target) {
    const std::string& serialized_token =
        target->document_identifier().serialized_token();

    RenderFrameHost* target_frame = nullptr;
    web_contents.ForEachRenderFrameHostWithAction(
        [&serialized_token, &target_frame](RenderFrameHost* rfh) {
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
  } else if (action_information.action_info_case() ==
             ActionInformation::kScroll) {
    // A scroll action may not have a target, which indicates scrolling the
    // main frame.
    return web_contents.GetPrimaryMainFrame();
  } else {
    ACTOR_LOG() << "Page-level BrowserAction did not specify an ActionTarget.";
    return nullptr;
  }
}

}  // namespace actor
