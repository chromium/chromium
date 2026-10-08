// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_MEDIA_CONTROL_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_MEDIA_CONTROL_TOOL_REQUEST_H_

#include <optional>
#include <string_view>

#include "base/time/time.h"
#include "chrome/browser/actor/tools/tool_request.h"

namespace actor {
class ToolRequestVisitorFunctor;

// Starts or resumes media playback in a specific tab.
class PlayMediaToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "PlayMedia";

  explicit PlayMediaToolRequest(tabs::TabHandle tab_handle);
  ~PlayMediaToolRequest() override;

  // TabToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;
};

// Pauses media playback in a specific tab.
class PauseMediaToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "PauseMedia";

  explicit PauseMediaToolRequest(tabs::TabHandle tab_handle);
  ~PauseMediaToolRequest() override;

  // TabToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;
};

// Seeks to a specific time in the media in a specific tab.
class SeekMediaToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "SeekMedia";
  // Canonical model-facing name exposed in LLM prompt schemas and indexed by
  // `ToolRegistry` for lookup and routing via `GetToolDefinition()`.
  static constexpr std::string_view kModelFacingName = "seek_to_timestamp";
  // JSON argument key for the target timecode parameter.
  static constexpr std::string_view kTimecodeParam = "timecode";

  // Parses a timecode of the form "S", "M:SS" or "H:MM:SS" (e.g. "30", "1:45",
  // "1:02:15"). Returns std::nullopt if `timecode` is malformed.
  static std::optional<base::TimeDelta> FromTimecode(std::string_view timecode);

  SeekMediaToolRequest(tabs::TabHandle tab_handle, base::TimeDelta seek_time);
  ~SeekMediaToolRequest() override;

  // Returns the `ToolId::kSeekToTimestamp` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  // TabToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

 private:
  base::TimeDelta seek_time_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_MEDIA_CONTROL_TOOL_REQUEST_H_
