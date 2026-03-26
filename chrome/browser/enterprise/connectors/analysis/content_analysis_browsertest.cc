// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/test/management_context_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/enterprise/connectors/core/content_analysis_browser_test_base.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {
namespace {

class ContentAnalysisBrowserTest : public MixinBasedPlatformBrowserTest,
                                   public test::ContentAnalysisBrowserTestBase {
 public:
  ContentAnalysisBrowserTest()
      : test::ContentAnalysisBrowserTestBase(&embedded_https_test_server()) {
    management_context_mixin_ =
        enterprise::test::ManagementContextMixin::Create(
            &mixin_host_, this, {.is_cloud_machine_managed = true});
  }
  ~ContentAnalysisBrowserTest() override = default;

  void SetUp() override {
    net::test_server::RegisterDefaultHandlers(&embedded_https_test_server());
    CHECK(embedded_https_test_server().InitializeAndListen());

    MixinBasedPlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        &test::ContentAnalysisBrowserTestBase::HandleRequest,
        base::Unretained(this)));
    embedded_https_test_server().StartAcceptingConnections();

    MixinBasedPlatformBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        safe_browsing::switches::kCloudBinaryUploadServiceUrlFlag,
        embedded_https_test_server()
            .GetURL("/safebrowsing/uploads/scan")
            .spec());
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy::ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();

    MixinBasedPlatformBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void EnableScanning(AnalysisConnector connector) {
    constexpr char kBlockingDlpScans[] = R"({
      "service_provider": "google",
      "enable": [
        {
          "url_list": ["*"],
          "tags": ["dlp"]
        }
      ],
      "block_until_verdict": 1
    })";

    test::SetAnalysisConnector(browser()->profile()->GetPrefs(), connector,
                               kBlockingDlpScans, true);
  }

  std::string text() { return "b" + std::string(100, 'a') + "b"; }

 protected:
  std::unique_ptr<enterprise::test::ManagementContextMixin>
      management_context_mixin_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ContentAnalysisBrowserTest, PasteAllowed) {
  EnableScanning(BULK_DATA_ENTRY);

  ContentAnalysisDelegate::Data data;
  data.text.emplace_back(text());
  data.reason = ContentAnalysisRequest::CLIPBOARD_PASTE;
  data.clipboard_source.set_url("https://source.com/");

  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(browser()->profile(),
                                                 browser()
                                                     ->tab_strip_model()
                                                     ->GetActiveWebContents()
                                                     ->GetLastCommittedURL(),
                                                 &data, BULK_DATA_ENTRY));
  AddExpectedScanningRequest(data, text());

  base::RunLoop run_loop;
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&run_loop](const ContentAnalysisDelegate::Data& data,
                      ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.paths_results.empty());
            ASSERT_EQ(result.text_results.size(), 1u);
            ASSERT_TRUE(result.text_results[0]);
            run_loop.Quit();
          }),
      DeepScanAccessPoint::PASTE);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContentAnalysisBrowserTest, FileAttachAllowed) {
  EnableScanning(FILE_ATTACHED);

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("test.txt");
  ASSERT_TRUE(base::WriteFile(file_path, text()));

  ContentAnalysisDelegate::Data data;
  data.paths.emplace_back(file_path);

  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(browser()->profile(),
                                                 browser()
                                                     ->tab_strip_model()
                                                     ->GetActiveWebContents()
                                                     ->GetLastCommittedURL(),
                                                 &data, FILE_ATTACHED));
  AddExpectedScanningRequest(data, text());

  base::RunLoop run_loop;
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&run_loop](const ContentAnalysisDelegate::Data& data,
                      ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_TRUE(result.paths_results[0]);
            run_loop.Quit();
          }),
      DeepScanAccessPoint::UPLOAD);
  run_loop.Run();
}

}  // namespace enterprise_connectors
