// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_FILE_SELECTOR_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_FILE_SELECTOR_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_ui.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace ash {

// Helper class to open a file dialog to choose user image from file.
// It takes in the success/failure callbacks on file selection.
class UserImageFileSelector : public ui::SelectFileDialog::Listener {
 public:
  explicit UserImageFileSelector(content::WebUI* web_ui);

  UserImageFileSelector(const UserImageFileSelector&) = delete;
  UserImageFileSelector& operator=(const UserImageFileSelector&) = delete;

  ~UserImageFileSelector() override;

  // Opens a file selection dialog to choose user image from file.
  virtual void SelectFile(
      base::OnceCallback<void(const base::FilePath&)> selected_cb,
      base::OnceCallback<void(void)> canceled_cb);

 private:
  // Returns handle to browser window or NULL if it can't be found.
  gfx::NativeWindow GetBrowserWindow();

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  raw_ptr<content::WebUI> web_ui_;

  base::OnceCallback<void(const base::FilePath&)> selected_cb_;

  base::OnceCallback<void(void)> canceled_cb_;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_FILE_SELECTOR_H_
