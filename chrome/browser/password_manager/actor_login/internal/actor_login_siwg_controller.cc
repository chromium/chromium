// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_siwg_controller.h"

#include "base/barrier_callback.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/webid/federated_actor_login_request.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
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

}  // namespace

ActorLoginSiwgController::ActorLoginSiwgController(
    content::WebContents* web_contents,
    LoginStatusResultOrErrorReply on_finished_callback)
    : ActorLoginSiwgController(
          web_contents,
          base::BindRepeating(&optimization_guide::GetAIPageContent),
          std::move(on_finished_callback)) {}

ActorLoginSiwgController::ActorLoginSiwgController(
    content::WebContents* web_contents,
    GetPageContentProvider get_page_content_provider,
    LoginStatusResultOrErrorReply on_finished_callback)
    : content::WebContentsObserver(web_contents),
      get_page_content_provider_(std::move(get_page_content_provider)),
      on_finished_callback_(std::move(on_finished_callback)) {}

ActorLoginSiwgController::~ActorLoginSiwgController() = default;

void ActorLoginSiwgController::StartFederatedLogin(
    const Credential& credential) {
  CHECK(credential.federation_detail);

  FederatedActorLoginRequest::Set(
      web_contents(), credential.federation_detail->idp_origin,
      credential.federation_detail->account_id,
      base::BindRepeating(
          &ActorLoginSiwgController::OnFederatedLoginResultReceived,
          weak_ptr_factory_.GetWeakPtr()));

  // There may be an existing FedCM dialog; if so, select an account in that
  // dialog instead of clicking the signin button.
  auto* source = content::webid::IdentityCredentialSource::FromPage(
      web_contents()->GetPrimaryPage());
  if (!source->SelectAccount(credential.federation_detail->idp_origin,
                             credential.federation_detail->account_id)) {
    ClickSiwgButton();
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
    content::webid::FederatedLoginResult result) {
  if (!on_finished_callback_) {
    return;
  }
  if (result == content::webid::FederatedLoginResult::kSuccess) {
    std::move(on_finished_callback_)
        // TODO(crbug.com/478799141): add new status for SiwG success.
        .Run(LoginStatusResult::kSuccessUsernameAndPasswordFilled);
  } else if (result == content::webid::FederatedLoginResult::kContinuation) {
    // TODO(crbug.com/481685277): handle the continuation case.
  } else {
    std::move(on_finished_callback_)
        // TODO(crbug.com/478799141): add new status for SiwG failure.
        .Run(base::unexpected(ActorLoginError::kFillingNotAllowed));
  }
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
  // TODO(crbug.com/478798187): Use ActorTask instead of InvokeTool.
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
