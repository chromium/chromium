// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/scoped_worker_based_extensions_channel.h"
#include "net/dns/mock_host_resolver.h"

namespace {

using ContextType = extensions::ExtensionApiTest::ContextType;
using extensions::ScopedWorkerBasedExtensionsChannel;

class DeclarativeNetRequestAPItest
    : public extensions::ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  DeclarativeNetRequestAPItest() = default;

 protected:
  // ExtensionApiTest override.
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    ASSERT_TRUE(StartEmbeddedTestServer());

    // Map all hosts to localhost.
    host_resolver()->AddRule("*", "127.0.0.1");

    base::FilePath test_data_dir =
        test_data_dir_.AppendASCII("declarative_net_request");

    // Copy the |test_data_dir| to a temporary location. We do this to ensure
    // that the temporary kMetadata folder created as a result of loading the
    // extension is not written to the src directory and is automatically
    // removed.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::CopyDirectory(test_data_dir, temp_dir_.GetPath(), true /*recursive*/);

    // Override the path used for loading the extension.
    test_data_dir_ = temp_dir_.GetPath().AppendASCII("declarative_net_request");

    // Service Workers are currently only available on certain channels, so set
    // the channel for those tests.
    if (GetParam() == ContextType::kServiceWorker)
      current_channel_ = std::make_unique<ScopedWorkerBasedExtensionsChannel>();
  }

  bool RunTest(const std::string& extension_path) {
    if (GetParam() != ContextType::kServiceWorker) {
      return RunExtensionTest(extension_path);
    }
    return RunExtensionTestWithFlags(
        extension_path, kFlagRunAsServiceWorkerBasedExtension, kFlagNone);
  }

 private:
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<ScopedWorkerBasedExtensionsChannel> current_channel_;
};

using DeclarativeNetRequestLazyAPItest = DeclarativeNetRequestAPItest;

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         DeclarativeNetRequestAPItest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(EventPage,
                         DeclarativeNetRequestLazyAPItest,
                         ::testing::Values(ContextType::kEventPage));
// Flaky (https://crbug.com/1111240)
INSTANTIATE_TEST_SUITE_P(DISABLED_ServiceWorker,
                         DeclarativeNetRequestLazyAPItest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestLazyAPItest, DynamicRules) {
  ASSERT_TRUE(RunTest("dynamic_rules")) << message_;
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestLazyAPItest, OnRulesMatchedDebug) {
  ASSERT_TRUE(RunTest("on_rules_matched_debug")) << message_;
}

// This test uses webRequest/webRequestBlocking, so it's not currently
// supported for service workers.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestAPItest, ModifyHeaders) {
  ASSERT_TRUE(RunTest("modify_headers")) << message_;
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestLazyAPItest, GetMatchedRules) {
  ASSERT_TRUE(RunTest("get_matched_rules")) << message_;
}

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestLazyAPItest, IsRegexSupported) {
  ASSERT_TRUE(RunTest("is_regex_supported")) << message_;
}

}  // namespace
