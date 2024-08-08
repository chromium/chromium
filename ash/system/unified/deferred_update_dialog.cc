// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/deferred_update_dialog.h"

#include "ash/strings/grit/ash_strings.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DeferredUpdateDialog,
                                      kAutoUpdateCheckboxId);
DeferredUpdateDialog* DeferredUpdateDialog::dialog_ = nullptr;

// static
void DeferredUpdateDialog::CreateDialog(Action callback_action,
                                        base::OnceClosure callback) {
  // Avoid duplicate dialogs.
  if (dialog_)
    return;

  // dialog_ will be released when the dialog is closed.
  dialog_ = new DeferredUpdateDialog();

  auto ok_text = IDS_DEFERRED_UPDATE_DIALOG_UPDATE_SIGN_OUT;
  auto cancel_text = IDS_DEFERRED_UPDATE_DIALOG_SIGN_OUT;

  // Override texts for shutdown.
  bool shutdown = callback_action == kShutDown;
  if (shutdown) {
    ok_text = IDS_DEFERRED_UPDATE_DIALOG_UPDATE_SHUT_DOWN;
    cancel_text = IDS_DEFERRED_UPDATE_DIALOG_SHUT_DOWN;
  }

  std::unique_ptr<ui::DialogModel> dialog_model =
      ui::DialogModel::Builder(std::make_unique<ui::DialogModelDelegate>())
          .SetTitle(l10n_util::GetStringUTF16(IDS_DEFERRED_UPDATE_DIALOG_TITLE))
          .AddOkButton(
              base::BindOnce(&DeferredUpdateDialog::OnApplyDeferredUpdate,
                             base::Unretained(dialog_)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(ok_text)))
          .AddCancelButton(
              base::BindOnce(&DeferredUpdateDialog::OnContinueWithoutUpdate,
                             base::Unretained(dialog_)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(cancel_text)))
          .AddParagraph(ui::DialogModelLabel(
              l10n_util::GetStringUTF16(IDS_DEFERRED_UPDATE_DIALOG_TEXT)))
          .AddCheckbox(kAutoUpdateCheckboxId,
                       ui::DialogModelLabel(l10n_util::GetStringUTF16(
                           IDS_DEFERRED_UPDATE_DIALOG_CHECKBOX)))
          .SetDialogDestroyingCallback(base::BindOnce(
              &DeferredUpdateDialog::OnDialogClosing, base::Unretained(dialog_),
              shutdown, std::move(callback)))
          .Build();

  dialog_->dialog_model_ = dialog_model.get();
  dialog_->dialog_result_ = kClose;

  auto bubble = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::mojom::ModalType::kSystem);
  bubble->SetOwnedByWidget(true);
  views::DialogDelegate::CreateDialogWidget(std::move(bubble),
                                            /*context=*/nullptr,
                                            /*parent=*/nullptr)
      ->Show();
}

// Invoked when "ok" button is clicked.
void DeferredUpdateDialog::OnApplyDeferredUpdate() {
  DCHECK(dialog_model_);
  ui::DialogModelCheckbox* check_box =
      dialog_model_->GetCheckboxByUniqueId(kAutoUpdateCheckboxId);
  if (check_box && check_box->is_checked()) {
    dialog_result_ = kApplyAutoUpdate;
  } else {
    dialog_result_ = kApplyUpdate;
  }
}

// Invoked when "cancel" button is clicked.
void DeferredUpdateDialog::OnContinueWithoutUpdate() {
  dialog_result_ = kIgnoreUpdate;
}

// Invoked when the dialog is closing.
void DeferredUpdateDialog::OnDialogClosing(bool shutdown_after_update,
                                           base::OnceClosure callback) {
  dialog_model_ = nullptr;

  switch (dialog_result_) {
    case kApplyAutoUpdate:
      UpdateEngineClient::Get()->ToggleFeature(
          update_engine::kFeatureConsumerAutoUpdate,
          /*enable=*/true);
      [[fallthrough]];
    case kApplyUpdate:
      UpdateEngineClient::Get()->ApplyDeferredUpdate(shutdown_after_update,
                                                     std::move(callback));
      break;
    case kIgnoreUpdate:
      std::move(callback).Run();
      break;
    case kClose:
      break;
  }

  delete this;
  dialog_ = nullptr;
}

}  // namespace ash
