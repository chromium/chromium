// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/select_file_dialog_util.h"

#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace file_manager {
namespace util {

std::u16string GetSelectFileDialogTitle(
    ui::SelectFileDialog::Type dialog_type) {
  std::u16string title;
  switch (dialog_type) {
    case ui::SelectFileDialog::SELECT_NONE:
      // Full page file manager doesn't need a title.
      break;

    case ui::SelectFileDialog::SELECT_FOLDER:
    case ui::SelectFileDialog::SELECT_EXISTING_FOLDER:
      title = l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SELECT_FOLDER_TITLE);
      break;

    case ui::SelectFileDialog::SELECT_UPLOAD_FOLDER:
      title = l10n_util::GetStringUTF16(
          IDS_FILE_BROWSER_SELECT_UPLOAD_FOLDER_TITLE);
      break;

    case ui::SelectFileDialog::SELECT_SAVEAS_FILE:
      title =
          l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SELECT_SAVEAS_FILE_TITLE);
      break;

    case ui::SelectFileDialog::SELECT_OPEN_FILE:
      title =
          l10n_util::GetStringUTF16(IDS_FILE_BROWSER_SELECT_OPEN_FILE_TITLE);
      break;

    case ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE:
      title = l10n_util::GetStringUTF16(
          IDS_FILE_BROWSER_SELECT_OPEN_MULTI_FILE_TITLE);
      break;
  }

  return title;
}

}  // namespace util
}  // namespace file_manager
