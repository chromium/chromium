// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/ansible/ansible_file_selector.h"

#include "base/path_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
ui::SelectFileDialog::FileTypeInfo GetAnsibleFileTypeInfo() {
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("yaml"));

  return file_type_info;
}
}  // namespace

namespace crostini {
AnsibleFileSelector::AnsibleFileSelector(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

AnsibleFileSelector::~AnsibleFileSelector() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void AnsibleFileSelector::SelectFile(
    base::OnceCallback<void(const base::FilePath&)> selected_callback,
    base::OnceCallback<void(void)> cancelled_callback) {
  selected_callback_ = std::move(selected_callback);
  cancelled_callback_ = std::move(cancelled_callback);
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui_->GetWebContents()));

  // TODO(b/231905716): Add some logic to start from the path of the previously
  // uploaded Ansible Playbook if the user has already "uploaded" a playbook
  // before.
  base::FilePath downloads_path;
  if (!base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &downloads_path))
    return;

  ui::SelectFileDialog::FileTypeInfo file_type_info(GetAnsibleFileTypeInfo());
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      l10n_util::GetStringUTF16(
          IDS_SETTINGS_CROSTINI_ANSIBLE_PLAYBOOK_SELECT_DIALOG_TITLE),
      downloads_path, &file_type_info, 0, FILE_PATH_LITERAL(""),
      GetBrowserWindow(), NULL);
}

gfx::NativeWindow AnsibleFileSelector::GetBrowserWindow() {
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui_->GetWebContents());
  return browser ? browser->window()->GetNativeWindow()
                 : gfx::kNullNativeWindow;
}

void AnsibleFileSelector::FileSelected(const base::FilePath& path,
                                       int index,
                                       void* params) {
  std::move(selected_callback_).Run(path);
}

void AnsibleFileSelector::FileSelectionCanceled(void* params) {
  if (cancelled_callback_)
    std::move(cancelled_callback_).Run();
}

}  // namespace crostini
