// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_SIWG_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_SIWG_CONTROLLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_metrics_helper.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permission_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class WebContents;

namespace webid {
enum class FederatedLoginResult;
}  // namespace webid
}  // namespace content

namespace actor_login {

// Controller for Sign-in with Google interaction.
// This class manages the federated login flow when user selects a federated
// credential.
class ActorLoginSiwgController : public content::WebContentsObserver {
 public:
  using GetPageContentProvider =
      base::RepeatingCallback<void(content::WebContents*,
                                   blink::mojom::AIPageContentOptionsPtr,
                                   optimization_guide::OnAIPageContentDone)>;

  ActorLoginSiwgController(
      content::WebContents* web_contents,
      const Credential& credential,
      bool should_store_permission,
      ActorLoginPermissionService& permission_service,
      LoginStatusResultOrErrorReply on_finished_callback,
      base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate,
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
      base::TimeTicks attempt_login_tool_start_time);
  ActorLoginSiwgController(
      content::WebContents* web_contents,
      const Credential& credential,
      GetPageContentProvider get_page_content_provider,
      bool should_store_permission,
      ActorLoginPermissionService& permission_service,
      LoginStatusResultOrErrorReply on_finished_callback,
      base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate,
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
      base::TimeTicks attempt_login_tool_start_time);
  ~ActorLoginSiwgController() override;

  // Not copyable or movable.
  ActorLoginSiwgController(const ActorLoginSiwgController&) = delete;
  ActorLoginSiwgController& operator=(const ActorLoginSiwgController&) = delete;

  bool should_store_permission() const { return should_store_permission_; }

  // Starts the federated login flow. This will notify FedCM API that an
  // automated login is in progress, and then start the button detection and
  // click flow.
  void StartFederatedLogin(
      std::unique_ptr<ActorLoginMetricsHelper> metrics_helper);

  // Starts the detection process for SiwG buttons on the current page and
  // clicks the first one found.
  void ClickSiwgButton();

  // Informs the controller about the success of a button click triggered by
  // the actor framework.
  void OnButtonClickCompleted(bool success);

 private:


  void OnFederatedLoginResultReceived(
      std::unique_ptr<ActorLoginMetricsHelper> metrics_helper,
      content::webid::FederatedLoginResult result);

  void LogFederatedLoginResult(content::webid::FederatedLoginResult result);

  GetPageContentProvider get_page_content_provider_;

  // Invoked once the actions taken by this class to advance the login are
  // complete. The login itself may still be in progress.
  LoginStatusResultOrErrorReply on_finished_callback_;
  // Delegate to notify once the login request initiated by this class produces
  // a result.
  base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate_;

  Credential credential_;
  // Passed from the attempt login tool when the user clicked "Allow always".
  bool should_store_permission_ = false;
  // `ProfileKeyedService`, will outlive this controller.
  const base::raw_ref<ActorLoginPermissionService> permission_service_;

  // Remote for the `ChromeRenderFrame` in the local root of the frame where the
  // SiwG button was found. Keeps the remote alive for the duration of the click
  // action.
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame_;

  // Details of the current federated login attempt.
  optimization_guide::proto::ActorLoginQuality_AttemptLoginDetails
      federated_attempt_login_details_;

  base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger_;
  base::TimeTicks attempt_login_tool_start_time_;

  base::WeakPtrFactory<ActorLoginSiwgController> weak_ptr_factory_{this};
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_SIWG_CONTROLLER_H_
