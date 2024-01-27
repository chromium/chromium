// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_TEST_UTIL_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_TEST_UTIL_H_

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/messaging_layer/public/report_client.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/test_storage_module.h"

#if !BUILDFLAG(IS_CHROMEOS)

#include <string_view>

#include "base/files/file_path.h"

#endif  // !BUILDFLAG(IS_CHROMEOS)

namespace reporting {

class ReportingClient::TestEnvironment {
 public:
#if !BUILDFLAG(IS_CHROMEOS)
  // Factory method creates an environment with actual local storage at
  // `reporting_path` location and with encryption support included.
  static std::unique_ptr<TestEnvironment> CreateWithLocalStorage(
      const base::FilePath& reporting_path,
      std::string_view verification_key);
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // Factory method creates an environment with a given storage module
  // (usually it is `test::TestStorageModule`).
  static std::unique_ptr<TestEnvironment> CreateWithStorageModule(
      scoped_refptr<StorageModuleInterface> storage =
          base::MakeRefCounted<test::TestStorageModule>());

  TestEnvironment(const TestEnvironment& other) = delete;
  TestEnvironment& operator=(const TestEnvironment& other) = delete;
  ~TestEnvironment();

 private:
  // Constructor is only called by factory methods above.
  explicit TestEnvironment(
      ReportingClient::StorageModuleCreateCallback storage_create_cb);

  const std::unique_ptr<ReportingClient> client_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_TEST_UTIL_H_
