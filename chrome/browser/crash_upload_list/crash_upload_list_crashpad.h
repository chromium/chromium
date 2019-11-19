// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRASH_UPLOAD_LIST_CRASH_UPLOAD_LIST_CRASHPAD_H_
#define CHROME_BROWSER_CRASH_UPLOAD_LIST_CRASH_UPLOAD_LIST_CRASHPAD_H_

#include "base/macros.h"
#include "components/upload_list/upload_list.h"

namespace base {
class Time;
}

// An UploadList that retrieves the list of crash reports from the
// Crashpad database.
class CrashUploadListCrashpad : public UploadList {
 public:
  CrashUploadListCrashpad();

 protected:
  ~CrashUploadListCrashpad() override;

  std::vector<UploadInfo> LoadUploadList() override;
  void ClearUploadList(const base::Time& begin, const base::Time& end) override;
  void RequestSingleUpload(const std::string& local_id) override;

  DISALLOW_COPY_AND_ASSIGN(CrashUploadListCrashpad);
};

#endif  // CHROME_BROWSER_CRASH_UPLOAD_LIST_CRASH_UPLOAD_LIST_CRASHPAD_H_
