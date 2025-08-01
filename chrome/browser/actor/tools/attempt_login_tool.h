// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_LOGIN_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_LOGIN_TOOL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

class AttemptLoginTool : public Tool {
 public:
  AttemptLoginTool(TaskId task_id,
                   ToolDelegate& tool_delegate,
                   tabs::TabInterface& tab);
  ~AttemptLoginTool() override;

  // actor::Tool
  void Validate(ValidateCallback callback) override;
  void Invoke(InvokeCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer()
      const override;
  void UpdateTaskBeforeInvoke(ActorTask& task,
                              InvokeCallback callback) const override;

 private:
  void OnGetCredentials(actor_login::CredentialsOrError credentials);
  void OnAttemptLogin(actor_login::LoginStatusResultOrError login_status);

  actor_login::ActorLoginService& GetActorLoginService();

  tabs::TabHandle tab_handle_;

  InvokeCallback invoke_callback_;

  base::WeakPtrFactory<AttemptLoginTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_LOGIN_TOOL_H_
