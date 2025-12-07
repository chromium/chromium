// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/media_control_tool_request.h"

#include "chrome/browser/actor/tools/media_control_tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace actor {

namespace {

struct MediaControlNameVisitor {
  std::string operator()(const PlayMedia&) const { return "PlayMedia"; }
  std::string operator()(const PauseMedia&) const { return "PauseMedia"; }
  std::string operator()(const SeekMedia&) const { return "SeekMedia"; }
};

}  // namespace

std::string MediaControlName(const MediaControl& media_control) {
  return std::visit(MediaControlNameVisitor{}, media_control);
}

MediaControlToolRequest::MediaControlToolRequest(tabs::TabHandle tab_handle,
                                                 MediaControl media_control)
    : TabToolRequest(tab_handle), media_control_(media_control) {}

MediaControlToolRequest::~MediaControlToolRequest() = default;

ToolRequest::CreateToolResult MediaControlToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  tabs::TabInterface* tab = GetTabHandle().Get();
  if (!tab) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }
  return {std::make_unique<MediaControlTool>(task_id, tool_delegate, *tab,
                                             media_control_),
          MakeOkResult()};
}

void MediaControlToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view MediaControlToolRequest::Name() const {
  return kName;
}

std::string MediaControlToolRequest::JournalEvent() const {
  return absl::StrFormat("%s[%s]", Name(), MediaControlName(media_control_));
}

}  // namespace actor
