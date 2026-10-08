// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/media_control_tool_request.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/media_control_tool.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

namespace {

// Default description for the `play_video` tool.
constexpr std::string_view kPlayMediaToolDescription = "Resume video playback.";

// Default description for the `pause_video` tool.
constexpr std::string_view kPauseMediaToolDescription = "Pause video playback.";

// Default description for the `seek_to_timestamp` tool.
constexpr std::string_view kSeekMediaToolDescription =
    "Jump the video to a specific timecode.";

// Description for the `timecode` parameter of the `seek_to_timestamp` tool.
constexpr std::string_view kSeekMediaTimecodeParamDescription =
    "The timecode to seek to, from the video transcript. Format: \"1:45\", "
    "\"0:30\", \"1:02:15\".";

ToolRequest::CreateToolResult CreateMediaControlTool(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    ActorSurfaceHandle actor_surface_handle,
    MediaControlTool::MediaControl media_control) {
  ActorSurface* actor_surface = actor_surface_handle.Get();
  if (!actor_surface) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }
  return {std::make_unique<MediaControlTool>(task_id, tool_delegate,
                                             *actor_surface, media_control),
          MakeOkResult()};
}

}  // namespace

PlayMediaToolRequest::PlayMediaToolRequest(tabs::TabHandle tab_handle)
    : TabToolRequest(tab_handle) {}

PlayMediaToolRequest::~PlayMediaToolRequest() = default;

// static
std::optional<ToolDefinition> PlayMediaToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kPlayVideo, kModelFacingName,
                               kPlayMediaToolDescription)
      .Build();
}

ToolRequest::CreateToolResult PlayMediaToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return CreateMediaControlTool(task_id, tool_delegate, GetActorSurfaceHandle(),
                                MediaControlTool::PlayMedia());
}

void PlayMediaToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view PlayMediaToolRequest::Name() const {
  return kName;
}

PauseMediaToolRequest::PauseMediaToolRequest(tabs::TabHandle tab_handle)
    : TabToolRequest(tab_handle) {}

PauseMediaToolRequest::~PauseMediaToolRequest() = default;

// static
std::optional<ToolDefinition> PauseMediaToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kPauseVideo, kModelFacingName,
                               kPauseMediaToolDescription)
      .Build();
}

ToolRequest::CreateToolResult PauseMediaToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return CreateMediaControlTool(task_id, tool_delegate, GetActorSurfaceHandle(),
                                MediaControlTool::PauseMedia());
}

void PauseMediaToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view PauseMediaToolRequest::Name() const {
  return kName;
}

// static
std::optional<base::TimeDelta> SeekMediaToolRequest::FromTimecode(
    std::string_view timecode) {
  std::vector<std::string_view> parts = base::SplitStringPiece(
      timecode, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  // Minutes and seconds following a ':' must be in the range [0, 60).
  auto is_sexagesimal = [](int value) { return value >= 0 && value < 60; };
  if (parts.size() == 1) {
    int s;
    if (base::StringToInt(parts[0], &s) && s >= 0) {
      return base::Seconds(s);
    }
  } else if (parts.size() == 2) {
    int m, s;
    if (base::StringToInt(parts[0], &m) && base::StringToInt(parts[1], &s) &&
        m >= 0 && is_sexagesimal(s)) {
      return base::Minutes(m) + base::Seconds(s);
    }
  } else if (parts.size() == 3) {
    int h, m, s;
    if (base::StringToInt(parts[0], &h) && base::StringToInt(parts[1], &m) &&
        base::StringToInt(parts[2], &s) && h >= 0 && is_sexagesimal(m) &&
        is_sexagesimal(s)) {
      return base::Hours(h) + base::Minutes(m) + base::Seconds(s);
    }
  }
  return std::nullopt;
}

SeekMediaToolRequest::SeekMediaToolRequest(tabs::TabHandle tab_handle,
                                           base::TimeDelta seek_time)
    : TabToolRequest(tab_handle), seek_time_(seek_time) {}

SeekMediaToolRequest::~SeekMediaToolRequest() = default;

// static
std::optional<ToolDefinition> SeekMediaToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kSeekToTimestamp, kModelFacingName,
                               kSeekMediaToolDescription)
      .SetToolParameterSchema(ToolSchemaBuilder().AddStringProperty(
          kTimecodeParam, kSeekMediaTimecodeParamDescription))
      .Build();
}

ToolRequest::CreateToolResult SeekMediaToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return CreateMediaControlTool(
      task_id, tool_delegate, GetActorSurfaceHandle(),
      MediaControlTool::SeekMedia{.seek_time = seek_time_});
}

void SeekMediaToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view SeekMediaToolRequest::Name() const {
  return kName;
}

}  // namespace actor
