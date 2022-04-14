// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/backend/feedback_service_provider.h"

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

using ::ash::os_feedback_ui::mojom::FeedbackContext;
using ::ash::os_feedback_ui::mojom::FeedbackContextPtr;
using ::ash::os_feedback_ui::mojom::FeedbackServiceProviderAsyncWaiter;

class FeedbackServiceProviderTest : public testing::Test {
 public:
  FeedbackServiceProviderTest() = default;
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

 protected:
  content::BrowserTaskEnvironment task_environment_;
  FeedbackServiceProvider provider_;
  mojo::Remote<os_feedback_ui::mojom::FeedbackServiceProvider> provider_remote_;
};

// Test that GetFeedbackContext returns a response with correct feedback
// context.
TEST_F(FeedbackServiceProviderTest, GetFeedbackContext) {
  auto feedback_context = GetFeedbackContextAndWait();

  EXPECT_EQ("test@test.com", feedback_context->email.value());
  EXPECT_EQ("chrome://flags/", feedback_context->page_url.value().spec());
}

}  // namespace feedback
}  // namespace ash
