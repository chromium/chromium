// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRASH_UPLOAD_LIST_CRASH_UPLOAD_LIST_FUCHSIA_H_
#define CHROME_BROWSER_CRASH_UPLOAD_LIST_CRASH_UPLOAD_LIST_FUCHSIA_H_

#include "components/upload_list/upload_list.h"

namespace base {
class Time;
}

// An UploadList that retrieves the list of crash reports from the
// Fuchsia crash component.
class CrashUploadListFuchsia : public UploadList {
 public:
  CrashUploadListFuchsia();

 protected:
  ~CrashUploadListFuchsia() override;

  std::vector<std::unique_ptr<UploadList::UploadInfo>> LoadUploadList()
      override;
  void ClearUploadList(const base::Time& begin, const base::Time& end) override;
  void RequestSingleUpload(const std::string& local_id) override;

  CrashUploadListFuchsia(const CrashUploadListFuchsia&) = delete;
  CrashUploadListFuchsia& operator=(const CrashUploadListFuchsia&) = delete;
};

#endif  // CHROME_BROWSER_CRASH_UPLOAD_LIST_CRASH_UPLOAD_LIST_FUCHSIA_H_
