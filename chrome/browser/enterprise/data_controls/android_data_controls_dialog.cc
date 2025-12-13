// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/android_data_controls_dialog.h"

#include "base/functional/callback.h"
#include "chrome/browser/enterprise/data_controls/android_data_controls_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/modal_dialog_wrapper.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"

namespace data_controls {

void AndroidDataControlsDialog::Show(base::OnceClosure on_destructed) {
  ui::WindowAndroid* window = web_contents()->GetTopLevelNativeWindow();
  // On Clank, the modal dialog model is created per-instance, so it must be
  // created and built in this method, as opposed to the constructor (like the
  // desktop dialog).
  ui::ModalDialogWrapper::ShowTabModal(
      CreateDialogModel(std::move(on_destructed)), window);
}

std::unique_ptr<ui::DialogModel> AndroidDataControlsDialog::CreateDialogModel(
    base::OnceClosure on_destructed) {
  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetTitle(GetDialogTitle())
      .AddParagraph(ui::DialogModelLabel(GetDialogLabel()));

  // For the "Warn" dialogs, "Cancel" and "Ok" buttons have their labels /
  // callbacks seemingly flipped - this is because the cancelling the action
  // that is warned against should be the desired / "default" response from
  // the user.
  int cancel_button_label_id;
  int ok_button_label_id;
  switch (type_) {
    case Type::kClipboardPasteWarn:
      cancel_button_label_id = IDS_DATA_CONTROLS_PASTE_WARN_CONTINUE_BUTTON;
      ok_button_label_id = IDS_DATA_CONTROLS_PASTE_WARN_CANCEL_BUTTON;
      break;

    case Type::kClipboardCopyWarn:
      cancel_button_label_id = IDS_DATA_CONTROLS_COPY_WARN_CONTINUE_BUTTON;
      ok_button_label_id = IDS_DATA_CONTROLS_COPY_WARN_CANCEL_BUTTON;
      break;

    case Type::kClipboardShareWarn:
      cancel_button_label_id = IDS_DATA_CONTROLS_SHARE_WARN_CONTINUE_BUTTON;
      ok_button_label_id = IDS_DATA_CONTROLS_SHARE_WARN_CANCEL_BUTTON;
      break;

    case Type::kClipboardActionWarn:
      cancel_button_label_id = IDS_CONTINUE;
      ok_button_label_id = IDS_CANCEL;
      break;

    case Type::kClipboardPasteBlock:
    case Type::kClipboardCopyBlock:
    case Type::kClipboardShareBlock:
    case Type::kClipboardActionBlock:
      // This case should not be reachable in practice.
      NOTREACHED();
      cancel_button_label_id = IDS_CANCEL;
      ok_button_label_id = IDS_OK;
      break;
  }

  switch (type_) {
    case Type::kClipboardPasteBlock:
    case Type::kClipboardCopyBlock:
    case Type::kClipboardShareBlock:
    case Type::kClipboardActionBlock:
      // This case should not be reachable in practice.
      NOTREACHED();

    case Type::kClipboardPasteWarn:
    case Type::kClipboardCopyWarn:
    case Type::kClipboardShareWarn:
    case Type::kClipboardActionWarn:
      dialog_builder.AddCancelButton(
          base::BindOnce(&AndroidDataControlsDialog::OnDialogButtonClicked,
                         base::Unretained(this),
                         /*bypassed=*/true),
          ui::DialogModel::Button::Params().SetLabel(
              l10n_util::GetStringUTF16(cancel_button_label_id)));
      dialog_builder.AddOkButton(
          base::BindOnce(&AndroidDataControlsDialog::OnDialogButtonClicked,
                         base::Unretained(this),
                         /*bypassed=*/false),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(ok_button_label_id))
              .SetStyle(ui::ButtonStyle::kProminent));
      break;
  }

  dialog_builder.SetDialogDestroyingCallback(base::BindOnce(
      [](AndroidDataControlsDialog* dialog,
         base::OnceClosure destruct_callback) {
        std::move(destruct_callback).Run();
        dialog->OnDialogButtonClicked(/*bypassed=*/false);
      },
      base::Unretained(this), std::move(on_destructed)));

  return dialog_builder.Build();
}

AndroidDataControlsDialog::~AndroidDataControlsDialog() = default;

std::u16string AndroidDataControlsDialog::GetDialogTitle() const {
  int id;
  switch (type_) {
    case Type::kClipboardPasteBlock:
    case Type::kClipboardCopyBlock:
    case Type::kClipboardShareBlock:
    case Type::kClipboardActionBlock:
      // This case should not be reachable in practice.
      NOTREACHED();
      id = IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION;
      break;

    case Type::kClipboardPasteWarn:
      id = IDS_DATA_CONTROLS_CLIPBOARD_PASTE_WARN_TITLE;
      break;

    case Type::kClipboardCopyWarn:
      id = IDS_DATA_CONTROLS_CLIPBOARD_COPY_WARN_TITLE;
      break;

    case Type::kClipboardShareWarn:
      id = IDS_DATA_CONTROLS_CLIPBOARD_SHARE_WARN_TITLE;
      break;

    case Type::kClipboardActionWarn:
      id = IDS_DATA_CONTROLS_CLIPBOARD_ACTION_WARN_TITLE;
      break;
  }
  return l10n_util::GetStringUTF16(id);
}

std::u16string AndroidDataControlsDialog::GetDialogLabel() const {
  int id;
  switch (type_) {
    case Type::kClipboardPasteBlock:
    case Type::kClipboardCopyBlock:
    case Type::kClipboardShareBlock:
    case Type::kClipboardActionBlock:
      // This case should not be reachable in practice.
      NOTREACHED();
      id = IDS_DATA_CONTROLS_BLOCKED_LABEL;
      break;

    case Type::kClipboardPasteWarn:
    case Type::kClipboardCopyWarn:
    case Type::kClipboardShareWarn:
    case Type::kClipboardActionWarn:
      id = IDS_DATA_CONTROLS_WARNED_LABEL;
      break;
  }
  return l10n_util::GetStringUTF16(id);
}

AndroidDataControlsDialog::AndroidDataControlsDialog(
    Type type,
    content::WebContents* contents,
    base::OnceCallback<void(bool bypassed)> callback)
    : DataControlsDialog(type, std::move(callback)),
      content::WebContentsObserver(contents) {}

}  // namespace data_controls
