// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_feedback/chrome_os_feedback_delegate.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/feedback/feedback_report.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/feedback_private/feedback_service.h"
#include "extensions/browser/api/feedback_private/mock_feedback_service.h"
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
using extensions::FeedbackParams;
using feedback::FeedbackData;
using testing::_;

constexpr char kPageUrl[] = "https://www.google.com/?q=123";
constexpr char kSignedInUserEmail[] = "test_user_email@gmail.com";
const std::u16string kDescription = u"This is a fake description";

}  // namespace

class ChromeOsFeedbackDelegateTest : public InProcessBrowserTest {
 public:
  ChromeOsFeedbackDelegateTest() = default;
  ~ChromeOsFeedbackDelegateTest() override = default;

  absl::optional<GURL> GetLastActivePageUrl() {
    ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());
    return feedback_delegate_.GetLastActivePageUrl();
  }

 protected:
  void RunSendReport(ReportPtr report,
                     const FeedbackParams& expected_params,
                     scoped_refptr<FeedbackData>& actual_feedback_data) {
    auto* profile_ = browser()->profile();
    auto mock = base::MakeRefCounted<extensions::MockFeedbackService>(profile_);

    EXPECT_CALL(*mock, SendFeedback(_, _, _))
        .WillOnce([&](const extensions::FeedbackParams& params,
                      scoped_refptr<FeedbackData> feedback_data,
                      extensions::SendFeedbackCallback callback) {
          // Pass the feedback data out to verify its properties
          actual_feedback_data = feedback_data;

          // Verify that the flags in params are set correctly
          EXPECT_EQ(expected_params.is_internal_email,
                    params.is_internal_email);
          EXPECT_EQ(expected_params.load_system_info, params.load_system_info);
          EXPECT_EQ(expected_params.send_tab_titles, params.send_tab_titles);
          EXPECT_EQ(expected_params.send_histograms, params.send_histograms);
          EXPECT_EQ(expected_params.send_bluetooth_logs,
                    params.send_bluetooth_logs);

          std::move(callback).Run(true);
        });

    auto feedback_delegate_ =
        std::make_unique<ChromeOsFeedbackDelegate>(profile_, std::move(mock));

    scoped_refptr<base::RefCountedMemory> fake_png_data;
    {
      const unsigned char kData[] = {12, 11, 99};
      fake_png_data =
          base::MakeRefCounted<base::RefCountedBytes>(kData, std::size(kData));
    }
    feedback_delegate_->SetPngDataForTesting(std::move(fake_png_data));

    base::test::TestFuture<SendReportStatus> future;
    feedback_delegate_->SendReport(std::move(report), future.GetCallback());

    EXPECT_NE(SendReportStatus::kUnknown, future.Get());
  }
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

// Test that feedback params and data are populated with correct data before
// passed to SendFeedback method of the feedback service.
// - System logs and histograms are included.
// - Screenshot is included.
// TODO(xiangdongkong): Add tests for other flags once they are supported.
// Currently, only load_system_info and send_histograms flags are implemented.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest,
                       FeedbackDataPopulatedIncludeSysLogsAndScreenshot) {
  ReportPtr report = Report::New();
  report->feedback_context = FeedbackContext::New();
  report->description = kDescription;
  report->include_screenshot = true;

  report->include_system_logs_and_histograms = true;
  const FeedbackParams expected_params{/*is_internal_email=*/false,
                                       /*load_system_info=*/true,
                                       /*send_tab_titles=*/false,
                                       /*send_histograms=*/true,
                                       /*send_bluetooth_logs=*/false};

  scoped_refptr<FeedbackData> feedback_data;
  RunSendReport(std::move(report), expected_params, feedback_data);

  EXPECT_EQ("", feedback_data->user_email());
  EXPECT_EQ("", feedback_data->page_url());
  EXPECT_EQ(base::UTF16ToUTF8(kDescription), feedback_data->description());
  // Verify screenshot is added to feedback data.
  EXPECT_GT(feedback_data->image().size(), 0);
}

// Test that feedback params and data are populated with correct data before
// passed to SendFeedback method of the feedback service.
// - System logs and histograms are not included.
// - Screenshot is not included.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest,
                       FeedbackDataPopulatedNotIncludeSysLogsOrScreenshot) {
  ReportPtr report = Report::New();
  report->feedback_context = FeedbackContext::New();
  report->feedback_context->email = kSignedInUserEmail;
  report->feedback_context->page_url = GURL(kPageUrl);
  report->description = kDescription;
  report->include_screenshot = false;

  report->include_system_logs_and_histograms = false;
  const FeedbackParams expected_params{/*is_internal_email=*/false,
                                       /*load_system_info=*/false,
                                       /*send_tab_titles=*/false,
                                       /*send_histograms=*/false,
                                       /*send_bluetooth_logs=*/false};

  scoped_refptr<FeedbackData> feedback_data;
  RunSendReport(std::move(report), expected_params, feedback_data);

  EXPECT_EQ(kSignedInUserEmail, feedback_data->user_email());
  EXPECT_EQ(kPageUrl, feedback_data->page_url());
  EXPECT_EQ(base::UTF16ToUTF8(kDescription), feedback_data->description());
  // Verify no screenshot is added to feedback data.
  EXPECT_EQ("", feedback_data->image());
}

}  // namespace ash
