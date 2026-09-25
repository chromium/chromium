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
#include "chrome/browser/ttc/app/public/tool_types.h"
#include "components/tabs/public/tab_interface.h"

class Profile;

namespace ttc {

class TtcKeyedService;

class ToolController {
 public:
  // TODO(b/563468348): Change this to take a SessionController& instead once
  // we remove the AiOverlayTools codepath.
  explicit ToolController(TtcKeyedService& service);
  ~ToolController();

  void ProcessToolCall(const ToolRequest& tool_request,
                       ToolResponseCallback callback);

  // Returns the definitions of the tools this controller can execute, for
  // registration with the model backend.
  std::vector<ToolDefinition> GetToolDefinitions();

 private:
  Profile* GetProfile();

  // Creates the actor task used to invoke tools, if one isn't already active.
  void EnsureTaskCreated(actor::ActorKeyedService* actor_service);

#if !BUILDFLAG(IS_ANDROID)
  void OpenUrl(const base::DictValue& arguments, ToolResponseCallback callback);
  void PerformSearch(const base::DictValue& arguments,
                     ToolResponseCallback callback);
  void CloseCurrentTab(ToolResponseCallback callback);

  // Runs the tool request returned by `create_action` against the session's
  // active tab, replying to `callback` with the result. Replies with an error
  // if there's no window or tab to act on, or if the actor service is
  // unavailable, in which case `create_action` isn't invoked.
  void PerformActionOnActiveTab(
      base::FunctionRef<std::unique_ptr<actor::ToolRequest>(tabs::TabHandle)>
          create_action,
      ToolResponseCallback callback);

  void OnActionsFinished(
      ToolResponseCallback callback,
      std::vector<actor::ActionResultWithLatencyInfo> results,
      actor::TabObservationStrategy strategy);
#endif

  // Indirectly owns this object (via SessionController).
  const raw_ref<TtcKeyedService> service_;

  actor::TaskId task_id_;
  base::WeakPtrFactory<ToolController> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TOOL_CONTROLLER_H_
