// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_MEDIA_CONTROL_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_MEDIA_CONTROL_TOOL_H_

#include <memory>
#include <string>
#include <variant>

#include "base/time/time.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"

namespace actor {

class ActorSurface;

class MediaControlTool : public Tool {
 public:
  // A media control action to start or resume media playback.
  struct PlayMedia {};

  // A media control action to pause media playback.
  struct PauseMedia {};

  // A media control action to seek to a specific time in the media.
  struct SeekMedia {
    base::TimeDelta seek_time;
  };

  // A variant that holds one of several possible media control actions.
  using MediaControl = std::variant<PlayMedia, PauseMedia, SeekMedia>;

  MediaControlTool(TaskId task_id,
                   ToolDelegate& tool_delegate,
                   ActorSurface& actor_surface,
                   MediaControl media_control);
  ~MediaControlTool() override;

  // Tool:
  void Validate(ToolCallback callback) override;
  void Invoke(ToolCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  void UpdateTaskBeforeInvoke(ActorTask& task,
                              ToolCallback callback) const override;
  ActorSurfaceHandle GetTargetActorSurface() const override;

 private:
  ActorSurfaceHandle actor_surface_handle_;
  MediaControl media_control_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_MEDIA_CONTROL_TOOL_H_
