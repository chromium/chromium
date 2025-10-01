// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_LOGIN_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_LOGIN_TOOL_H_

#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/common/actor_webui.mojom-forward.h"
#include "components/tabs/public/tab_interface.h"

class GURL;
namespace favicon_base {
struct FaviconImageResult;
}  // namespace favicon_base

namespace gfx {
class Image;
}  // namespace gfx

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
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      std::optional<ObservationDelayController::PageStabilityConfig>
          page_stability_config) override;
  void UpdateTaskBeforeInvoke(ActorTask& task,
                              InvokeCallback callback) const override;
  tabs::TabHandle GetTargetTab() const override;

 private:
  void OnGetCredentials(actor_login::CredentialsOrError credentials);
  void FetchIcons();
  void OnIconFetched(base::RepeatingClosure barrier,
                     GURL origin,
                     const favicon_base::FaviconImageResult& result);
  void OnAllIconsFetched();
  void OnCredentialSelected(
      webui::mojom::SelectCredentialDialogResponsePtr response);
  void OnAttemptLogin(actor_login::LoginStatusResultOrError login_status);

  actor_login::ActorLoginService& GetActorLoginService();

  // Holds the credentials after they are returned from the login service. The
  // credentials are cleared after the login attempt is made.
  std::vector<actor_login::Credential> credentials_;

  // Stores the icons for each unique `source_site_or_app` in `credentials_`.
  // Populated by `OnIconFetched()`.
  base::flat_map<std::string, gfx::Image> fetched_icons_;

  std::vector<base::CancelableTaskTracker> favicon_requests_tracker_;

  tabs::TabHandle tab_handle_;

  // Set on invocation. Used to check if the document changed during credential
  // selection.
  content::GlobalRenderFrameHostToken main_rfh_token_;

  InvokeCallback invoke_callback_;

  base::WeakPtrFactory<AttemptLoginTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_LOGIN_TOOL_H_
