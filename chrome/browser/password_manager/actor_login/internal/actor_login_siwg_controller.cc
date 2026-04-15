// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_siwg_controller.h"

#include "base/barrier_callback.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace actor_login {

namespace {

AttemptLoginOutcomeMqls FromFederatedLoginResultToMqls(
    content::webid::FederatedLoginResult result) {
  switch (result) {
    case content::webid::FederatedLoginResult::kSuccess:
      return AttemptLoginOutcomeMqls::kFederatedSuccess;
    case content::webid::FederatedLoginResult::kContinuation:
      return AttemptLoginOutcomeMqls::kFederatedContinuation;
    case content::webid::FederatedLoginResult::kAccountNotLoggedIn:
      return AttemptLoginOutcomeMqls::kFederatedAccountNotLoggedIn;
    case content::webid::FederatedLoginResult::kAccountIsSignUp:
      return AttemptLoginOutcomeMqls::kFederatedAccountIsSignUp;
    case content::webid::FederatedLoginResult::kAccountNotAvailable:
      return AttemptLoginOutcomeMqls::kFederatedAccountIsNotAvailable;
    case content::webid::FederatedLoginResult::kIdpReturnedError:
      return AttemptLoginOutcomeMqls::kFederatedIdpReturnedError;
    case content::webid::FederatedLoginResult::kIdpNetworkError:
      return AttemptLoginOutcomeMqls::kFederatedIdpNetworkError;
    case content::webid::FederatedLoginResult::kTokenRequestAborted:
      return AttemptLoginOutcomeMqls::kFederatedTokenRequestAborted;
    case content::webid::FederatedLoginResult::kFrameNotActive:
      return AttemptLoginOutcomeMqls::kFederatedFrameNotActive;
    case content::webid::FederatedLoginResult::kExpectedAccountNotPresent:
      return AttemptLoginOutcomeMqls::kFederatedExpectedAccountNotPresent;
    case content::webid::FederatedLoginResult::kTimeout:
    case content::webid::FederatedLoginResult::kTimeoutByEmbedder:
      return AttemptLoginOutcomeMqls::kFederatedTimeout;
  }
}

// Finds the local root of a given RenderFrameHost.
content::RenderFrameHost* GetLocalRoot(content::RenderFrameHost* rfh) {
  content::RenderFrameHost* local_root = rfh;
  while (local_root && local_root->GetParent()) {
    if (local_root->GetRenderWidgetHost() !=
        local_root->GetParent()->GetRenderWidgetHost()) {
      break;
    }
    local_root = local_root->GetParent();
  }
  return local_root;
}

LoginStatusResult FromFederatedLoginResult(
    content::webid::FederatedLoginResult result) {
  switch (result) {
    case content::webid::FederatedLoginResult::kSuccess:
      return LoginStatusResult::kSuccessFederated;
    case content::webid::FederatedLoginResult::kContinuation:
      return LoginStatusResult::kErrorFederatedContinuation;
    case content::webid::FederatedLoginResult::kAccountNotLoggedIn:
      return LoginStatusResult::kErrorFederatedAccountNotLoggedIn;
    case content::webid::FederatedLoginResult::kAccountIsSignUp:
      return LoginStatusResult::kErrorFederatedAccountIsSignUp;
    case content::webid::FederatedLoginResult::kAccountNotAvailable:
      return LoginStatusResult::kErrorFederatedAccountNotAvailable;
    case content::webid::FederatedLoginResult::kIdpReturnedError:
      return LoginStatusResult::kErrorFederatedIdpReturnedError;
    case content::webid::FederatedLoginResult::kIdpNetworkError:
      return LoginStatusResult::kErrorFederatedIdpNetworkError;
    case content::webid::FederatedLoginResult::kTokenRequestAborted:
      return LoginStatusResult::kErrorFederatedTokenRequestAborted;
    case content::webid::FederatedLoginResult::kFrameNotActive:
      return LoginStatusResult::kErrorFederatedFrameNotActive;
    case content::webid::FederatedLoginResult::kExpectedAccountNotPresent:
      return LoginStatusResult::kErrorFederatedExpectedAccountNotPresent;
    case content::webid::FederatedLoginResult::kTimeout:
    case content::webid::FederatedLoginResult::kTimeoutByEmbedder:
      return LoginStatusResult::kErrorFederatedTimeout;
  }
  NOTREACHED();
}

}  // namespace

ActorLoginSiwgController::ActorLoginSiwgController(
    content::WebContents* web_contents,
    const Credential& credential,
    bool should_store_permission,
    ActorLoginPermissionService& permission_service,
    LoginStatusResultOrErrorReply on_finished_callback,
    base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate,
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    base::TimeTicks attempt_login_tool_start_time)
    : ActorLoginSiwgController(
          web_contents,
          credential,
          base::BindRepeating(&optimization_guide::GetAIPageContent),
          should_store_permission,
          permission_service,
          std::move(on_finished_callback),
          std::move(action_sequence_delegate),
          std::move(mqls_logger),
          attempt_login_tool_start_time) {}

ActorLoginSiwgController::ActorLoginSiwgController(
    content::WebContents* web_contents,
    const Credential& credential,
    GetPageContentProvider get_page_content_provider,
    bool should_store_permission,
    ActorLoginPermissionService& permission_service,
    LoginStatusResultOrErrorReply on_finished_callback,
    base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate,
    base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
    base::TimeTicks attempt_login_tool_start_time)
    : content::WebContentsObserver(web_contents),
      get_page_content_provider_(std::move(get_page_content_provider)),
      on_finished_callback_(std::move(on_finished_callback)),
      action_sequence_delegate_(std::move(action_sequence_delegate)),
      credential_(credential),
      should_store_permission_(should_store_permission),
      permission_service_(permission_service),
      mqls_logger_(std::move(mqls_logger)),
      attempt_login_tool_start_time_(attempt_login_tool_start_time) {}

ActorLoginSiwgController::~ActorLoginSiwgController() = default;

void ActorLoginSiwgController::StartFederatedLogin(
    std::unique_ptr<ActorLoginMetricsHelper> metrics_helper) {
  CHECK(credential_.federation_detail);

  auto* source = content::webid::IdentityCredentialSource::FromPage(
      web_contents()->GetPrimaryPage());

  auto* metrics_helper_raw = metrics_helper.get();
  source->SetEmbedderLoginRequest(
      credential_.federation_detail->idp_origin,
      credential_.federation_detail->account_id,
      base::BindOnce(&ActorLoginSiwgController::OnFederatedLoginResultReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(metrics_helper)));

  // There may be an existing FedCM dialog; if so, select an account in that
  // dialog instead of clicking the signin button.
  if (metrics_helper_raw) {
    metrics_helper_raw->RecordFederatedHangingFedCmRequestExists(
        source->HasPendingRequest());
  }
  if (!source->SelectAccount(credential_.federation_detail->idp_origin,
                             credential_.federation_detail->account_id)) {
    if (base::FeatureList::IsEnabled(
            password_manager::features::kActorLoginFederatedClickFromActor)) {
      federated_attempt_login_details_.set_button_click_required(true);
      std::move(on_finished_callback_)
          .Run(LoginStatusResult::kRequiresButtonClick);
    } else {
      ClickSiwgButton();
    }
  }
}

void ActorLoginSiwgController::ClickSiwgButton() {
  get_page_content_provider_.Run(
      web_contents(),
      optimization_guide::ActionableAIPageContentOptions(
          /*on_critical_path=*/false),
      base::BindOnce(&ActorLoginSiwgController::OnPageContentReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ActorLoginSiwgController::OnFederatedLoginResultReceived(
    std::unique_ptr<ActorLoginMetricsHelper> metrics_helper,
    content::webid::FederatedLoginResult result) {
  if (metrics_helper) {
    if (result == content::webid::FederatedLoginResult::kContinuation) {
      metrics_helper->RecordFederatedContinuationShown();
    } else {
      metrics_helper->RecordFederatedLoginResult(result);
    }
  }

  if (result == content::webid::FederatedLoginResult::kSuccess &&
      should_store_permission_) {
    FederatedPermission permission;
    permission.idp_origin = credential_.federation_detail->idp_origin;
    permission.rp_embedder_origin = credential_.request_origin;
    // Assuming identical to rp_embedder_origin since cross-origin iframes
    // aren't supported.
    permission.rp_requester_origin = permission.rp_embedder_origin;
    permission.chosen_account_id = credential_.federation_detail->account_id;
    permission.chosen_account_email = base::UTF16ToUTF8(credential_.username);

    // `DoNothing()` for the response callback because there is nothing we can
    // do with failed requests.
    permission_service_->GrantPermission(permission, base::DoNothing());
  }

  LogFederatedLoginResult(result);
  LoginStatusResult status = FromFederatedLoginResult(result);
  if (action_sequence_delegate_) {
    action_sequence_delegate_->OnFederatedLoginOutcome(status);
  }
  if (on_finished_callback_) {
    std::move(on_finished_callback_).Run(status);
  }
}

void ActorLoginSiwgController::OnButtonClickCompleted(bool success) {
  federated_attempt_login_details_.set_button_click_succeeded(success);
}

void ActorLoginSiwgController::LogFederatedLoginResult(
    content::webid::FederatedLoginResult result) {
  if (!mqls_logger_) {
    return;
  }

  federated_attempt_login_details_.set_outcome(
      OutcomeEnumToProtoType(FromFederatedLoginResultToMqls(result)));
  federated_attempt_login_details_.set_attempt_login_time_ms(
      (base::TimeTicks::Now() - attempt_login_tool_start_time_)
          .InMilliseconds());
  mqls_logger_->AddAttemptLoginDetails(federated_attempt_login_details_);
}

void ActorLoginSiwgController::OnPageContentReceived(
    optimization_guide::AIPageContentResultOrError content) {
  if (!content.has_value()) {
    if (on_finished_callback_) {
      std::move(on_finished_callback_)
          // TODO(crbug.com/478799141): add new status for SiwG failure.
          .Run(base::unexpected(ActorLoginError::kFillingNotAllowed));
    }
    return;
  }

  // Move the page content directly into the finder.
  siwg_finder_ = std::make_unique<SiwgButtonFinder>(std::move(content->proto));

  // This will find 0 or 1 buttons per frame and click each one found. If we
  // ever want to productionize this code, we should add some logic to choose
  // between buttons in different frames.
  std::vector<autofill::ContentAutofillDriver*> drivers;
  web_contents()->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&](content::RenderFrameHost* rfh) {
        if (autofill::ContentAutofillDriver* driver =
                autofill::ContentAutofillDriver::GetForRenderFrameHost(rfh)) {
          drivers.push_back(driver);
        }
      });

  auto all_frames_scanned_barrier =
      base::BarrierCallback<FrameSiwgButtonCandidates>(
          drivers.size(),
          base::BindOnce(&ActorLoginSiwgController::OnAllFramesScanned,
                         weak_ptr_factory_.GetWeakPtr()));

  for (autofill::ContentAutofillDriver* driver : drivers) {
    driver->GetAutofillAgent()->FindPotentialSiwgButtons(
        base::BindOnce(&ActorLoginSiwgController::OnPotentialSiwgButtonsFound,
                       weak_ptr_factory_.GetWeakPtr(),
                       driver->render_frame_host()->GetGlobalId(),
                       all_frames_scanned_barrier));
  }
}

void ActorLoginSiwgController::OnPotentialSiwgButtonsFound(
    content::GlobalRenderFrameHostId rfh_id,
    base::OnceCallback<void(FrameSiwgButtonCandidates)> barrier_callback,
    std::vector<autofill::mojom::SiwgButtonDataPtr> buttons) {
  std::move(barrier_callback).Run(std::make_pair(rfh_id, std::move(buttons)));
}

void ActorLoginSiwgController::OnAllFramesScanned(
    std::vector<FrameSiwgButtonCandidates> results) {
  CHECK(siwg_finder_);
  CHECK(on_finished_callback_);

  for (const auto& [rfh_id, buttons] : results) {
    content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(rfh_id);
    if (!rfh) {
      // Frame went away.
      continue;
    }
    std::optional<SiwgButtonFinder::SiwgButton> button =
        siwg_finder_->FindButton(rfh, buttons);
    if (button) {
      ClickButton(rfh, button->dom_node_id, std::move(button->observed_target));
      // Ensure we only click one button.
      // ClickButton is the terminal state of this flow - it will resolve
      // on_finished_callback_ after the click result is received.
      return;
    }
  }

  std::move(on_finished_callback_)
      // TODO(crbug.com/478799141): add new status for SiwG failure.
      .Run(base::unexpected(ActorLoginError::kFillingNotAllowed));
}

void ActorLoginSiwgController::ClickButton(
    content::RenderFrameHost* rfh,
    int dom_node_id,
    actor::mojom::ObservedToolTargetPtr observed_target) {
  CHECK(!base::FeatureList::IsEnabled(
      password_manager::features::kActorLoginFederatedClickFromActor));
  GetLocalRoot(rfh)->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame_);

  auto invocation = actor::mojom::ToolInvocation::New();
  auto click = actor::mojom::ClickAction::New();
  click->type = actor::mojom::ClickType::kLeft;
  click->count = actor::mojom::ClickCount::kSingle;
  invocation->action = actor::mojom::ToolAction::NewClick(std::move(click));
  invocation->target = actor::mojom::ToolTarget::NewDomNodeId(dom_node_id);
  invocation->observed_target = std::move(observed_target);

  chrome_render_frame_->InvokeTool(
      std::move(invocation),
      base::BindOnce(&ActorLoginSiwgController::OnClickFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ActorLoginSiwgController::OnClickFinished(
    actor::mojom::ActionResultPtr result) {
  // If the flow already finished (e.g. federated login completed before click
  // returned), we don't need to do anything.
  if (!on_finished_callback_) {
    return;
  }

  if (result->code != actor::mojom::ActionResultCode::kOk) {
    std::move(on_finished_callback_)
        // TODO(crbug.com/478799141): add new status for SiwG failure.
        .Run(base::unexpected(ActorLoginError::kFillingNotAllowed));
  }
}

}  // namespace actor_login
