// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_TEST_UTIL_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_TEST_UTIL_H_

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/policy/messaging_layer/public/report_client.h"

namespace reporting {

class ReportingClient::TestEnvironment {
 public:
  TestEnvironment(const TestEnvironment& other) = delete;
  TestEnvironment& operator=(const TestEnvironment& other) = delete;
  TestEnvironment(const base::FilePath& reporting_path,
                  base::StringPiece verification_key);
  ~TestEnvironment();

 private:
  ReportingClient::StorageModuleCreateCallback saved_storage_create_cb_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_TEST_UTIL_H_
