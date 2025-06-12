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
using ::optimization_guide::proto::Action;
using ::optimization_guide::proto::ActionTarget;

namespace {

bool IsTargetingTab(const Action& action) {
  switch (action.action_case()) {
    case Action::ActionCase::kClick:
    case Action::ActionCase::kType:
    case Action::ActionCase::kScroll:
    case Action::ActionCase::kMoveMouse:
    case Action::ActionCase::kDragAndRelease:
    case Action::ActionCase::kSelect:
    // These actions target neither tabs nor frames.
    case Action::ActionCase::kCreateTab:
    case Action::ActionCase::kCloseTab:
    case Action::ActionCase::kActivateTab:
    case Action::ActionCase::kCreateWindow:
    case Action::ActionCase::kCloseWindow:
    case Action::ActionCase::kActivateWindow:
    case Action::ActionCase::kYieldToUser:
      return false;
    case Action::ActionCase::kNavigate:
    case Action::ActionCase::kBack:
    case Action::ActionCase::kForward:
    case Action::ActionCase::kWait:
      return true;
    case Action::ActionCase::ACTION_NOT_SET:
      NOTREACHED();
  }
}

}  // namespace

const ActionTarget* ExtractTarget(const Action& action) {
  switch (action.action_case()) {
    case Action::ActionCase::kClick:
      if (!action.click().has_target()) {
        return nullptr;
      }
      return &action.click().target();
    case Action::ActionCase::kType:
      if (!action.type().has_target()) {
        return nullptr;
      }
      return &action.type().target();
    case Action::ActionCase::kScroll:
      if (!action.scroll().has_target()) {
        return nullptr;
      }
      return &action.scroll().target();
    case Action::ActionCase::kMoveMouse:
      if (!action.move_mouse().has_target()) {
        return nullptr;
      }
      return &action.move_mouse().target();
    case Action::ActionCase::kDragAndRelease:
      if (!action.drag_and_release().has_from_target()) {
        return nullptr;
      }
      return &action.drag_and_release().from_target();
    case Action::ActionCase::kSelect:
      if (!action.select().has_target()) {
        return nullptr;
      }
      return &action.select().target();
    case Action::ActionCase::kNavigate:
    case Action::ActionCase::kBack:
    case Action::ActionCase::kForward:
    case Action::ActionCase::kWait:
    case Action::ActionCase::kCreateTab:
    case Action::ActionCase::kCloseTab:
    case Action::ActionCase::kActivateTab:
    case Action::ActionCase::kCreateWindow:
    case Action::ActionCase::kCloseWindow:
    case Action::ActionCase::kActivateWindow:
    case Action::ActionCase::kYieldToUser:
    case Action::ActionCase::ACTION_NOT_SET:
      return nullptr;
  }
}

RenderFrameHost* FindTargetFrame(WebContents& web_contents,
                                 const Action& action) {
  if (IsTargetingTab(action)) {
    return web_contents.GetPrimaryMainFrame();
  }

  const ActionTarget* target = ExtractTarget(action);

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
  } else if (action.action_case() == Action::kScroll) {
    // A scroll action may not have a target, which indicates scrolling the
    // main frame.
    return web_contents.GetPrimaryMainFrame();
  } else {
    ACTOR_LOG() << "Page-level BrowserAction did not specify an ActionTarget.";
    return nullptr;
  }
}

}  // namespace actor
