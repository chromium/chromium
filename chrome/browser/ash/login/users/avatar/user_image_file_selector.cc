// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/user_image_file_selector.h"

#include <utility>

#include "base/check.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

// Returns info about extensions for files we support as user images.
ui::SelectFileDialog::FileTypeInfo GetUserImageFileTypeInfo() {
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);

  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("bmp"));

  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("jpg"));
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("jpeg"));

  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("png"));

  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("tif"));
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("tiff"));

  file_type_info.extension_description_overrides.resize(1);
  file_type_info.extension_description_overrides[0] =
      l10n_util::GetStringUTF16(IDS_IMAGE_FILES);

  return file_type_info;
}

}  // namespace

namespace ash {

UserImageFileSelector::UserImageFileSelector(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

UserImageFileSelector::~UserImageFileSelector() {
  if (select_file_dialog_.get()) {
    select_file_dialog_->ListenerDestroyed();
  }
}

void UserImageFileSelector::SelectFile(
    base::OnceCallback<void(const base::FilePath&)> selected_cb,
    base::OnceCallback<void(void)> canceled_cb) {
  // Early return if the select file dialog is already active.
  if (select_file_dialog_) {
    std::move(canceled_cb).Run();
    return;
  }

  selected_cb_ = std::move(selected_cb);
  canceled_cb_ = std::move(canceled_cb);
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui_->GetWebContents()));

  base::FilePath downloads_path;
  if (!base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &downloads_path)) {
    return;
  }

  ui::SelectFileDialog::FileTypeInfo file_type_info(GetUserImageFileTypeInfo());

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_DOWNLOADS_DIRECTORY_LABEL),
      downloads_path, &file_type_info, 0, FILE_PATH_LITERAL(""),
      GetBrowserWindow());
}

gfx::NativeWindow UserImageFileSelector::GetBrowserWindow() {
  Browser* browser = chrome::FindBrowserWithTab(web_ui_->GetWebContents());
  return browser ? browser->window()->GetNativeWindow() : gfx::NativeWindow();
}

void UserImageFileSelector::FileSelected(const ui::SelectedFileInfo& file,
                                         int index) {
  std::move(selected_cb_).Run(file.path());
  select_file_dialog_.reset();
}

void UserImageFileSelector::FileSelectionCanceled() {
  if (!canceled_cb_.is_null()) {
    std::move(canceled_cb_).Run();
  }

  select_file_dialog_.reset();
}

}  // namespace ash
