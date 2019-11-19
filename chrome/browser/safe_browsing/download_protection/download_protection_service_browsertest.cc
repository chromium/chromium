// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "components/safe_browsing/web_ui/safe_browsing_ui.h"
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
        content::BrowserContext::GetDownloadManager(browser()->profile());
    content::DownloadTestObserverTerminal observer(
        download_manager, 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);

    // This call will block until the navigation completes, but will not wait
    // for the download to finish.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

    observer.WaitForFinished();
  }

  // The hash of 10 A's, followed by a newline. Was generated as follows:
  //   echo "AAAAAAAAAA" > a.zip
  //   sha256sum a.zip
  static const uint8_t kAZipDigest[];

  // The hash of 9 B's, followed by a newline. Was generated as follows:
  //   echo "BBBBBBBBB" > a.zip
  //   sha256sum a.zip
  static const uint8_t kBZipDigest[];
};

const uint8_t DownloadProtectionServiceBrowserTest::kAZipDigest[] = {
    0x70, 0x5d, 0x29, 0x0c, 0x12, 0x89, 0x59, 0x01, 0xf8, 0x09, 0xf6,
    0xc2, 0xfe, 0x77, 0x2a, 0x94, 0xdb, 0x51, 0x81, 0xd7, 0x26, 0x46,
    0x4d, 0x84, 0x06, 0x82, 0x10, 0x6f, 0x4a, 0x93, 0x4b, 0x87};

const uint8_t DownloadProtectionServiceBrowserTest::kBZipDigest[] = {
    0x94, 0x1e, 0x17, 0x3f, 0x62, 0xbc, 0x04, 0x50, 0x6f, 0xeb, 0xb5,
    0xe2, 0x8c, 0x38, 0x6c, 0xb2, 0x11, 0x91, 0xf3, 0x77, 0xa7, 0x2c,
    0x11, 0x92, 0xe0, 0x25, 0xb0, 0xe5, 0xc7, 0x70, 0x3b, 0x23};

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

  EXPECT_EQ("a.zip", requests[0]->archived_binary(0).file_basename());
  EXPECT_EQ(std::string(kAZipDigest, kAZipDigest + crypto::kSHA256Length),
            requests[0]->archived_binary(0).digests().sha256());

  EXPECT_EQ("b.zip", requests[0]->archived_binary(1).file_basename());
  EXPECT_EQ(std::string(kBZipDigest, kBZipDigest + crypto::kSHA256Length),
            requests[0]->archived_binary(1).digests().sha256());
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

  EXPECT_EQ("a.zip", requests[0]->archived_binary(0).file_basename());
  EXPECT_EQ(std::string(kAZipDigest, kAZipDigest + crypto::kSHA256Length),
            requests[0]->archived_binary(0).digests().sha256());

  EXPECT_EQ("b.zip", requests[0]->archived_binary(1).file_basename());
  EXPECT_EQ(std::string(kBZipDigest, kBZipDigest + crypto::kSHA256Length),
            requests[0]->archived_binary(1).digests().sha256());
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
  EXPECT_EQ("random.exe", requests[0]->archived_binary(0).file_basename());
}

}  // namespace safe_browsing
