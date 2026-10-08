// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/media_control_tool.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "base/check.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace actor {

namespace {

struct MediaControlNameVisitor {
  std::string_view operator()(const MediaControlTool::PlayMedia&) const {
    return "PlayMedia";
  }
  std::string_view operator()(const MediaControlTool::PauseMedia&) const {
    return "PauseMedia";
  }
  std::string_view operator()(const MediaControlTool::SeekMedia&) const {
    return "SeekMedia";
  }
};

std::string_view MediaControlName(
    const MediaControlTool::MediaControl& media_control) {
  return std::visit(MediaControlNameVisitor{}, media_control);
}

}  // namespace

MediaControlTool::MediaControlTool(TaskId task_id,
                                   ToolDelegate& tool_delegate,
                                   ActorSurface& actor_surface,
                                   MediaControl media_control)
    : Tool(task_id, tool_delegate),
      actor_surface_handle_(actor_surface.GetHandle()),
      media_control_(media_control) {}

MediaControlTool::~MediaControlTool() = default;

void MediaControlTool::Validate(ToolCallback callback) {
  PostResponseTask(std::move(callback), MakeOkResult());
}

void MediaControlTool::Invoke(ToolCallback callback) {
  ActorSurface* actor_surface = actor_surface_handle_.Get();
  if (!actor_surface) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  // Get the media session associated with the surface's web contents.
  CHECK(actor_surface->GetWebContents());
  content::MediaSession* media_session =
      content::MediaSession::GetIfExists(actor_surface->GetWebContents());
  if (!media_session) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kMediaControlNoMedia));
    return;
  }

  // Invoke the appropriate media control action.
  std::visit(
      absl::Overload(
          [media_session](const PlayMedia& arg) {
            // Resume media playback.
            media_session->Resume(content::MediaSession::SuspendType::kSystem);
          },
          [media_session](const PauseMedia& arg) {
            // Suspend media playback.
            media_session->Suspend(content::MediaSession::SuspendType::kSystem);
          },
          [media_session](const SeekMedia& arg) {
            // Seek to a specific time in the media.
            auto media_position = media_session->GetMediaSessionPosition();
            if (arg.seek_time >= base::Seconds(0) && media_position &&
                arg.seek_time <= media_position->duration()) {
              media_session->SeekTo(arg.seek_time);
            }
          }),
      media_control_);

  // Post a task to run the callback with a success result.
  PostResponseTask(std::move(callback), MakeOkResult());
}

std::string MediaControlTool::DebugString() const {
  return absl::StrFormat("MediaControlTool[%s]",
                         MediaControlName(media_control_));
}

std::string MediaControlTool::JournalEvent() const {
  return std::string(MediaControlName(media_control_));
}

std::unique_ptr<ObservationDelayController>
MediaControlTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  ActorSurface* actor_surface = actor_surface_handle_.Get();
  if (!actor_surface) {
    return nullptr;
  }
  return std::make_unique<ObservationDelayController>(
      *actor_surface->GetWebContents()->GetPrimaryMainFrame(), task_id(),
      journal(), page_stability_config);
}

void MediaControlTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                              ToolCallback callback) const {
  task.AddActorSurface(actor_surface_handle_, /*stop_task_on_detach=*/true,
                       std::move(callback));
}

ActorSurfaceHandle MediaControlTool::GetTargetActorSurface() const {
  return actor_surface_handle_;
}

}  // namespace actor
