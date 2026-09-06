// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

Tool::Tool(TaskId task_id, ToolDelegate& tool_delegate)
    : task_id_(task_id), tool_delegate_(tool_delegate) {}
Tool::~Tool() = default;

mojom::ActionResultPtr Tool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  return MakeOkResult();
}

void Tool::Cancel() {}

void Tool::NotifyPaused() {}

GURL Tool::JournalURL() const {
  return GURL::EmptyGURL();
}

void Tool::UpdateTaskBeforeInvoke(ActorTask& task,
                                  ToolCallback callback) const {
  // Do nothing by default, just trigger the callback.
  std::move(callback).Run(MakeOkResult());
}

void Tool::UpdateTaskAfterInvoke(ActorTask& task,
                                 mojom::ActionResultPtr result,
                                 ToolCallback callback) const {
  // Do nothing by default, just trigger the callback.
  std::move(callback).Run(std::move(result));
}

#if !BUILDFLAG(IS_ANDROID)
mojom::ActionResultPtr Tool::ValidateBrowserWindow(
    const BrowserWindowInterface* browser) const {
  if (!browser) {
    return MakeResult(mojom::ActionResultCode::kWindowWentAway,
                      /*requires_page_stabilization=*/false,
                      "The target window could not be found.");
  }
  if (browser->GetProfile() != &tool_delegate_->GetProfile()) {
    return MakeResult(mojom::ActionResultCode::kActionTargetCrossProfile,
                      /*requires_page_stabilization=*/false,
                      "The target window belongs to a different profile.");
  }
  return MakeOkResult();
}

mojom::ActionResultPtr Tool::ValidateWindowId(SessionID window_id) const {
  return ValidateBrowserWindow(
      BrowserWindowInterface::FromSessionID(window_id));
}
#endif

}  // namespace actor
