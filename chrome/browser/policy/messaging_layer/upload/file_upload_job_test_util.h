// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FILE_UPLOAD_JOB_TEST_UTIL_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FILE_UPLOAD_JOB_TEST_UTIL_H_

#include "chrome/browser/policy/messaging_layer/upload/file_upload_job.h"

namespace reporting {

class FileUploadJob::TestEnvironment {
 public:
  TestEnvironment();
  TestEnvironment(const TestEnvironment& other) = delete;
  TestEnvironment& operator=(const TestEnvironment& other) = delete;
  ~TestEnvironment();
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FILE_UPLOAD_JOB_TEST_UTIL_H_
