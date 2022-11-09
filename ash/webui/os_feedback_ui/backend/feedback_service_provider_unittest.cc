// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/backend/feedback_service_provider.h"

#include <utility>

#include "ash/webui/os_feedback_ui/backend/histogram_util.h"
#include "ash/webui/os_feedback_ui/backend/os_feedback_delegate.h"
#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom-test-utils.h"
#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace feedback {

namespace {

constexpr char kPageUrl[] = "https://www.google.com/";
constexpr char kSignedInUserEmail[] = "test_user_email@test.com";
constexpr char kSignedInInternalUserEmail[] = "test_user_email@google.com";
constexpr char kFeedbackAppPostSubmitAction[] =
    "Feedback.ChromeOSApp.PostSubmitAction";
// Set this flag true to use kSignedInInternalUserEmail as signed in email,
// set false to use kSignedInUserEmail as signed in email.
bool kUseInternalUserEmail = false;
constexpr bool kIsInternalEmail = true;
constexpr bool kIsNotInternalEmail = false;
constexpr int kPerformanceTraceId = 1;
const std::vector<uint8_t> kFakePngData = {42, 22, 26, 13, 7, 16, 8, 2};

using FeedbackAppPostSubmitAction =
    ash::os_feedback_ui::mojom::FeedbackAppPostSubmitAction;

}  // namespace

using ::ash::os_feedback_ui::mojom::FeedbackContext;
using ::ash::os_feedback_ui::mojom::FeedbackContextPtr;
using ::ash::os_feedback_ui::mojom::FeedbackServiceProviderAsyncWaiter;
using ::ash::os_feedback_ui::mojom::Report;
using ::ash::os_feedback_ui::mojom::ReportPtr;
using ::ash::os_feedback_ui::mojom::SendReportStatus;

class TestOsFeedbackDelegate : public OsFeedbackDelegate {
 public:
  TestOsFeedbackDelegate() = default;
  ~TestOsFeedbackDelegate() override = default;

  std::string GetApplicationLocale() override { return "zh"; }

  bool IsChildAccount() override { return false; }

  absl::optional<GURL> GetLastActivePageUrl() override {
    return GURL(kPageUrl);
  }

  absl::optional<std::string> GetSignedInUserEmail() const override {
    return kUseInternalUserEmail ? kSignedInInternalUserEmail
                                 : kSignedInUserEmail;
  }

  int GetPerformanceTraceId() override { return kPerformanceTraceId; }

  void GetScreenshotPng(GetScreenshotPngCallback callback) override {
    std::move(callback).Run(kFakePngData);
  }

  void SendReport(os_feedback_ui::mojom::ReportPtr report,
                  SendReportCallback callback) override {
    std::move(callback).Run(SendReportStatus::kSuccess);
  }

  void OpenDiagnosticsApp() override {}

  void OpenExploreApp() override {}

  void OpenMetricsDialog() override {}

  void OpenSystemInfoDialog() override {}
};

class FeedbackServiceProviderTest : public testing::Test {
 public:
  FeedbackServiceProviderTest()
      : provider_(FeedbackServiceProvider(
            std::make_unique<TestOsFeedbackDelegate>())) {}

  ~FeedbackServiceProviderTest() override = default;

  void SetUp() override {
    provider_.BindInterface(provider_remote_.BindNewPipeAndPassReceiver());
  }

  // Call the GetFeedbackContext of the remote provider async and return the
  // response.
  FeedbackContextPtr GetFeedbackContextAndWait() {
    FeedbackContextPtr out_feedback_context;
    FeedbackServiceProviderAsyncWaiter(provider_remote_.get())
        .GetFeedbackContext(&out_feedback_context);
    return out_feedback_context;
  }

  // Call the GetScreenshotPng of the remote provider async and return the
  // response.
  std::vector<uint8_t> GetScreenshotPngAndWait() {
    std::vector<uint8_t> out_png_data;
    FeedbackServiceProviderAsyncWaiter(provider_remote_.get())
        .GetScreenshotPng(&out_png_data);
    return out_png_data;
  }

  // Call the SendReport of the remote provider async and return the
  // response.
  SendReportStatus SendReportAndWait(ReportPtr report) {
    SendReportStatus out_status;
    FeedbackServiceProviderAsyncWaiter(provider_remote_.get())
        .SendReport(std::move(report), &out_status);
    return out_status;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  FeedbackServiceProvider provider_;
  mojo::Remote<os_feedback_ui::mojom::FeedbackServiceProvider> provider_remote_;
};

// Test that GetFeedbackContext returns a response with correct feedback
// context.
TEST_F(FeedbackServiceProviderTest, GetFeedbackContext) {
  kUseInternalUserEmail = true;
  auto internal_feedback_context = GetFeedbackContextAndWait();

  EXPECT_EQ(kSignedInInternalUserEmail,
            internal_feedback_context->email.value());
  EXPECT_EQ(kPageUrl, internal_feedback_context->page_url.value().spec());
  EXPECT_EQ(kIsInternalEmail, internal_feedback_context->is_internal_account);
  EXPECT_EQ(kPerformanceTraceId, internal_feedback_context->trace_id);

  kUseInternalUserEmail = false;
  auto feedback_context = GetFeedbackContextAndWait();

  EXPECT_EQ(kSignedInUserEmail, feedback_context->email.value());
  EXPECT_EQ(kPageUrl, feedback_context->page_url.value().spec());
  EXPECT_EQ(kIsNotInternalEmail, feedback_context->is_internal_account);
  EXPECT_EQ(kPerformanceTraceId, feedback_context->trace_id);
}

// Test that GetScreenshotPng returns a response with correct status.
TEST_F(FeedbackServiceProviderTest, GetScreenshotPng) {
  auto png_data = GetScreenshotPngAndWait();
  EXPECT_EQ(kFakePngData, png_data);
}

// Test that SendReport returns a response with correct status.
TEST_F(FeedbackServiceProviderTest, SendReportSuccess) {
  ReportPtr report = Report::New();
  report->feedback_context = FeedbackContext::New();
  auto status = SendReportAndWait(std::move(report));
  EXPECT_EQ(status, SendReportStatus::kSuccess);
}

// Test that expected metric is triggered when RecordPostSubmitAction
// is called.
TEST_F(FeedbackServiceProviderTest, RecordPostSubmitAction) {
  base::HistogramTester histogram_tester_;
  histogram_tester_.ExpectBucketCount(
      kFeedbackAppPostSubmitAction,
      FeedbackAppPostSubmitAction::kClickDoneButton, 0);
  provider_.RecordPostSubmitAction(
      FeedbackAppPostSubmitAction::kClickDoneButton);
  histogram_tester_.ExpectBucketCount(
      kFeedbackAppPostSubmitAction,
      FeedbackAppPostSubmitAction::kClickDoneButton, 1);
}

TEST_F(FeedbackServiceProviderTest, ResetReceiverOnBindInterface) {
  // This test simulates a user trying to open a second instant. The receiver
  // should be reset before binding the new receiver. Otherwise we would get a
  // DCHECK error from mojo::Receiver
  provider_remote_.reset();  // reset the binding done in Setup.
  provider_.BindInterface(provider_remote_.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();
}

}  // namespace feedback
}  // namespace ash
