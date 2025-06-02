// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/browser_action_util.h"

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

RenderFrameHost* FindTargetFrame(WebContents& web_contents,
                                 const ActionInformation& action_information) {
  if (IsTargetingTab(action_information)) {
    return web_contents.GetPrimaryMainFrame();
  }

  const ActionTarget* target = ExtractTarget(action_information);
  CHECK(target);

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
}

}  // namespace actor
