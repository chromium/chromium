// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;

namespace safe_browsing {

namespace {

const char kTestDownloadUrl[] = "http://evildownload.com";
const char kDownloadResponseToken[] = "default_token";

}  // namespace

class DownloadDangerPromptTest : public InProcessBrowserTest {
 public:
  DownloadDangerPromptTest()
      : prompt_(nullptr),
        expected_action_(DownloadDangerPrompt::CANCEL),
        did_receive_callback_(false),
        test_safe_browsing_factory_(
            std::make_unique<TestSafeBrowsingServiceFactory>()) {}

  DownloadDangerPromptTest(const DownloadDangerPromptTest&) = delete;
  DownloadDangerPromptTest& operator=(const DownloadDangerPromptTest&) = delete;

  ~DownloadDangerPromptTest() override {}

  void SetUp() override {
    SafeBrowsingService::RegisterFactory(test_safe_browsing_factory_.get());
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    SafeBrowsingService::RegisterFactory(nullptr);
    InProcessBrowserTest::TearDown();
  }

  // Opens a new tab and waits for navigations to finish. If there are pending
  // navigations, the constrained prompt might be dismissed when the navigation
  // completes.
  void OpenNewTab(Browser* browser_to_use) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser_to_use, GURL("about:blank"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
            ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  // Opens a new window and waits for navigations to finish. If there are
  // pending navigations, the constrained prompt might be dismissed when the
  // navigation completes.
  void OpenNewWindow(Browser* browser_to_use) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser_to_use, GURL("about:blank"), WindowOpenDisposition::NEW_WINDOW,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void SetUpExpectations(
      const DownloadDangerPrompt::Action& expected_action,
      const download::DownloadDangerType& danger_type,
      const ClientDownloadResponse::Verdict& download_verdict,
      const std::string& token,
      Browser* browser_to_use) {
    content::DownloadItemUtils::AttachInfoForTesting(
        &download(), browser_to_use->profile(), nullptr);
    did_receive_callback_ = false;
    expected_action_ = expected_action;
    SetUpDownloadItemExpectations(danger_type, token, download_verdict);
    SetUpSafeBrowsingReportExpectations(
        expected_action == DownloadDangerPrompt::ACCEPT, download_verdict,
        token, browser_to_use);
    CreatePrompt(browser_to_use);
  }

  void VerifyExpectations(bool should_send_report) {
    content::RunAllPendingInMessageLoop();
    // At the end of each test, we expect no more activity from the prompt. The
    // prompt shouldn't exist anymore either.
    EXPECT_TRUE(did_receive_callback_);
    EXPECT_FALSE(prompt_);

    if (should_send_report) {
      EXPECT_EQ(expected_serialized_report_,
                test_safe_browsing_factory_->test_safe_browsing_service()
                    ->serialized_download_report());
    } else {
      EXPECT_TRUE(test_safe_browsing_factory_->test_safe_browsing_service()
                      ->serialized_download_report()
                      .empty());
    }
    testing::Mock::VerifyAndClearExpectations(&download_);
    test_safe_browsing_factory_->test_safe_browsing_service()
        ->ClearDownloadReport();
  }

  void SimulatePromptAction(DownloadDangerPrompt::Action action) {
    prompt_->InvokeActionForTesting(action);
  }

  download::MockDownloadItem& download() { return download_; }

  DownloadDangerPrompt* prompt() { return prompt_; }

 private:
  void SetUpDownloadItemExpectations(
      const download::DownloadDangerType& danger_type,
      const std::string& token,
      const ClientDownloadResponse::Verdict& download_verdict) {
    EXPECT_CALL(download_, GetFileNameToReportUser())
        .WillRepeatedly(Return(base::FilePath(FILE_PATH_LITERAL("evil.exe"))));
    EXPECT_CALL(download_, GetDangerType()).WillRepeatedly(Return(danger_type));
    auto token_obj =
        std::make_unique<DownloadProtectionService::DownloadProtectionData>(
            token, download_verdict, ClientDownloadResponse::TailoredVerdict());
    download_.SetUserData(DownloadProtectionService::kDownloadProtectionDataKey,
                          std::move(token_obj));
  }

  void SetUpSafeBrowsingReportExpectations(
      bool did_proceed,
      const ClientDownloadResponse::Verdict& download_verdict,
      const std::string& token,
      Browser* browser_to_use) {
    ClientSafeBrowsingReportRequest expected_report;
    expected_report.set_url(GURL(kTestDownloadUrl).spec());
    expected_report.set_type(
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_BY_API);
    expected_report.set_download_verdict(download_verdict);
    expected_report.set_did_proceed(did_proceed);
    if (!token.empty())
      expected_report.set_token(token);
    expected_report.SerializeToString(&expected_serialized_report_);
  }

  void CreatePrompt(Browser* browser_to_use) {
    prompt_ = DownloadDangerPrompt::Create(
        &download_, browser_to_use->tab_strip_model()->GetActiveWebContents(),
        base::BindOnce(&DownloadDangerPromptTest::PromptCallback,
                       base::Unretained(this)));
    content::RunAllPendingInMessageLoop();
  }

  void PromptCallback(DownloadDangerPrompt::Action action) {
    EXPECT_FALSE(did_receive_callback_);
    EXPECT_EQ(expected_action_, action);
    did_receive_callback_ = true;
    prompt_ = nullptr;
  }

  download::MockDownloadItem download_;
  raw_ptr<DownloadDangerPrompt> prompt_;
  DownloadDangerPrompt::Action expected_action_;
  bool did_receive_callback_;
  std::unique_ptr<TestSafeBrowsingServiceFactory> test_safe_browsing_factory_;
  std::string expected_serialized_report_;
};

// Disabled for flaky timeouts on Windows. crbug.com/446696
#if BUILDFLAG(IS_WIN)
#define MAYBE_TestAll DISABLED_TestAll
#else
#define MAYBE_TestAll TestAll
#endif
IN_PROC_BROWSER_TEST_F(DownloadDangerPromptTest, MAYBE_TestAll) {
  GURL download_url(kTestDownloadUrl);
  ON_CALL(download(), GetURL()).WillByDefault(ReturnRef(download_url));
  ON_CALL(download(), GetReferrerUrl())
      .WillByDefault(ReturnRef(GURL::EmptyGURL()));
  base::FilePath empty_file_path;
  ON_CALL(download(), GetTargetFilePath())
      .WillByDefault(ReturnRef(empty_file_path));

  OpenNewTab(browser());

  // If file is downloaded through download api, a confirm download dialog
  // instead of a recovery dialog is shown. Clicking the Accept button should
  // invoke the ACCEPT action, a report will be sent with type
  // DANGEROUS_DOWNLOAD_BY_API.
  SetUpExpectations(DownloadDangerPrompt::ACCEPT,
                    download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
                    ClientDownloadResponse::DANGEROUS, kDownloadResponseToken,
                    browser());
  EXPECT_CALL(download(), IsDangerous()).WillRepeatedly(Return(true));
  SimulatePromptAction(DownloadDangerPrompt::ACCEPT);
  VerifyExpectations(true);

  // If file is downloaded through download api, a confirm download dialog
  // instead of a recovery dialog is shown. Clicking the Cancel button should
  // invoke the CANCEL action, a report will NOT be sent with type
  // DANGEROUS_DOWNLOAD_BY_API.
  SetUpExpectations(DownloadDangerPrompt::CANCEL,
                    download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT,
                    ClientDownloadResponse::UNCOMMON, std::string(), browser());
  EXPECT_CALL(download(), IsDangerous()).WillRepeatedly(Return(true));
  SimulatePromptAction(DownloadDangerPrompt::CANCEL);
  VerifyExpectations(false);
}

// Class for testing interactive dialogs.
class DownloadDangerPromptBrowserTest : public DialogBrowserTest {
 protected:
  DownloadDangerPromptBrowserTest() : download_url_(kTestDownloadUrl) {}

  DownloadDangerPromptBrowserTest(const DownloadDangerPromptBrowserTest&) =
      delete;
  DownloadDangerPromptBrowserTest& operator=(
      const DownloadDangerPromptBrowserTest&) = delete;

  void RunTest(download::DownloadDangerType danger_type) {
    danger_type_ = danger_type;
    ShowAndVerifyUi();
  }

 private:
  void ShowUi(const std::string& name) override {
    ON_CALL(download_, GetURL()).WillByDefault(ReturnRef(download_url_));
    ON_CALL(download_, GetReferrerUrl())
        .WillByDefault(ReturnRef(GURL::EmptyGURL()));
    ON_CALL(download_, GetTargetFilePath())
        .WillByDefault(ReturnRef(empty_file_path_));
    ON_CALL(download_, IsDangerous()).WillByDefault(Return(true));
    ON_CALL(download_, GetFileNameToReportUser())
        .WillByDefault(Return(base::FilePath(FILE_PATH_LITERAL("evil.exe"))));

    // Set up test-specific parameters
    ON_CALL(download_, GetDangerType()).WillByDefault(Return(danger_type_));
    content::DownloadItemUtils::AttachInfoForTesting(
        &download_, browser()->profile(), nullptr);
    DownloadDangerPrompt::Create(
        &download_, browser()->tab_strip_model()->GetActiveWebContents(),
        DownloadDangerPrompt::OnDone());
  }

  const GURL download_url_;
  const base::FilePath empty_file_path_;

  download::DownloadDangerType danger_type_;
  download::MockDownloadItem download_;
};

IN_PROC_BROWSER_TEST_F(DownloadDangerPromptBrowserTest,
                       InvokeUi_DangerousFileFromApi) {
  RunTest(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);
}

IN_PROC_BROWSER_TEST_F(DownloadDangerPromptBrowserTest,
                       InvokeUi_DangerousUrlFromApi) {
  RunTest(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL);
}

IN_PROC_BROWSER_TEST_F(DownloadDangerPromptBrowserTest,
                       InvokeUi_UncommonContentFromApi) {
  RunTest(download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT);
}

IN_PROC_BROWSER_TEST_F(DownloadDangerPromptBrowserTest,
                       InvokeUi_PotentiallyUnwantedFromApi) {
  RunTest(download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED);
}

IN_PROC_BROWSER_TEST_F(DownloadDangerPromptBrowserTest,
                       InvokeUi_AccountCompromiseFromApi) {
  RunTest(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE);
}

}  // namespace safe_browsing
