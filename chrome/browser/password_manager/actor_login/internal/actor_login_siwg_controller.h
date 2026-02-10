// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_SIWG_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_SIWG_CONTROLLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/actor_login/internal/siwg_button_finder.h"
#include "chrome/browser/webid/federated_actor_login_request.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class WebContents;
class RenderFrameHost;

namespace webid {
enum class FederatedLoginResult;
}  // namespace webid
}  // namespace content

namespace actor_login {

// Controller for Sign-in with Google (SiwG) detection and interaction.
// The flow is as follows:
// 1. The controller is created and `StartFederatedLogin` is called.
// 2. The controller starts capturing the annotated page content.
// 3. Once captured, the controller goes through all frames and calls the
//    renderers to extract potential SiwG buttons.
// 4. The potential buttons are narrowed down using the page content and
//    heuristics on the attributes from DOM.
// 5. The controller clicks the first button that satisfies all requirements.
class ActorLoginSiwgController : public content::WebContentsObserver {
 public:
  using GetPageContentProvider =
      base::RepeatingCallback<void(content::WebContents*,
                                   blink::mojom::AIPageContentOptionsPtr,
                                   optimization_guide::OnAIPageContentDone)>;

  ActorLoginSiwgController(content::WebContents* web_contents,
                           LoginStatusResultOrErrorReply callback);
  ActorLoginSiwgController(content::WebContents* web_contents,
                           GetPageContentProvider get_page_content_provider,
                           LoginStatusResultOrErrorReply callback);
  ~ActorLoginSiwgController() override;

  // Not copyable or movable.
  ActorLoginSiwgController(const ActorLoginSiwgController&) = delete;
  ActorLoginSiwgController& operator=(const ActorLoginSiwgController&) = delete;

  // Starts the federated login flow. This will notify FedCM API that an
  // automated login is in progress, and then start the button detection and
  // click flow.
  void StartFederatedLogin(const Credential& credential);

  // Starts the detection process for SiwG buttons on the current page and
  // clicks the first one found.
  void ClickSiwgButton();

 private:
  void OnPageContentReceived(
      optimization_guide::AIPageContentResultOrError content);

  using FrameSiwgButtonCandidates =
      std::pair<content::GlobalRenderFrameHostId,
                std::vector<autofill::mojom::SiwgButtonDataPtr>>;

  void OnPotentialSiwgButtonsFound(
      content::GlobalRenderFrameHostId rfh_id,
      base::OnceCallback<void(FrameSiwgButtonCandidates)>
          all_frames_scanned_barrier,
      std::vector<autofill::mojom::SiwgButtonDataPtr> buttons);

  void OnAllFramesScanned(std::vector<FrameSiwgButtonCandidates> results);

  void ClickButton(content::RenderFrameHost* rfh,
                   int dom_node_id,
                   actor::mojom::ObservedToolTargetPtr observed_target);

  void OnClickFinished(actor::mojom::ActionResultPtr result);

  void OnFederatedLoginResultReceived(
      content::webid::FederatedLoginResult result);

  GetPageContentProvider get_page_content_provider_;
  std::unique_ptr<SiwgButtonFinder> siwg_finder_;
  LoginStatusResultOrErrorReply on_finished_callback_;

  // Remote for the `ChromeRenderFrame` in the local root of the frame where the
  // SiwG button was found. Keeps the remote alive for the duration of the click
  // action.
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame_;

  base::WeakPtrFactory<ActorLoginSiwgController> weak_ptr_factory_{this};
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_SIWG_CONTROLLER_H_
