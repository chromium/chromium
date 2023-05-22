// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_download.h"

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bruschetta {
namespace {

class BruschettaDownloadBrowserTest : public InProcessBrowserTest {
 public:
  BruschettaDownloadBrowserTest() = default;
  ~BruschettaDownloadBrowserTest() override = default;

  BruschettaDownloadBrowserTest(const BruschettaDownloadBrowserTest&) = delete;
  BruschettaDownloadBrowserTest& operator=(
      const BruschettaDownloadBrowserTest&) = delete;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

bool PathExistsBlockingAllowed(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  return base::PathExists(path);
}

IN_PROC_BROWSER_TEST_F(BruschettaDownloadBrowserTest, TestHappyPath) {
  auto expected_hash = base::ToUpperASCII(
      "f54d00e6d24844ee3b1d0d8c2b9d2ed80b967e94eb1055bb1fd43eb9522908cc");

  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  server.ServeFilesFromSourceDirectory("chrome/test/data/bruschetta");
  auto server_handle = server.StartAndReturnHandle();
  ASSERT_TRUE(server_handle);
  GURL url = server.GetURL("/download_file.img");

  base::test::TestFuture<base::FilePath, std::string> future;
  base::FilePath path;
  auto download = SimpleURLLoaderDownload::StartDownload(
      browser()->profile(), url, future.GetCallback());

  path = future.Get<base::FilePath>();
  auto hash = future.Get<std::string>();

  ASSERT_FALSE(path.empty());
  EXPECT_EQ(hash, expected_hash);

  // Deleting the download should clean up downloaded files. I found
  // RunUntilIdle to be flaky hence we have an explicit callback for after
  // deletion completes.
  base::RunLoop run_loop;
  download->SetPostDeletionCallbackForTesting(run_loop.QuitClosure());
  download.reset();
  run_loop.Run();
  EXPECT_FALSE(PathExistsBlockingAllowed(path));
}

IN_PROC_BROWSER_TEST_F(BruschettaDownloadBrowserTest, TestDownloadFailed) {
  base::test::TestFuture<base::FilePath, std::string> future;
  auto download = SimpleURLLoaderDownload::StartDownload(
      browser()->profile(), GURL("bad url"), future.GetCallback());

  auto path = future.Get<base::FilePath>();
  auto hash = future.Get<std::string>();

  ASSERT_TRUE(path.empty());
  EXPECT_EQ(hash, "");
}

}  // namespace
}  // namespace bruschetta
