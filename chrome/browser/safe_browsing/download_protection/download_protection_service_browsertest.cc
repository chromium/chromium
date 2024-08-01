// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace safe_browsing {

class DownloadProtectionServiceBrowserTest : public InProcessBrowserTest {
 protected:
  base::FilePath GetTestDataDirectory() {
    base::FilePath test_file_directory;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_file_directory);
    return test_file_directory;
  }

  void DownloadAndWait(GURL url) {
    content::DownloadManager* download_manager =
        browser()->profile()->GetDownloadManager();
    content::DownloadTestObserverTerminal observer(
        download_manager, 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);

    // This call will block until the navigation completes, but will not wait
    // for the download to finish.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

    observer.WaitForFinished();
  }

  // The hash of 10 A's, followed by a newline. Was generated as follows:
  //   echo "AAAAAAAAAA" > a.zip
  //   sha256sum a.zip
  static constexpr std::string_view kAZipDigest =
      "\x70\x5d\x29\x0c\x12\x89\x59\x01\xf8\x09\xf6\xc2\xfe\x77\x2a\x94\xdb\x51"
      "\x81\xd7\x26\x46\x4d\x84\x06\x82\x10\x6f\x4a\x93\x4b\x87";

  // The hash of 9 B's, followed by a newline. Was generated as follows:
  //   echo "BBBBBBBBB" > a.zip
  //   sha256sum a.zip
  static constexpr std::string_view kBZipDigest =
      "\x94\x1e\x17\x3f\x62\xbc\x04\x50\x6f\xeb\xb5\xe2\x8c\x38\x6c\xb2\x11\x91"
      "\xf3\x77\xa7\x2c\x11\x92\xe0\x25\xb0\xe5\xc7\x70\x3b\x23";
};

IN_PROC_BROWSER_TEST_F(DownloadProtectionServiceBrowserTest, VerifyZipHash) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());

  WebUIInfoSingleton::GetInstance()->AddListenerForTesting();

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  DownloadAndWait(url);

  const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
      WebUIInfoSingleton::GetInstance()->client_download_requests_sent();

  ASSERT_EQ(1u, requests.size());
  ASSERT_EQ(2, requests[0]->archived_binary_size());

  EXPECT_EQ("a.zip", requests[0]->archived_binary(0).file_path());
  EXPECT_EQ(11, requests[0]->archived_binary(0).length());
  EXPECT_EQ(kAZipDigest, requests[0]->archived_binary(0).digests().sha256());

  EXPECT_EQ("b.zip", requests[0]->archived_binary(1).file_path());
  EXPECT_EQ(10, requests[0]->archived_binary(1).length());
  EXPECT_EQ(kBZipDigest, requests[0]->archived_binary(1).digests().sha256());
}

IN_PROC_BROWSER_TEST_F(DownloadProtectionServiceBrowserTest, VerifyRarHash) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());

  WebUIInfoSingleton::GetInstance()->AddListenerForTesting();

  GURL url =
      embedded_test_server()->GetURL("/safe_browsing/rar/has_two_archives.rar");
  DownloadAndWait(url);

  const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
      WebUIInfoSingleton::GetInstance()->client_download_requests_sent();

  ASSERT_EQ(1u, requests.size());
  ASSERT_EQ(2, requests[0]->archived_binary_size());

  EXPECT_EQ("a.zip", requests[0]->archived_binary(0).file_path());
  EXPECT_EQ(kAZipDigest, requests[0]->archived_binary(0).digests().sha256());

  EXPECT_EQ("b.zip", requests[0]->archived_binary(1).file_path());
  EXPECT_EQ(kBZipDigest, requests[0]->archived_binary(1).digests().sha256());
}

IN_PROC_BROWSER_TEST_F(DownloadProtectionServiceBrowserTest,
                       MultipartRarInspection) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());

  WebUIInfoSingleton::GetInstance()->AddListenerForTesting();

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/rar/multipart.part0001.rar");
  DownloadAndWait(url);

  const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
      WebUIInfoSingleton::GetInstance()->client_download_requests_sent();

  ASSERT_EQ(1u, requests.size());
  ASSERT_EQ(1, requests[0]->archived_binary_size());
  EXPECT_EQ("random.exe", requests[0]->archived_binary(0).file_path());
}

IN_PROC_BROWSER_TEST_F(DownloadProtectionServiceBrowserTest,
                       MultipartRarInspectionSecondPart) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());

  WebUIInfoSingleton::GetInstance()->AddListenerForTesting();

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/rar/multipart.part0002.rar");
  DownloadAndWait(url);

  const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
      WebUIInfoSingleton::GetInstance()->client_download_requests_sent();

  ASSERT_EQ(1u, requests.size());
  ASSERT_EQ(1, requests[0]->archived_binary_size());
  EXPECT_EQ("random.exe", requests[0]->archived_binary(0).file_path());
}

}  // namespace safe_browsing
