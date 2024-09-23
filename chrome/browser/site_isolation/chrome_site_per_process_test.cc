// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_site_per_process_test.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

ChromeSitePerProcessTest::ChromeSitePerProcessTest() {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
      // disable this feature.
      /*disabled_features=*/{features::kHttpsUpgrades});
}

ChromeSitePerProcessTest::~ChromeSitePerProcessTest() {}

void ChromeSitePerProcessTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  content::IsolateAllSitesForTesting(command_line);
}

void ChromeSitePerProcessTest::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  content::SetupCrossSiteRedirector(embedded_test_server());

  // Serve from the root so that flash_object.html can load the swf file.
  // Needed for the PluginWithRemoteTopFrame test.
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

  // Add content/test/data for cross_site_iframe_factory.html
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

  embedded_test_server()->StartAcceptingConnections();
}
