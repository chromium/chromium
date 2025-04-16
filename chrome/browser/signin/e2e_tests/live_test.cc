// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/signin/e2e_tests/live_test.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "net/dns/mock_host_resolver.h"

base::FilePath::StringViewType kTestAccountFilePath = FILE_PATH_LITERAL(
    "chrome/browser/internal/resources/signin/test_accounts.json");

const char* kRunLiveTestFlag = "run-live-tests";

namespace signin::test {

DirectLookupMixin::DirectLookupMixin(InProcessBrowserTestMixinHost* host,
                                     InProcessBrowserTest* test_base)
    : InProcessBrowserTestMixin(host), test_base_(test_base) {}

void DirectLookupMixin::SetUpInProcessBrowserTestFixture() {
  // Whitelists a bunch of hosts.
  test_base_->host_resolver()->AllowDirectLookup("*.google.com");
  test_base_->host_resolver()->AllowDirectLookup("*.geotrust.com");
  test_base_->host_resolver()->AllowDirectLookup("*.gstatic.com");
  test_base_->host_resolver()->AllowDirectLookup("*.googleapis.com");
  // Allows country-specific TLDs.
  test_base_->host_resolver()->AllowDirectLookup("accounts.google.*");
}

LiveTestMixin::LiveTestMixin(InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}
void LiveTestMixin::SetUp() {
  enabled_ =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kRunLiveTestFlag);
  if (!enabled_) {
    LOG(INFO) << "This test should get skipped (live tests not requested); use "
              << kRunLiveTestFlag << " to enable.";
  }
}

TestAccountsMixin::TestAccountsMixin(InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}
TestAccountsMixin::~TestAccountsMixin() = default;

void TestAccountsMixin::SetUp() {
  base::FilePath root_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path);
  base::FilePath config_path =
      base::MakeAbsoluteFilePath(root_path.Append(kTestAccountFilePath));

  if (config_path.empty()) {
    LOG(INFO) << kTestAccountFilePath
              << " does not exist. This file is only available "
                 "in Google-internal checkouts.";
    return;
  }

  test_accounts_.emplace();
  CHECK(test_accounts_->Init(config_path));
}

void LiveTest::SetUp() {
  if (!live_test_mixin_.Enabled()) {
    GTEST_SKIP() << "Live tests not explicitly requested (use `"
                 << kRunLiveTestFlag << "` command line flag).";
  }
  if (!test_accounts_mixin_.AreAccountsLoaded()) {
    // Only run live tests on Chrome-branded builds
    GTEST_SKIP() << "Test accounts not available (are you running a "
                    "chrome-branded build?).";
  }

  MixinBasedInProcessBrowserTest::SetUp();
}

void LiveTest::TearDown() {
  // This test was skipped, no need to tear down.
  if (!live_test_mixin_.Enabled() ||
      !test_accounts_mixin_.AreAccountsLoaded()) {
    return;
  }
  MixinBasedInProcessBrowserTest::TearDown();
}

void LiveTest::PostRunTestOnMainThread() {
  // This test was skipped. Running PostRunTestOnMainThread can cause
  // TIMED_OUT on Win7.
  if (!live_test_mixin_.Enabled() ||
      !test_accounts_mixin_.AreAccountsLoaded()) {
    return;
  }
  MixinBasedInProcessBrowserTest::PostRunTestOnMainThread();
}
}  // namespace signin::test
