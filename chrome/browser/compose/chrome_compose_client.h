// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPOSE_CHROME_COMPOSE_CLIENT_H_
#define CHROME_BROWSER_COMPOSE_CHROME_COMPOSE_CLIENT_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "chrome/common/compose/compose.mojom.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/core/browser/compose_client.h"
#include "components/compose/core/browser/compose_manager.h"
#include "components/compose/core/browser/compose_manager_impl.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
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
      public compose::mojom::ComposeDialogPageHandler,
      public content::WebContentsUserData<ChromeComposeClient> {
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
      ComposeDialogCallback callback) override;

  void BindComposeDialog(
      mojo::PendingReceiver<compose::mojom::ComposeDialogPageHandler> handler,
      mojo::PendingRemote<compose::mojom::ComposeDialog> dialog);

  // ComposeDialogPageHandler
  void Compose(compose::mojom::StyleModifiersPtr style,
               const std::string& input,
               ComposeCallback callback) override;

  // Retrieves and returns (through `callback`) state information for the last
  // field the user selected compose on.
  void RequestInitialState(RequestInitialStateCallback callback) override;

  void SetModelExecutorForTest(
      optimization_guide::OptimizationGuideModelExecutor* model_executor);
  void SetSkipShowDialogForTest();

 private:
  friend class content::WebContentsUserData<ChromeComposeClient>;
  explicit ChromeComposeClient(content::WebContents* web_contents);

  optimization_guide::OptimizationGuideModelExecutor* GetModelExecutor();

  void ModelExecutionCallback(
      ComposeCallback callback,
      optimization_guide::OptimizationGuideModelExecutionResult result);

  // Creates a compose state for `field_id` if it does not exist.
  void SaveFieldAndCreateComposeStateIfEmpty(
      const autofill::FieldGlobalId& field_id);
  // Saves the compose request in the current state, and saves the previous
  // state for undo.
  void SaveNewComposeRequest(const std::string& input,
                             compose::mojom::StyleModifiersPtr style);
  // Saves the current state in the undo stack if it contains a valid
  // response. States with no response, or with error will not be stored for
  // undo.
  void MaybeSaveCurrentStateInUndoStack();
  // Replaces the existing current compose state with a new blank state.
  void CreateNewCurrentComposeState();
  // Updates the compose state with a new response.
  void UpdateComposeStateWithResponse(compose::mojom::ComposeStatus status,
                                      const std::string& response_text);

  compose::ComposeManagerImpl manager_;
  std::unique_ptr<mojo::Receiver<compose::mojom::ComposeDialogPageHandler>>
      handler_receiver_;
  std::unique_ptr<mojo::Remote<compose::mojom::ComposeDialog>> dialog_remote_;

  std::optional<optimization_guide::OptimizationGuideModelExecutor*>
      model_executor_for_test_;

  // The unique renderer ID of the last field the user selected compose on.
  autofill::FieldGlobalId last_compose_field_id_;

  // Saved states for each compose field.
  base::flat_map<autofill::FieldGlobalId, compose::mojom::ComposeStatePtr>
      field_states_;

  bool skip_show_dialog_for_test_;

  base::WeakPtrFactory<ChromeComposeClient> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_COMPOSE_CHROME_COMPOSE_CLIENT_H_
