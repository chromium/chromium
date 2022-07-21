// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/backend/feedback_service_provider.h"

#include <utility>

#include "ash/webui/os_feedback_ui/backend/os_feedback_delegate.h"
#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom-test-utils.h"
#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace feedback {

namespace {

constexpr char kPageUrl[] = "https://www.google.com/";
constexpr char kSignedInUserEmail[] = "test_user_email@test.com";
const std::vector<uint8_t> kFakePngData = {42, 22, 26, 13, 7, 16, 8, 2};

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

  absl::optional<GURL> GetLastActivePageUrl() override {
    return GURL(kPageUrl);
  }

  absl::optional<std::string> GetSignedInUserEmail() const override {
    return kSignedInUserEmail;
  }

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
  auto feedback_context = GetFeedbackContextAndWait();

  EXPECT_EQ(kSignedInUserEmail, feedback_context->email.value());
  EXPECT_EQ(kPageUrl, feedback_context->page_url.value().spec());
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

}  // namespace feedback
}  // namespace ash
