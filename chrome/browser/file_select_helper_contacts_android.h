// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SELECT_HELPER_CONTACTS_ANDROID_H_
#define CHROME_BROWSER_FILE_SELECT_HELPER_CONTACTS_ANDROID_H_

#include "chrome/browser/file_select_helper.h"

class FileSelectHelperContactsAndroid : public FileSelectHelper {
 public:
  FileSelectHelperContactsAndroid(const FileSelectHelperContactsAndroid&) =
      delete;
  FileSelectHelperContactsAndroid& operator=(
      const FileSelectHelperContactsAndroid&) = delete;

  // A SelectFileDialog::Listener override. |file| and |index| are unused in
  // this override, since the file contents are passed in as string to |params|.
  //
  // TODO(https://crbug.com/340178601): what does this comment mean? how can the
  // file contents be passed in via params? params is supplied by the caller...
  void FileSelected(const ui::SelectedFileInfo& file,
                    int index,
                    void* params) override;
  void FileSelectionCanceled() override;

 private:
  friend class FileSelectHelper;

  explicit FileSelectHelperContactsAndroid(Profile* profile);
  ~FileSelectHelperContactsAndroid() override = default;

  void ProcessContactsForAndroid(const std::string& contacts);
  void ProcessContactsForAndroidOnUIThread(const base::FilePath& temp_file);
};

#endif  // CHROME_BROWSER_FILE_SELECT_HELPER_CONTACTS_ANDROID_H_
