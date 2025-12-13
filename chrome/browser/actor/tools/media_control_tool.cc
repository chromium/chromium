// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/media_control_tool.h"

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor/action_result.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace actor {

namespace {

content::RenderFrameHost& GetPrimaryMainFrameOfTab(tabs::TabHandle tab_handle) {
  return *tab_handle.Get()->GetContents()->GetPrimaryMainFrame();
}

}  // namespace

MediaControlTool::MediaControlTool(TaskId task_id,
                                   ToolDelegate& tool_delegate,
                                   tabs::TabInterface& tab,
                                   MediaControl media_control)
    : Tool(task_id, tool_delegate),
      tab_handle_(tab.GetHandle()),
      media_control_(media_control) {}

MediaControlTool::~MediaControlTool() = default;

void MediaControlTool::Validate(ToolCallback callback) {
  PostResponseTask(std::move(callback), MakeOkResult());
}

void MediaControlTool::Invoke(ToolCallback callback) {
  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  // Get the media session associated with the tab's web contents.
  CHECK(tab->GetContents());
  content::MediaSession* media_session =
      content::MediaSession::GetIfExists(tab->GetContents());
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
            media_session->Resume(content::MediaSession::SuspendType::kUI);
          },
          [media_session](const PauseMedia& arg) {
            // Suspend media playback.
            media_session->Suspend(content::MediaSession::SuspendType::kUI);
          },
          [media_session](const SeekMedia& arg) {
            // Seek to a specific time in the media.
            media_session->SeekTo(
                base::Microseconds(arg.seek_time_microseconds));
          }),
      media_control_);

  // Post a task to run the callback with a success result.
  PostResponseTask(std::move(callback), MakeOkResult());
}

std::string MediaControlTool::DebugString() const {
  return absl::StrFormat("MediaControlTool[%s]", JournalEvent());
}

std::string MediaControlTool::JournalEvent() const {
  return MediaControlName(media_control_);
}

std::unique_ptr<ObservationDelayController>
MediaControlTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  if (!tab_handle_.Get()) {
    return nullptr;
  }
  return std::make_unique<ObservationDelayController>(
      GetPrimaryMainFrameOfTab(tab_handle_), task_id(), journal(),
      page_stability_config);
}

void MediaControlTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                              ToolCallback callback) const {
  task.AddTab(tab_handle_, std::move(callback));
}

tabs::TabHandle MediaControlTool::GetTargetTab() const {
  return tab_handle_;
}

}  // namespace actor
