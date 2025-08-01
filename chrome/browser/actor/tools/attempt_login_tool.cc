// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_login_tool.h"

#include "base/notimplemented.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/common/actor/action_result.h"
#include "content/public/browser/web_contents.h"

// TODO(crbug.com/427817201): Throughout this file replace
// ActionResultCode::kError with new error codes.

namespace actor {

namespace {

content::RenderFrameHost& GetPrimaryMainFrameOfTab(tabs::TabHandle tab_handle) {
  return *tab_handle.Get()->GetContents()->GetPrimaryMainFrame();
}

mojom::ActionResultCode LoginErrorToActorError(
    actor_login::ActorLoginError login_error) {
  switch (login_error) {
    case actor_login::ActorLoginError::kServiceBusy:
      return mojom::ActionResultCode::kError;
    case actor_login::ActorLoginError::kInvalidTabInterface:
      return mojom::ActionResultCode::kTabWentAway;
    case actor_login::ActorLoginError::kUnknown:
    default:
      return mojom::ActionResultCode::kError;
  }
}

mojom::ActionResultCode LoginResultToActorResult(
    actor_login::LoginStatusResult login_result) {
  switch (login_result) {
    case actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled:
      return mojom::ActionResultCode::kOk;
    case actor_login::LoginStatusResult::kErrorNoSigninForm:
      return mojom::ActionResultCode::kError;
  }
}

}  // namespace

AttemptLoginTool::AttemptLoginTool(TaskId task_id,
                                   ToolDelegate& tool_delegate,
                                   tabs::TabInterface& tab)
    : Tool(task_id, tool_delegate), tab_handle_(tab.GetHandle()) {}

AttemptLoginTool::~AttemptLoginTool() = default;

void AttemptLoginTool::Validate(ValidateCallback callback) {
  PostResponseTask(std::move(callback), MakeOkResult());
}

void AttemptLoginTool::Invoke(InvokeCallback callback) {
  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  invoke_callback_ = std::move(callback);
  GetActorLoginService().GetCredentials(
      tab, base::BindOnce(&AttemptLoginTool::OnGetCredentials,
                          weak_ptr_factory_.GetWeakPtr()));
}

void AttemptLoginTool::OnGetCredentials(
    actor_login::CredentialsOrError credentials) {
  if (!credentials.has_value()) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(LoginErrorToActorError(credentials.error())));
    return;
  }

  std::vector<actor_login::Credential> creds = credentials.value();
  if (creds.empty()) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(mojom::ActionResultCode::kError));
    return;
  }

  std::erase_if(creds, [](const actor_login::Credential& cred) {
    return !cred.immediatelyAvailableToLogin;
  });
  if (creds.empty()) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(mojom::ActionResultCode::kError));
    return;
  }

  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  // TODO(crbug.com/427817882): Ask the client to choose the credential.
  GetActorLoginService().AttemptLogin(
      tab, creds.front(),
      base::BindOnce(&AttemptLoginTool::OnAttemptLogin,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AttemptLoginTool::OnAttemptLogin(
    actor_login::LoginStatusResultOrError login_status) {
  if (!login_status.has_value()) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(LoginErrorToActorError(login_status.error())));
    return;
  }

  PostResponseTask(std::move(invoke_callback_),
                   MakeResult(LoginResultToActorResult(login_status.value())));
}

std::string AttemptLoginTool::DebugString() const {
  return "AttemptLoginTool";
}

std::string AttemptLoginTool::JournalEvent() const {
  return "AttemptLogin";
}

std::unique_ptr<ObservationDelayController>
AttemptLoginTool::GetObservationDelayer() const {
  return std::make_unique<ObservationDelayController>(
      GetPrimaryMainFrameOfTab(tab_handle_));
}

void AttemptLoginTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                              InvokeCallback callback) const {
  task.AddTab(tab_handle_, std::move(callback));
}

actor_login::ActorLoginService& AttemptLoginTool::GetActorLoginService() {
  return tool_delegate().GetActorLoginService();
}

}  // namespace actor
