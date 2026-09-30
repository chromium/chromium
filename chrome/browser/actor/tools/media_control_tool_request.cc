// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/media_control_tool_request.h"

#include <string_view>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "chrome/browser/actor/tools/media_control_tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace actor {

namespace {

struct MediaControlNameVisitor {
  std::string operator()(const PlayMedia&) const { return "PlayMedia"; }
  std::string operator()(const PauseMedia&) const { return "PauseMedia"; }
  std::string operator()(const SeekMedia&) const { return "SeekMedia"; }
};

}  // namespace

// static
std::optional<SeekMedia> SeekMedia::FromTimecode(std::string_view timecode) {
  std::vector<std::string_view> parts = base::SplitStringPiece(
      timecode, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  // Minutes and seconds following a ':' must be in the range [0, 60).
  auto is_sexagesimal = [](int value) { return value >= 0 && value < 60; };
  auto make_seek = [](base::TimeDelta seek_time) {
    return SeekMedia{.seek_time_milliseconds = seek_time.InMilliseconds()};
  };
  if (parts.size() == 1) {
    int s;
    if (base::StringToInt(parts[0], &s) && s >= 0) {
      return make_seek(base::Seconds(s));
    }
  } else if (parts.size() == 2) {
    int m, s;
    if (base::StringToInt(parts[0], &m) && base::StringToInt(parts[1], &s) &&
        m >= 0 && is_sexagesimal(s)) {
      return make_seek(base::Minutes(m) + base::Seconds(s));
    }
  } else if (parts.size() == 3) {
    int h, m, s;
    if (base::StringToInt(parts[0], &h) && base::StringToInt(parts[1], &m) &&
        base::StringToInt(parts[2], &s) && h >= 0 && is_sexagesimal(m) &&
        is_sexagesimal(s)) {
      return make_seek(base::Hours(h) + base::Minutes(m) + base::Seconds(s));
    }
  }
  return std::nullopt;
}

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
