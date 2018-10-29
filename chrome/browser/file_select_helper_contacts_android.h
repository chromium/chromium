// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SELECT_HELPER_CONTACTS_ANDROID_H_
#define CHROME_BROWSER_FILE_SELECT_HELPER_CONTACTS_ANDROID_H_

#include "chrome/browser/file_select_helper.h"

class FileSelectHelperContactsAndroid : public FileSelectHelper {
 public:
  // A SelectFileDialog::Listener override. |file| and |index| are unused in
  // this override, since the file contents are passed in as string to |params|.
  void FileSelectedWithExtraInfo(const ui::SelectedFileInfo& file,
                                 int index,
                                 void* params) override;

 private:
  friend class FileSelectHelper;

  explicit FileSelectHelperContactsAndroid(Profile* profile);
  ~FileSelectHelperContactsAndroid() override = default;

  void ProcessContactsForAndroid(const std::string& contacts);
  void ProcessContactsForAndroidOnUIThread(const base::FilePath& temp_file);

  DISALLOW_COPY_AND_ASSIGN(FileSelectHelperContactsAndroid);
};

#endif  // CHROME_BROWSER_FILE_SELECT_HELPER_CONTACTS_ANDROID_H_
