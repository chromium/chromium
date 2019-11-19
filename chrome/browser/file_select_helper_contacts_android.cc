// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_select_helper_contacts_android.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "ui/shell_dialogs/selected_file_info.h"

FileSelectHelperContactsAndroid::FileSelectHelperContactsAndroid(
    Profile* profile)
    : FileSelectHelper(profile) {}

void FileSelectHelperContactsAndroid::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& file,
    int index,
    void* params) {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &FileSelectHelperContactsAndroid::ProcessContactsForAndroid, this,
          (char*)params));
}

void FileSelectHelperContactsAndroid::ProcessContactsForAndroid(
    const std::string& contacts) {
  base::FilePath temp_file;
  if (base::CreateTemporaryFile(&temp_file)) {
    bool success =
        WriteFile(temp_file, contacts.c_str(), contacts.length()) > 0;
    temporary_files_.push_back(temp_file);
    if (!success)
      temp_file = base::FilePath();
  }

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &FileSelectHelperContactsAndroid::ProcessContactsForAndroidOnUIThread,
          this, temp_file));
}

void FileSelectHelperContactsAndroid::ProcessContactsForAndroidOnUIThread(
    const base::FilePath& temp_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<ui::SelectedFileInfo> files;

  if (temp_file.empty()) {
    ConvertToFileChooserFileInfoList(files);
    return;
  }

  ui::SelectedFileInfo file_info;
  file_info.local_path = temp_file;
  file_info.display_name = "contacts.json";
  files.push_back(file_info);

  // Typically, |temporary_files| are deleted after |web_contents_| is
  // destroyed. If |web_contents_| is already NULL, then the temporary files
  // need to be deleted now.
  if (!web_contents_) {
    DeleteTemporaryFiles();
    RunFileChooserEnd();
    return;
  }

  ConvertToFileChooserFileInfoList(files);
}
