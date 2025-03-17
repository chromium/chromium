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

  switch (type_) {
    case Type::kClipboardPasteBlock:
      // TODO (crbug.com/385163723): Remove callbacks for copy/paste block
      dialog_builder.AddOkButton(
          base::BindOnce(&AndroidDataControlsDialog::OnDialogButtonClicked,
                         base::Unretained(this),
                         /*bypassed=*/false),
          ui::DialogModel::Button::Params().SetLabel(
              l10n_util::GetStringUTF16(IDS_OK)));
      break;

    case Type::kClipboardCopyBlock:
      // TODO (crbug.com/385163723): Remove callbacks for copy/paste block
      dialog_builder.AddOkButton(
          base::BindOnce(&AndroidDataControlsDialog::OnDialogButtonClicked,
                         base::Unretained(this),
                         /*bypassed=*/false),
          ui::DialogModel::Button::Params().SetLabel(
              l10n_util::GetStringUTF16(IDS_OK)));
      break;

      // For the "Warn" dialogs, "Cancel" and "Ok" buttons have their labels /
      // callbacks seemingly flipped - this is because the cancelling the action
      // that is warned against should be the desired / "default" response from
      // the user.

    case Type::kClipboardPasteWarn:
      dialog_builder.AddCancelButton(
          base::BindOnce(&AndroidDataControlsDialog::OnDialogButtonClicked,
                         base::Unretained(this),
                         /*bypassed=*/true),
          ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
              IDS_DATA_CONTROLS_PASTE_WARN_CONTINUE_BUTTON)));
      dialog_builder.AddOkButton(
          base::BindOnce(&AndroidDataControlsDialog::OnDialogButtonClicked,
                         base::Unretained(this),
                         /*bypassed=*/false),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(
                  IDS_DATA_CONTROLS_PASTE_WARN_CANCEL_BUTTON))
              .SetStyle(ui::ButtonStyle::kProminent));
      break;

    case Type::kClipboardCopyWarn:
      dialog_builder.AddCancelButton(
          base::BindOnce(&AndroidDataControlsDialog::OnDialogButtonClicked,
                         base::Unretained(this),
                         /*bypassed=*/true),
          ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
              IDS_DATA_CONTROLS_COPY_WARN_CONTINUE_BUTTON)));
      dialog_builder.AddOkButton(
          base::BindOnce(&AndroidDataControlsDialog::OnDialogButtonClicked,
                         base::Unretained(this),
                         /*bypassed=*/false),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(
                  IDS_DATA_CONTROLS_COPY_WARN_CANCEL_BUTTON))
              .SetStyle(ui::ButtonStyle::kProminent));
      break;
  }

  dialog_builder.SetDialogDestroyingCallback(std::move(on_destructed));
  return dialog_builder.Build();
}

AndroidDataControlsDialog::~AndroidDataControlsDialog() = default;

std::u16string AndroidDataControlsDialog::GetDialogTitle() const {
  int id;
  switch (type_) {
    case Type::kClipboardPasteBlock:
      id = IDS_DATA_CONTROLS_CLIPBOARD_PASTE_BLOCK_TITLE;
      break;

    case Type::kClipboardCopyBlock:
      id = IDS_DATA_CONTROLS_CLIPBOARD_COPY_BLOCK_TITLE;
      break;

    case Type::kClipboardPasteWarn:
      id = IDS_DATA_CONTROLS_CLIPBOARD_PASTE_WARN_TITLE;
      break;

    case Type::kClipboardCopyWarn:
      id = IDS_DATA_CONTROLS_CLIPBOARD_COPY_WARN_TITLE;
      break;
  }
  return l10n_util::GetStringUTF16(id);
}

std::u16string AndroidDataControlsDialog::GetDialogLabel() const {
  int id;
  switch (type_) {
    case Type::kClipboardPasteBlock:
    case Type::kClipboardCopyBlock:
      id = IDS_DATA_CONTROLS_BLOCKED_LABEL;
      break;

    case Type::kClipboardPasteWarn:
    case Type::kClipboardCopyWarn:
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
