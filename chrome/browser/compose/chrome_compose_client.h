// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPOSE_CHROME_COMPOSE_CLIENT_H_
#define CHROME_BROWSER_COMPOSE_CHROME_COMPOSE_CLIENT_H_

#include <memory>
#include <string>

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

  void SetModelExecutorForTest(
      optimization_guide::OptimizationGuideModelExecutor* model_executor);

 private:
  friend class content::WebContentsUserData<ChromeComposeClient>;
  explicit ChromeComposeClient(content::WebContents* web_contents);

  optimization_guide::OptimizationGuideModelExecutor* GetModelExecutor();

  void ModelExecutionCallback(
      ComposeCallback callback,
      optimization_guide::OptimizationGuideModelExecutionResult result);

  compose::ComposeManagerImpl manager_;
  std::unique_ptr<mojo::Receiver<compose::mojom::ComposeDialogPageHandler>>
      handler_receiver_;
  std::unique_ptr<mojo::Remote<compose::mojom::ComposeDialog>> dialog_remote_;

  raw_ptr<optimization_guide::OptimizationGuideModelExecutor>
      model_executor_for_test_;

  // The unique renderer ID of the last field the user selected compose on.
  autofill::FieldGlobalId last_compose_field_id_;

  base::WeakPtrFactory<ChromeComposeClient> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_COMPOSE_CHROME_COMPOSE_CLIENT_H_
