// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPOSE_CHROME_COMPOSE_CLIENT_H_
#define CHROME_BROWSER_COMPOSE_CHROME_COMPOSE_CLIENT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/browser/compose/compose_session.h"
#include "chrome/browser/compose/proto/compose_optimization_guide.pb.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/common/compose/compose.mojom.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/core/browser/compose_client.h"
#include "components/compose/core/browser/compose_dialog_controller.h"
#include "components/compose/core/browser/compose_manager.h"
#include "components/compose/core/browser/compose_manager_impl.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebContents;
}  // namespace content

// An implementation of `ComposeClient` for Desktop and Android.
class ChromeComposeClient
    : public compose::ComposeClient,
      public content::WebContentsUserData<ChromeComposeClient>,
      public compose::mojom::ComposeDialogClosePageHandler {
 public:
  ChromeComposeClient(const ChromeComposeClient&) = delete;
  ChromeComposeClient& operator=(const ChromeComposeClient&) = delete;
  ~ChromeComposeClient() override;

  // compose::ComposeClient:
  compose::ComposeManager& GetManager() override;
  void ShowComposeDialog(
      autofill::AutofillComposeDelegate::UiEntryPoint ui_entry_point,
      const autofill::FormFieldData& trigger_field,
      std::optional<autofill::AutofillClient::PopupScreenLocation>
          popup_screen_location,
      ComposeCallback callback) override;
  bool HasSession(const autofill::FieldGlobalId& trigger_field_id) override;

  // ComposeDialogClosePageHandler
  // Closes the compose dialog. `reason` describes the user action that
  // triggered the close.
  void CloseUI(compose::mojom::CloseReason reason) override;

  bool ShouldTriggerPopup(std::string autocomplete_attribute,
                          autofill::FieldGlobalId field_id) override;
  virtual bool ShouldTriggerContextMenu(content::RenderFrameHost* rfh,
                                        content::ContextMenuParams& params);

  void BindComposeDialog(
      mojo::PendingReceiver<compose::mojom::ComposeDialogClosePageHandler>
          close_handler,
      mojo::PendingReceiver<compose::mojom::ComposeDialogPageHandler> handler,
      mojo::PendingRemote<compose::mojom::ComposeDialog> dialog);

  void SetModelExecutorForTest(
      optimization_guide::OptimizationGuideModelExecutor* model_executor);
  void SetSkipShowDialogForTest();

  void SetOptimizationGuideForTest(
      optimization_guide::OptimizationGuideDecider* opt_guide);

  // This API gets optimization guidance for a web site.  We use this
  // to guide our decision to enable the feature and trigger the nudge.
  compose::ComposeHintDecision GetOptimizationGuidanceForUrl(const GURL& url);

  ComposeEnabling& GetComposeEnabling();

 protected:
  explicit ChromeComposeClient(content::WebContents* web_contents);
  optimization_guide::OptimizationGuideModelExecutor* GetModelExecutor();
  optimization_guide::OptimizationGuideDecider* GetOptimizationGuide();
  std::unique_ptr<TranslateLanguageProvider> translate_language_provider_;
  ComposeEnabling compose_enabling_;

 private:
  friend class content::WebContentsUserData<ChromeComposeClient>;
  raw_ptr<Profile> profile_;

  // Creates a session for `trigger_field` and initializes it as necessary.
  // `callback` is a callback to the renderer to insert the compose response
  // into the compose field.
  void CreateSessionIfNeeded(const autofill::FormFieldData& trigger_field,
                             ComposeCallback callback);

  // Removes `last_compose_field_id_` from `sessions_` and resets
  // `last_compose_field_id_`.
  void RemoveActiveSession();

  compose::ComposeManagerImpl manager_;

  std::unique_ptr<compose::ComposeDialogController> compose_dialog_controller_;
  // A handle to optimization guide for information about URLs that have
  // recently been navigated to.
  raw_ptr<optimization_guide::OptimizationGuideDecider> opt_guide_;

  std::optional<optimization_guide::OptimizationGuideModelExecutor*>
      model_executor_for_test_;

  // The unique renderer ID of the last field the user selected compose on.
  std::optional<autofill::FieldGlobalId> last_compose_field_id_;

  // Saved states for each compose field.
  base::flat_map<autofill::FieldGlobalId, std::unique_ptr<ComposeSession>>
      sessions_;

  // A mojom receiver that is bound to `this` in `BindComposeDialog()`. A pipe
  // may disconnect but this receiver will still be bound, until reset in the
  // next bind call. With mojo, there is no need to immediately reset the
  // binding when the pipe disconnects. Any callbacks in receiver methods can be
  // safely called even when the pipe is disconnected.
  mojo::Receiver<compose::mojom::ComposeDialogClosePageHandler>
      close_page_receiver_;

  // Used to test Compose in a tab at |chrome://compose|.
  std::unique_ptr<ComposeSession> debug_session_;

  bool skip_show_dialog_for_test_ = false;

  base::WeakPtrFactory<ChromeComposeClient> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_COMPOSE_CHROME_COMPOSE_CLIENT_H_
