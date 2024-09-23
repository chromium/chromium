// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_file_selector.h"

#include "base/path_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace crostini {
CrostiniFileSelector::CrostiniFileSelector(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

CrostiniFileSelector::~CrostiniFileSelector() {
  if (select_file_dialog_.get()) {
    select_file_dialog_->ListenerDestroyed();
  }
}

void CrostiniFileSelector::SelectFile(
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
  if (!base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &downloads_path)) {
    LOG(ERROR) << "Default Downloads path does not exist, cannot open file "
                  "selector for create container";
    return;
  }

  ui::SelectFileDialog::FileTypeInfo file_type_info{
      // Allowed file types include:
      // * Ansible playbooks (yaml)
      // * Crostini backup files (tini, tar.gz, tgz)
      {FILE_PATH_LITERAL("yaml"), FILE_PATH_LITERAL("tini"),
       FILE_PATH_LITERAL("tar.gz"), FILE_PATH_LITERAL("tgz")},
  };
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      l10n_util::GetStringUTF16(
          IDS_SETTINGS_CROSTINI_FILE_SELECTOR_DIALOG_TITLE),
      downloads_path, &file_type_info, 0, FILE_PATH_LITERAL(""),
      GetBrowserWindow());
}

gfx::NativeWindow CrostiniFileSelector::GetBrowserWindow() {
  Browser* browser = chrome::FindBrowserWithTab(web_ui_->GetWebContents());
  return browser ? browser->window()->GetNativeWindow() : gfx::NativeWindow();
}

void CrostiniFileSelector::FileSelected(const ui::SelectedFileInfo& file,
                                        int index) {
  std::move(selected_callback_).Run(file.path());
}

void CrostiniFileSelector::FileSelectionCanceled() {
  if (cancelled_callback_) {
    std::move(cancelled_callback_).Run();
  }
}

}  // namespace crostini
