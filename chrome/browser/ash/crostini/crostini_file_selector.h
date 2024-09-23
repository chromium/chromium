// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_FILE_SELECTOR_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_FILE_SELECTOR_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_ui.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace crostini {

// Helper class to open a file dialog to select a file from which the
// container will be created.
class CrostiniFileSelector : public ui::SelectFileDialog::Listener {
 public:
  explicit CrostiniFileSelector(content::WebUI* web_ui);
  CrostiniFileSelector(const CrostiniFileSelector&) = delete;
  CrostiniFileSelector& operator=(const CrostiniFileSelector&) = delete;

  ~CrostiniFileSelector() override;

  // Opens a file selection dialog to choose an Ansible Playbook.
  virtual void SelectFile(
      base::OnceCallback<void(const base::FilePath&)> selected_callback,
      base::OnceCallback<void(void)> cancelled_callback);

 private:
  // Returns handle to browser window or NULL if it can't be found
  gfx::NativeWindow GetBrowserWindow();

  void FileSelected(const ui::SelectedFileInfo& file, int index) override;

  void FileSelectionCanceled() override;

  raw_ptr<content::WebUI> web_ui_;
  base::OnceCallback<void(const base::FilePath&)> selected_callback_;
  base::OnceCallback<void(void)> cancelled_callback_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_FILE_SELECTOR_H_
