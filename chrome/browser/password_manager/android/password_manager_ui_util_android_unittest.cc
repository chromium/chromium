// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_ui_util_android.h"

#include <memory>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
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

    password_mananger_driver_ =
        std::make_unique<password_manager::ContentPasswordManagerDriver>(
            main_rfh(), &client_, &test_autofill_client_);
  }

  password_manager::ContentPasswordManagerDriver* password_mananger_driver() {
    return password_mananger_driver_.get();
  }

 private:
  std::unique_ptr<password_manager::ContentPasswordManagerDriver>
      password_mananger_driver_;
  password_manager::StubPasswordManagerClient client_;
  autofill::TestAutofillClient test_autofill_client_;
};

TEST_F(PasswordManagerUIUtilAndroidTest, ShouldAcceptFocusEvent) {
  EXPECT_TRUE(ShouldAcceptFocusEvent(web_contents(), password_mananger_driver(),
                                     FocusedFieldType::kFillablePasswordField));
}

TEST_F(PasswordManagerUIUtilAndroidTest,
       ShouldNotAcceptFocusEventAfterLoosingFocus) {
  // Pretend that the focus was lost or moved to an unfillable field.
  NavigateAndCommit(GURL("https://random.other-site.org/"));

  EXPECT_FALSE(
      ShouldAcceptFocusEvent(web_contents(), password_mananger_driver(),
                             FocusedFieldType::kFillablePasswordField));
}

TEST_F(PasswordManagerUIUtilAndroidTest,
       ShouldStillAcceptFocusEventIfNoFocusedFrame) {
  // Reset contents, so that no frame was focused.
  SetContents(CreateTestWebContents());

  EXPECT_TRUE(ShouldAcceptFocusEvent(web_contents(), password_mananger_driver(),
                                     FocusedFieldType::kUnknown));
}
