// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_MEDIA_CONTROL_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_MEDIA_CONTROL_TOOL_REQUEST_H_

#include <variant>

#include "chrome/browser/actor/tools/tool_request.h"

namespace actor {

// A media control action to start or resume media playback.
struct PlayMedia {};

// A media control action to pause media playback.
struct PauseMedia {};

// A media control action to seek to a specific time in the media.
struct SeekMedia {
  int64_t seek_time_microseconds;
};

// A variant that holds one of several possible media control actions.
using MediaControl = std::variant<PlayMedia, PauseMedia, SeekMedia>;

// Returns the name of the media control variant.
std::string MediaControlName(const MediaControl& media_control);

// A tool request for performing a media control action on a specific tab.
class MediaControlToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "MediaControl";

  explicit MediaControlToolRequest(tabs::TabHandle tab_handle,
                                   MediaControl media_control);
  ~MediaControlToolRequest() override;

  // TabToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;
  std::string JournalEvent() const override;

 private:
  MediaControl media_control_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_MEDIA_CONTROL_TOOL_REQUEST_H_
