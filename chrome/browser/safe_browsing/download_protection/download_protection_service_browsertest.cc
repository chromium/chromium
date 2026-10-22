// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(IS_MAC)
#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "components/safe_browsing/content/common/file_type_policies_test_util.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#endif

namespace safe_browsing {

class DownloadProtectionServiceBrowserTest : public InProcessBrowserTest {
 public:
  DownloadProtectionServiceBrowserTest() {
    // TODO(crbug.com/452061489): Fix tests that fail when the WebUI Omnibox is
    // enabled and then remove this.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{omnibox::internal::kWebUIOmniboxPopup,
                               omnibox::internal::kWebUIOmniboxAimPopup});
  }

 protected:
  base::FilePath GetTestDataDirectory() {
    base::FilePath test_file_directory;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_file_directory);
    return test_file_directory;
  }

  void DownloadAndWait(GURL url) {
    content::DownloadManager* download_manager =
        browser()->GetProfile()->GetDownloadManager();
    content::DownloadTestObserverTerminal observer(
        download_manager, 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);

    // This call will block until the navigation completes, but will not wait
    // for the download to finish.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::kCurrentTab,
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

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    if (auto* sb_service = g_browser_process->safe_browsing_service()) {
      if (auto* download_service = sb_service->download_protection_service()) {
        download_service->SetDownloadRequestTimeoutForTesting(
            base::Seconds(60));
      }
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DownloadProtectionServiceBrowserTest, VerifyZipHash) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());

  WebUIContentInfoSingleton::GetInstance()->AddListenerForTesting();

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  DownloadAndWait(url);

  const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
      WebUIContentInfoSingleton::GetInstance()->client_download_requests_sent();

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

  WebUIContentInfoSingleton::GetInstance()->AddListenerForTesting();

  GURL url =
      embedded_test_server()->GetURL("/safe_browsing/rar/has_two_archives.rar");
  DownloadAndWait(url);

  const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
      WebUIContentInfoSingleton::GetInstance()->client_download_requests_sent();

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

  WebUIContentInfoSingleton::GetInstance()->AddListenerForTesting();

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/rar/multipart.part0001.rar");
  DownloadAndWait(url);

  const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
      WebUIContentInfoSingleton::GetInstance()->client_download_requests_sent();

  ASSERT_EQ(1u, requests.size());
  ASSERT_EQ(1, requests[0]->archived_binary_size());
  EXPECT_EQ("random.exe", requests[0]->archived_binary(0).file_path());
}

IN_PROC_BROWSER_TEST_F(DownloadProtectionServiceBrowserTest,
                       MultipartRarInspectionSecondPart) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());

  WebUIContentInfoSingleton::GetInstance()->AddListenerForTesting();

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/rar/multipart.part0002.rar");
  DownloadAndWait(url);

  const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
      WebUIContentInfoSingleton::GetInstance()->client_download_requests_sent();

  ASSERT_EQ(1u, requests.size());
  ASSERT_EQ(1, requests[0]->archived_binary_size());
  EXPECT_EQ("random.exe", requests[0]->archived_binary(0).file_path());
}

#if BUILDFLAG(IS_MAC)
using ::testing::AllOf;
using ::testing::IsSupersetOf;
using ::testing::Property;
using ::testing::ResultOf;

IN_PROC_BROWSER_TEST_F(DownloadProtectionServiceBrowserTest,
                       VerifyDmgInspection) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());

  WebUIContentInfoSingleton::GetInstance()->AddListenerForTesting();
  base::HistogramTester histogram_tester;

  DownloadAndWait(embedded_test_server()->GetURL(
      "/safe_browsing/dmg/data/mach_o_in_dmg.dmg"));

  histogram_tester.ExpectUniqueSample(
      "SBClientDownload.DmgArchiveAnalysisResult",
      ArchiveAnalysisResult::kValid, 1);

  const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
      WebUIContentInfoSingleton::GetInstance()->client_download_requests_sent();

  ASSERT_EQ(1u, requests.size());
  const ClientDownloadRequest& request = *requests[0];
  EXPECT_EQ(ClientDownloadRequest::MAC_EXECUTABLE, request.download_type());
  EXPECT_EQ(ClientDownloadRequest::ArchiveSummary::VALID,
            request.archive_summary().parser_status());
  EXPECT_FALSE(request.has_udif_code_signature());

  EXPECT_THAT(
      request.archived_binary(),
      IsSupersetOf(
          {AllOf(
               Property(&ClientDownloadRequest::ArchivedBinary::file_path,
                        "Mach-O in DMG/Foo.app/Contents/MacOS/executablefat"),
               Property(&ClientDownloadRequest::ArchivedBinary::download_type,
                        ClientDownloadRequest::MAC_EXECUTABLE),
               Property(&ClientDownloadRequest::ArchivedBinary::image_headers,
                        Property(&ClientDownloadRequest::ImageHeaders::
                                     mach_o_headers_size,
                                 2)),
               Property(
                   &ClientDownloadRequest::ArchivedBinary::digests,
                   Property(
                       &ClientDownloadRequest::Digests::sha256,
                       ResultOf(
                           [](std::string_view sha256) {
                             return base::HexEncode(sha256);
                           },
                           "E462FF752FF9D84E34D843E5D46E2012ADCBD48540A8473FB7"
                           "94B286A389B945")))),
           AllOf(Property(&ClientDownloadRequest::ArchivedBinary::file_path,
                          "Mach-O in DMG/.hidden/lib64.dylib"),
                 Property(&ClientDownloadRequest::ArchivedBinary::download_type,
                          ClientDownloadRequest::MAC_EXECUTABLE),
                 Property(&ClientDownloadRequest::ArchivedBinary::image_headers,
                          Property(&ClientDownloadRequest::ImageHeaders::
                                       mach_o_headers_size,
                                   1)),
                 Property(&ClientDownloadRequest::ArchivedBinary::digests,
                          Property(&ClientDownloadRequest::Digests::sha256,
                                   ResultOf(
                                       [](std::string_view sha256) {
                                         return base::HexEncode(sha256);
                                       },
                                       "2012CE4987B0FA4A5D285DF7E810560E841CFAB"
                                       "3054BC19E1AAB"
                                       "345F862A6C4E"))))}));

  ASSERT_EQ(1, request.detached_code_signature_size());
  EXPECT_EQ(
      "Mach-O in DMG/shell-script.app/Contents/_CodeSignature/CodeSignature",
      request.detached_code_signature(0).file_name());
  EXPECT_EQ(1842u, request.detached_code_signature(0).contents().size());
}

IN_PROC_BROWSER_TEST_F(DownloadProtectionServiceBrowserTest,
                       CorruptDmgInspection) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());

  WebUIContentInfoSingleton::GetInstance()->AddListenerForTesting();
  base::HistogramTester histogram_tester;

  DownloadAndWait(embedded_test_server()->GetURL(
      "/safe_browsing/dmg/data/mach_o_in_dmg_no_koly_signature.dmg"));

  histogram_tester.ExpectUniqueSample(
      "SBClientDownload.DmgArchiveAnalysisResult",
      ArchiveAnalysisResult::kUnknown, 1);

  const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
      WebUIContentInfoSingleton::GetInstance()->client_download_requests_sent();

  ASSERT_EQ(1u, requests.size());
  const ClientDownloadRequest& request = *requests[0];
  EXPECT_EQ(ClientDownloadRequest::MAC_ARCHIVE_FAILED_PARSING,
            request.download_type());
  EXPECT_EQ(ClientDownloadRequest::ArchiveSummary::UNKNOWN,
            request.archive_summary().parser_status());
  EXPECT_EQ(0, request.archived_binary_size());
  EXPECT_EQ(0, request.detached_code_signature_size());
  EXPECT_FALSE(request.has_udif_code_signature());
}

IN_PROC_BROWSER_TEST_F(DownloadProtectionServiceBrowserTest,
                       VerifySignedDmgInspection) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());

  WebUIContentInfoSingleton::GetInstance()->AddListenerForTesting();
  base::HistogramTester histogram_tester;

  DownloadAndWait(embedded_test_server()->GetURL(
      "/safe_browsing/mach_o/signed-archive.dmg"));

  histogram_tester.ExpectUniqueSample(
      "SBClientDownload.DmgArchiveAnalysisResult",
      ArchiveAnalysisResult::kValid, 1);

  const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
      WebUIContentInfoSingleton::GetInstance()->client_download_requests_sent();

  ASSERT_EQ(1u, requests.size());
  const ClientDownloadRequest& request = *requests[0];
  EXPECT_EQ(ClientDownloadRequest::MAC_EXECUTABLE, request.download_type());
  EXPECT_EQ(ClientDownloadRequest::ArchiveSummary::VALID,
            request.archive_summary().parser_status());
  ASSERT_TRUE(request.has_udif_code_signature());

  std::string expected_signature;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(
        GetTestDataDirectory().AppendASCII(
            "safe_browsing/mach_o/signed-archive-signature.data"),
        &expected_signature));
  }
  EXPECT_EQ(2215u, expected_signature.size());
  EXPECT_EQ(expected_signature, request.udif_code_signature());
}

IN_PROC_BROWSER_TEST_F(DownloadProtectionServiceBrowserTest,
                       SniffDmgWithoutDmgExtension) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  // Serve the DMG with a .pkg extension, which is a checked binary type on
  // macOS with `inspection_type: NONE`, so that FileAnalyzer must sniff the
  // UDIF `koly` trailer via DiskImageTypeSnifferMac.
  ASSERT_TRUE(
      base::CopyFile(GetTestDataDirectory().AppendASCII(
                         "safe_browsing/dmg/data/mach_o_in_dmg.dmg"),
                     temp_dir.GetPath().AppendASCII("mach_o_in_dmg.pkg")));

  embedded_test_server()->ServeFilesFromDirectory(temp_dir.GetPath());
  ASSERT_TRUE(embedded_test_server()->Start());

  WebUIContentInfoSingleton::GetInstance()->AddListenerForTesting();
  base::HistogramTester histogram_tester;

  DownloadAndWait(embedded_test_server()->GetURL("/mach_o_in_dmg.pkg"));

  histogram_tester.ExpectUniqueSample(
      "SBClientDownload.DmgArchiveAnalysisResult",
      ArchiveAnalysisResult::kValid, 1);

  const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
      WebUIContentInfoSingleton::GetInstance()->client_download_requests_sent();

  ASSERT_EQ(1u, requests.size());
  const ClientDownloadRequest& request = *requests[0];
  EXPECT_EQ(ClientDownloadRequest::MAC_EXECUTABLE, request.download_type());
  EXPECT_EQ(ClientDownloadRequest::ArchiveSummary::VALID,
            request.archive_summary().parser_status());
  EXPECT_GT(request.archived_binary_size(), 0);
  EXPECT_EQ(1, request.detached_code_signature_size());
}

IN_PROC_BROWSER_TEST_F(DownloadProtectionServiceBrowserTest,
                       SniffNonDmgWithoutDmgExtension) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  // Serve the file without a `koly` signature under a .pkg extension so that
  // DiskImageTypeSnifferMac::IsAppleDiskImage() returns false and FileAnalyzer
  // falls back to StartExtractFileFeatures() instead of DMG analysis.
  ASSERT_TRUE(base::CopyFile(
      GetTestDataDirectory().AppendASCII(
          "safe_browsing/dmg/data/mach_o_in_dmg_no_koly_signature.dmg"),
      temp_dir.GetPath().AppendASCII("mach_o_in_dmg_no_koly_signature.pkg")));

  embedded_test_server()->ServeFilesFromDirectory(temp_dir.GetPath());
  ASSERT_TRUE(embedded_test_server()->Start());

  WebUIContentInfoSingleton::GetInstance()->AddListenerForTesting();
  base::HistogramTester histogram_tester;

  DownloadAndWait(
      embedded_test_server()->GetURL("/mach_o_in_dmg_no_koly_signature.pkg"));

  histogram_tester.ExpectTotalCount("SBClientDownload.DmgArchiveAnalysisResult",
                                    0);

  const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
      WebUIContentInfoSingleton::GetInstance()->client_download_requests_sent();

  ASSERT_EQ(1u, requests.size());
  const ClientDownloadRequest& request = *requests[0];
  EXPECT_EQ(ClientDownloadRequest::MAC_EXECUTABLE, request.download_type());
  EXPECT_FALSE(request.archive_summary().has_parser_status());
  EXPECT_EQ(0, request.archived_binary_size());
  EXPECT_EQ(0, request.detached_code_signature_size());
  EXPECT_FALSE(request.has_udif_code_signature());
}

IN_PROC_BROWSER_TEST_F(DownloadProtectionServiceBrowserTest,
                       LargeDmgSkipsInspection) {
  FileTypePoliciesTestOverlay overlay;
  std::unique_ptr<DownloadFileTypeConfig> config = overlay.DuplicateConfig();
  for (DownloadFileType& file_type : *config->mutable_file_types()) {
    if (file_type.extension() == "dmg") {
      for (DownloadFileType::PlatformSettings& settings :
           *file_type.mutable_platform_settings()) {
        settings.set_max_file_size_to_analyze(0);
      }
      break;
    }
  }
  overlay.SwapConfig(config);

  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());

  WebUIContentInfoSingleton::GetInstance()->AddListenerForTesting();
  base::HistogramTester histogram_tester;

  DownloadAndWait(embedded_test_server()->GetURL(
      "/safe_browsing/dmg/data/mach_o_in_dmg.dmg"));

  histogram_tester.ExpectUniqueSample(
      "SBClientDownload.DmgArchiveAnalysisResult",
      ArchiveAnalysisResult::kTooLarge, 1);

  const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
      WebUIContentInfoSingleton::GetInstance()->client_download_requests_sent();

  ASSERT_EQ(1u, requests.size());
  const ClientDownloadRequest& request = *requests[0];
  EXPECT_EQ(ClientDownloadRequest::MAC_ARCHIVE_FAILED_PARSING,
            request.download_type());
  EXPECT_EQ(ClientDownloadRequest::ArchiveSummary::TOO_LARGE,
            request.archive_summary().parser_status());
  EXPECT_EQ(0, request.archived_binary_size());
  EXPECT_EQ(0, request.detached_code_signature_size());
  EXPECT_FALSE(request.has_udif_code_signature());
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace safe_browsing
