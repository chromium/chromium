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
  on_destructed_ = std::move(on_destructed);

  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetTitle(GetDialogTitle())
      .AddParagraph(ui::DialogModelLabel(GetDialogLabel()))
      .AddOkButton(std::move(on_destructed_),
                   ui::DialogModel::Button::Params().SetLabel(
                       l10n_util::GetStringUTF16(IDS_OK)));

  ui::WindowAndroid* window = web_contents()->GetTopLevelNativeWindow();
  ui::ModalDialogWrapper::ShowTabModal(dialog_builder.Build(), window);
}

AndroidDataControlsDialog::~AndroidDataControlsDialog() {
  if (on_destructed_) {
    std::move(on_destructed_).Run();
  }
}

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
