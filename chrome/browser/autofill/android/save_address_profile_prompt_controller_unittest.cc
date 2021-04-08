// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/save_address_profile_prompt_controller.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class MockSaveAddressProfilePromptView : public SaveAddressProfilePromptView {
 public:
  MOCK_METHOD(bool,
              Show,
              (SaveAddressProfilePromptController * controller),
              (override));
};

class SaveAddressProfilePromptControllerTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        features::kAutofillAddressProfileSavePrompt);

    auto prompt_view = std::make_unique<MockSaveAddressProfilePromptView>();
    prompt_view_ = prompt_view.get();
    controller_ = std::make_unique<SaveAddressProfilePromptController>(
        std::move(prompt_view), profile_, save_address_profile_callback_.Get(),
        dismissal_callback_.Get());
    ON_CALL(*prompt_view_, Show(controller_.get()))
        .WillByDefault(testing::Return(true));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  MockSaveAddressProfilePromptView* prompt_view_;
  AutofillProfile profile_ = test::GetFullProfile();
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      save_address_profile_callback_;
  base::MockCallback<base::OnceCallback<void()>> dismissal_callback_;
  std::unique_ptr<SaveAddressProfilePromptController> controller_;
};

TEST_F(SaveAddressProfilePromptControllerTest, ShouldShowViewOnDisplayPrompt) {
  EXPECT_CALL(*prompt_view_, Show(controller_.get()));
  controller_->DisplayPrompt();
}

TEST_F(SaveAddressProfilePromptControllerTest,
       ShouldInvokeDismissalCallbackWhenShowReturnsFalse) {
  EXPECT_CALL(*prompt_view_, Show(controller_.get()))
      .WillOnce(testing::Return(false));

  EXPECT_CALL(dismissal_callback_, Run());
  controller_->DisplayPrompt();
}

TEST_F(SaveAddressProfilePromptControllerTest,
       ShouldInvokeSaveCallbackWhenUserAccepts) {
  controller_->DisplayPrompt();

  EXPECT_CALL(
      save_address_profile_callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted,
          profile_));
  controller_->OnAccepted();
}

TEST_F(SaveAddressProfilePromptControllerTest,
       ShouldInvokeSaveCallbackWhenUserDeclines) {
  controller_->DisplayPrompt();

  EXPECT_CALL(
      save_address_profile_callback_,
      Run(AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined,
          profile_));
  controller_->OnDeclined();
}

TEST_F(SaveAddressProfilePromptControllerTest,
       ShouldInvokeDismissalCallbackWhenPromptIsDismissed) {
  controller_->DisplayPrompt();

  EXPECT_CALL(dismissal_callback_, Run());
  controller_->OnPromptDismissed();
}

TEST_F(SaveAddressProfilePromptControllerTest,
       ShouldInvokeSaveCallbackWhenControllerDiesWithoutInteraction) {
  controller_->DisplayPrompt();

  EXPECT_CALL(save_address_profile_callback_,
              Run(AutofillClient::SaveAddressProfileOfferUserDecision::kIgnored,
                  profile_));
  controller_.reset();
}

}  // namespace autofill
