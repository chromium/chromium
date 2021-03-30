// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

namespace {

using ContextType = extensions::ExtensionApiTest::ContextType;
using extensions::ScopedCurrentChannel;

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
  }

  bool RunTest(const std::string& extension_path) {
    return RunExtensionTest(
        {.name = extension_path.c_str()},
        {.load_as_service_worker = GetParam() == ContextType::kServiceWorker});
  }

 private:
  base::ScopedTempDir temp_dir_;
};

using DeclarativeNetRequestLazyAPItest = DeclarativeNetRequestAPItest;

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         DeclarativeNetRequestAPItest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(EventPage,
                         DeclarativeNetRequestLazyAPItest,
                         ::testing::Values(ContextType::kEventPage));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         DeclarativeNetRequestLazyAPItest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestLazyAPItest, DynamicRules) {
  ASSERT_TRUE(RunTest("dynamic_rules")) << message_;
}

// Flaky on ASAN/MSAN: https://crbug.com/1167168
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_DynamicRulesLimits DISABLED_DynamicRulesLimits
#else
#define MAYBE_DynamicRulesLimits DynamicRulesLimits
#endif
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestLazyAPItest,
                       MAYBE_DynamicRulesLimits) {
  ASSERT_TRUE(RunTest("dynamic_rules_limits")) << message_;
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
