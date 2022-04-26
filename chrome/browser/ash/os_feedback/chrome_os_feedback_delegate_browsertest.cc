// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_feedback/chrome_os_feedback_delegate.h"

#include <memory>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_uploader.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {

namespace {

using ::ash::os_feedback_ui::mojom::FeedbackContext;
using ::ash::os_feedback_ui::mojom::FeedbackContextPtr;
using ::ash::os_feedback_ui::mojom::Report;
using ::ash::os_feedback_ui::mojom::ReportPtr;
using ::ash::os_feedback_ui::mojom::SendReportStatus;

constexpr char kPageUrl[] = "https://www.google.com/?q=123";
constexpr char kSignedInUserEmail[] = "test_user_email@gmail.com";

class MockUploader : public feedback::FeedbackUploader {
 public:
  MockUploader(
      bool is_off_the_record,
      const base::FilePath& state_path,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : FeedbackUploader(is_off_the_record, state_path, url_loader_factory) {}

  MockUploader(const MockUploader&) = delete;
  MockUploader& operator=(const MockUploader&) = delete;

  void QueueReport(std::unique_ptr<std::string> data, bool has_email) override {
    report_had_email_ = has_email;
    called_queue_report_ = true;
  }

  bool called_queue_report() const { return called_queue_report_; }
  bool report_had_email() const { return report_had_email_; }

 private:
  bool called_queue_report_ = false;
  bool report_had_email_ = false;
};

}  // namespace

class ChromeOsFeedbackDelegateTest : public InProcessBrowserTest {
 public:
  ChromeOsFeedbackDelegateTest() {
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }
  ~ChromeOsFeedbackDelegateTest() override = default;

  absl::optional<GURL> GetLastActivePageUrl() {
    ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());
    return feedback_delegate_.GetLastActivePageUrl();
  }

 protected:
  void RunSendReport(ReportPtr report) {
    network::TestURLLoaderFactory test_url_loader_factory_;
    scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    mock_uploader_ = std::make_unique<MockUploader>(
        /*is_off_the_record=*/false, scoped_temp_dir_.GetPath(),
        test_shared_loader_factory_);

    ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());
    feedback_delegate_.SetFeedbackUploaderForTesting(mock_uploader_.get());

    bool send_report_callback_called = false;
    feedback_delegate_.SendReport(
        std::move(report),
        base::BindLambdaForTesting([&](SendReportStatus status) {
          send_report_callback_called = true;
        }));

    EXPECT_TRUE(send_report_callback_called);
  }

  base::ScopedTempDir scoped_temp_dir_;
  std::unique_ptr<MockUploader> mock_uploader_;
};

// Test GetApplicationLocale returns a valid locale.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, GetApplicationLocale) {
  ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());
  EXPECT_EQ(feedback_delegate_.GetApplicationLocale(), "en-US");
}

// Test GetLastActivePageUrl returns last active page url if any.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, GetLastActivePageUrl) {
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1u);
  EXPECT_EQ(GetLastActivePageUrl()->spec(), "about:blank");

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kPageUrl)));
  EXPECT_EQ(GetLastActivePageUrl()->spec(), kPageUrl);
}

// Test GetSignedInUserEmail returns primary account of signed in user if any..
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, GetSignedInUserEmail) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(identity_manager);

  ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());
  EXPECT_EQ(feedback_delegate_.GetSignedInUserEmail(), "");

  signin::MakePrimaryAccountAvailable(identity_manager, kSignedInUserEmail,
                                      signin::ConsentLevel::kSignin);
  EXPECT_EQ(feedback_delegate_.GetSignedInUserEmail(), kSignedInUserEmail);
}

// Test that SendReport method adds the report to queue without an email.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, SendReportNoEmail) {
  ReportPtr report = Report::New();
  report->feedback_context = FeedbackContext::New();
  RunSendReport(std ::move(report));

  EXPECT_TRUE(mock_uploader_->called_queue_report());
  EXPECT_FALSE(mock_uploader_->report_had_email());
}

// Test that SendReport method adds the report to queue with an email.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, SendReportWithEmail) {
  ReportPtr report = Report::New();
  report->feedback_context = FeedbackContext::New();
  report->feedback_context->email = kSignedInUserEmail;
  RunSendReport(std ::move(report));

  EXPECT_TRUE(mock_uploader_->called_queue_report());
  EXPECT_TRUE(mock_uploader_->report_had_email());
}

}  // namespace ash
