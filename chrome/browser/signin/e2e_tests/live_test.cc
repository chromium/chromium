// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/signin/e2e_tests/live_test.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "net/dns/mock_host_resolver.h"

base::FilePath::StringPieceType kTestAccountFilePath = FILE_PATH_LITERAL(
    "chrome/browser/internal/resources/signin/test_accounts.json");

const char* kRunLiveTestFlag = "run-live-tests";

namespace signin {
namespace test {

void LiveTest::SetUpInProcessBrowserTestFixture() {
  // Whitelists a bunch of hosts.
  host_resolver()->AllowDirectLookup("*.google.com");
  host_resolver()->AllowDirectLookup("*.geotrust.com");
  host_resolver()->AllowDirectLookup("*.gstatic.com");
  host_resolver()->AllowDirectLookup("*.googleapis.com");
  // Allows country-specific TLDs.
  host_resolver()->AllowDirectLookup("accounts.google.*");

  InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
}

void LiveTest::SetUp() {
  // Only run live tests when specified.
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(kRunLiveTestFlag)) {
    LOG(INFO) << "This test should get skipped.";
    skip_test_ = true;
    GTEST_SKIP();
  }
  base::FilePath root_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path);
  base::FilePath config_path =
      base::MakeAbsoluteFilePath(root_path.Append(kTestAccountFilePath));
  CHECK(!config_path.empty()) << kTestAccountFilePath
                              << " does not exist. This file is only available "
                                 "in Google-internal checkouts.";
  CHECK(test_accounts_.Init(config_path));
  InProcessBrowserTest::SetUp();
}

void LiveTest::TearDown() {
  // This test was skipped, no need to tear down.
  if (skip_test_)
    return;
  InProcessBrowserTest::TearDown();
}

void LiveTest::PostRunTestOnMainThread() {
  // This test was skipped. Running PostRunTestOnMainThread can cause
  // TIMED_OUT on Win7.
  if (skip_test_)
    return;
  InProcessBrowserTest::PostRunTestOnMainThread();
}
}  // namespace test
}  // namespace signin
