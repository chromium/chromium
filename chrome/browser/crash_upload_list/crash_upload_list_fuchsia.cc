// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list/crash_upload_list_fuchsia.h"

#include "base/notreached.h"
#include "base/time/time.h"

CrashUploadListFuchsia::CrashUploadListFuchsia() = default;

CrashUploadListFuchsia::~CrashUploadListFuchsia() = default;

std::vector<UploadList::UploadInfo> CrashUploadListFuchsia::LoadUploadList() {
  // TODO(crbug.com/1234373): Implement using the crash system component.
  NOTIMPLEMENTED_LOG_ONCE();
  return {};
}

void CrashUploadListFuchsia::ClearUploadList(const base::Time& begin,
                                             const base::Time& end) {
  // TODO(crbug.com/1234373): Implement using the crash system component.
  NOTIMPLEMENTED_LOG_ONCE();
}

void CrashUploadListFuchsia::RequestSingleUpload(const std::string& local_id) {
  // TODO(crbug.com/1234373): Implement using the crash system component.
  NOTIMPLEMENTED_LOG_ONCE();
}
