// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TOOL_CONTROLLER_H_
#define CHROME_BROWSER_TTC_CORE_TOOL_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/ttc/app/public/tool_types.h"

class Profile;

namespace actor {
class ToolRequest;
}  // namespace actor

namespace ttc {

class SessionControllerImpl;

// Executes the session's tool calls in an actor task.
class ToolController {
 public:
  explicit ToolController(SessionControllerImpl& session_controller);
  ~ToolController();

  void ProcessToolCall(const ToolRequest& tool_request,
                       ToolResponseCallback callback);

  // Returns the definitions of the tools this controller can execute, for
  // registration with the model backend.
  std::vector<ToolDefinition> GetToolDefinitions();

 private:
  Profile* GetProfile();

  void OpenUrl(const base::DictValue& arguments, ToolResponseCallback callback);
  void PerformSearch(const base::DictValue& arguments,
                     ToolResponseCallback callback);
  void GoBack(ToolResponseCallback callback);
  void GoForward(ToolResponseCallback callback);
  void ReloadPage(ToolResponseCallback callback);
  void FindAndHighlight(const base::DictValue& arguments,
                        ToolResponseCallback callback);
  void PlayVideo(ToolResponseCallback callback);
  void PauseVideo(ToolResponseCallback callback);
  void SeekToTimestamp(const base::DictValue& arguments,
                       ToolResponseCallback callback);
  void TranslatePage(const base::DictValue& arguments,
                     ToolResponseCallback callback);
  void ClickElement(const base::DictValue& arguments,
                    ToolResponseCallback callback);
  void SetText(const base::DictValue& arguments, ToolResponseCallback callback);
  void SelectOption(const base::DictValue& arguments,
                    ToolResponseCallback callback);

  // TODO(crbug.com/470475787): The actor tools backing these haven't been
  // ported to Android yet (see `skip_android_unmigrated_actor_files` in
  // //chrome/browser/actor/BUILD.gn), so they're neither declared to the model
  // nor dispatched there. Drop the guard once they are.
#if !BUILDFLAG(IS_ANDROID)
  void CloseCurrentTab(ToolResponseCallback callback);
  void SwitchTab(const base::DictValue& arguments,
                 ToolResponseCallback callback);
  void OpenKnownPage(const base::DictValue& arguments,
                     ToolResponseCallback callback);
  void SetFullscreen(const base::DictValue& arguments,
                     ToolResponseCallback callback);
#endif

  // Runs the tool request returned by `create_action` against the contents
  // tracked by the session (see
  // SessionControllerImpl::GetVoiceFocusedWebContents()), replying to
  // `callback` with the result. Replies with an error if there's no tab to act
  // on, in which case `create_action` isn't invoked.
  void PerformActionOnTrackedContents(
      base::FunctionRef<std::unique_ptr<actor::ToolRequest>(tabs::TabHandle)>
          create_action,
      ToolResponseCallback callback);

  // Runs `action` in the session's actor task and replies to `callback` with
  // the result.
  void PerformAction(std::unique_ptr<actor::ToolRequest> action,
                     ToolResponseCallback callback);

  void OnActionsFinished(
      ToolResponseCallback callback,
      std::vector<actor::ActionResultWithLatencyInfo> results,
      actor::TabObservationStrategy strategy);

  // Owns this object.
  const raw_ref<SessionControllerImpl> session_controller_;

  base::WeakPtrFactory<ToolController> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TOOL_CONTROLLER_H_
