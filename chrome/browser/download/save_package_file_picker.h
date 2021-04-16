// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_H_
#define CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/save_page_type.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class DownloadPrefs;

// Handles showing a dialog to the user to ask for the filename to save a page.
class SavePackageFilePicker : public ui::SelectFileDialog::Listener {
 public:
  SavePackageFilePicker(content::WebContents* web_contents,
                        const base::FilePath& suggested_path,
                        const base::FilePath::StringType& default_extension,
                        bool can_save_as_complete,
                        DownloadPrefs* download_prefs,
                        content::SavePackagePathPickedCallback callback);
  ~SavePackageFilePicker() override;

  // Used to disable prompting the user for a directory/filename of the saved
  // web page.  This is available for testing.
  static void SetShouldPromptUser(bool should_prompt);

 private:
  // SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* unused_params) override;
  void FileSelectionCanceled(void* unused_params) override;

  bool ShouldSaveAsOnlyHTML(content::WebContents* web_contents) const;
  bool ShouldSaveAsMHTML() const;

  // Used to look up the renderer process for this request to get the context.
  int render_process_id_;

  // Whether the web page can be saved as a complete HTML file.
  bool can_save_as_complete_;

  DownloadPrefs* download_prefs_;

  content::SavePackagePathPickedCallback callback_;

  std::vector<content::SavePageType> save_types_;

  // For managing select file dialogs.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(SavePackageFilePicker);
};

#endif  // CHROME_BROWSER_DOWNLOAD_SAVE_PACKAGE_FILE_PICKER_H_
