// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_ui_util_android.h"

#include <memory>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/public/browser/render_frame_host.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::mojom::FocusedFieldType;

constexpr char kExampleSite[] = "https://example.com";

class PasswordManagerUIUtilAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL(kExampleSite));
    FocusWebContentsOnMainFrame();

    ASSERT_TRUE(web_contents()->GetFocusedFrame());

    password_manager_driver_ = CreatePasswordManagerDriver(main_rfh());
  }

  password_manager::ContentPasswordManagerDriver* password_manager_driver() {
    return password_manager_driver_.get();
  }

  std::unique_ptr<password_manager::ContentPasswordManagerDriver>
  CreatePasswordManagerDriver(content::RenderFrameHost* rfh) {
    return std::make_unique<password_manager::ContentPasswordManagerDriver>(
        rfh, &client_);
  }

 private:
  std::unique_ptr<password_manager::ContentPasswordManagerDriver>
      password_manager_driver_;
  password_manager::StubPasswordManagerClient client_;
};

TEST_F(PasswordManagerUIUtilAndroidTest, ShouldAcceptFocusEvent) {
  EXPECT_TRUE(ShouldAcceptFocusEvent(web_contents(), password_manager_driver(),
                                     FocusedFieldType::kFillablePasswordField));
}

TEST_F(PasswordManagerUIUtilAndroidTest,
       ShouldNotAcceptFocusEventAfterFrameLostFocus) {
  // Make a driver for another frame than the focused one.
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  std::unique_ptr<password_manager::ContentPasswordManagerDriver> bad_driver =
      CreatePasswordManagerDriver(subframe);

  EXPECT_FALSE(
      ShouldAcceptFocusEvent(web_contents(), bad_driver.get(),
                             FocusedFieldType::kFillablePasswordField));
}

TEST_F(PasswordManagerUIUtilAndroidTest,
       ShouldStillAcceptFocusEventIfNoFocusedFrame) {
  // Reset contents, so that no frame was focused.
  SetContents(CreateTestWebContents());

  EXPECT_TRUE(ShouldAcceptFocusEvent(web_contents(), password_manager_driver(),
                                     FocusedFieldType::kUnknown));
}
