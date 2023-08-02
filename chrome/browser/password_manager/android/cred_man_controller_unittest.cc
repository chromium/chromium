// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/cred_man_controller.h"
#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/content/browser/mock_keyboard_replacing_surface_visibility_controller.h"
#include "components/password_manager/core/browser/password_credential_filler.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "device/fido/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

using testing::_;
using testing::Return;

using webauthn::WebAuthnCredManDelegate;
using ToShowVirtualKeyboard = PasswordManagerDriver::ToShowVirtualKeyboard;

struct MockPasswordCredentialFiller : public PasswordCredentialFiller {
  MOCK_METHOD(void,
              FillUsernameAndPassword,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(void, UpdateTriggerSubmission, (bool), (override));
  MOCK_METHOD(bool, ShouldTriggerSubmission, (), (const override));
  MOCK_METHOD(SubmissionReadinessState,
              GetSubmissionReadinessState,
              (),
              (const override));
  MOCK_METHOD(base::WeakPtr<password_manager::PasswordManagerDriver>,
              GetDriver,
              (),
              (const override));
  MOCK_METHOD(const GURL&, GetFrameUrl, (), (const override));
  MOCK_METHOD(void, Dismiss, (ToShowVirtualKeyboard), (override));
};

class CredManControllerTest : public testing::Test {
 public:
  void SetUp() override {
    controller_ =
        std::make_unique<CredManController>(visibility_controller_.AsWeakPtr());
    driver_ = std::make_unique<StubPasswordManagerDriver>();
    web_authn_cred_man_delegate_ =
        std::make_unique<WebAuthnCredManDelegate>(nullptr);
    webauthn::WebAuthnCredManDelegate::override_android_version_for_testing(
        true);
  }

  std::unique_ptr<MockPasswordCredentialFiller> PrepareFiller() {
    auto filler = std::make_unique<MockPasswordCredentialFiller>();
    last_filler_ = filler.get();
    return filler;
  }

  CredManController& controller() { return *controller_.get(); }
  PasswordManagerDriver* driver() { return driver_.get(); }
  WebAuthnCredManDelegate* web_authn_cred_man_delegate() {
    return web_authn_cred_man_delegate_.get();
  }
  MockPasswordCredentialFiller& last_filler() {
    EXPECT_NE(last_filler_, nullptr)
        << "Call PrepareFiller to setup last_filler.";
    return *last_filler_.get();
  }
  MockKeyboardReplacingSurfaceVisibilityController& visibility_controller() {
    return visibility_controller_;
  }

 private:
  std::unique_ptr<CredManController> controller_;
  std::unique_ptr<StubPasswordManagerDriver> driver_;
  std::unique_ptr<WebAuthnCredManDelegate> web_authn_cred_man_delegate_;
  base::raw_ptr<MockPasswordCredentialFiller> last_filler_;
  MockKeyboardReplacingSurfaceVisibilityController visibility_controller_;
};

TEST_F(CredManControllerTest, DoesNotShowIfNonWebAuthnForm) {
  std::unique_ptr<MockPasswordCredentialFiller> filler = PrepareFiller();
  EXPECT_CALL(visibility_controller(), SetVisible(_)).Times(0);
  EXPECT_CALL(last_filler(), Dismiss(ToShowVirtualKeyboard(false))).Times(1);
  EXPECT_FALSE(controller().Show(web_authn_cred_man_delegate(),
                                 std::move(filler),
                                 /*frame_driver=*/nullptr,
                                 /*is_webauthn_form=*/false));
}

TEST_F(CredManControllerTest, DoesNotShowIfFeatureDisabled) {
  std::unique_ptr<MockPasswordCredentialFiller> filler = PrepareFiller();
  EXPECT_CALL(visibility_controller(), SetVisible(_)).Times(0);
  EXPECT_CALL(last_filler(), Dismiss(ToShowVirtualKeyboard(false))).Times(1);
  EXPECT_FALSE(controller().Show(web_authn_cred_man_delegate(),
                                 std::move(filler),
                                 /*frame_driver=*/nullptr,
                                 /*is_webauthn_form=*/true));
}

TEST_F(CredManControllerTest, DoesNotShowIfNoResults) {
  base::test::ScopedFeatureList enable_feature(device::kWebAuthnAndroidCredMan);
  std::unique_ptr<MockPasswordCredentialFiller> filler = PrepareFiller();
  EXPECT_CALL(last_filler(), Dismiss(ToShowVirtualKeyboard(false))).Times(1);

  base::MockCallback<base::RepeatingCallback<void(bool)>>
      mock_full_assertion_request;
  web_authn_cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/false, mock_full_assertion_request.Get());

  EXPECT_CALL(visibility_controller(), SetVisible(_)).Times(0);
  EXPECT_FALSE(controller().Show(web_authn_cred_man_delegate(),
                                 std::move(filler),
                                 /*frame_driver=*/nullptr,
                                 /*is_webauthn_form=*/true));
}

TEST_F(CredManControllerTest, ShowIfResultsExist) {
  base::test::ScopedFeatureList enable_feature(device::kWebAuthnAndroidCredMan);
  std::unique_ptr<MockPasswordCredentialFiller> filler = PrepareFiller();

  base::MockCallback<base::RepeatingCallback<void(bool)>>
      mock_full_assertion_request;
  web_authn_cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/true, mock_full_assertion_request.Get());

  EXPECT_CALL(visibility_controller(), SetVisible(_)).Times(1);
  EXPECT_TRUE(controller().Show(web_authn_cred_man_delegate(),
                                std::move(filler),
                                /*frame_driver=*/nullptr,
                                /*is_webauthn_form=*/true));
}

TEST_F(CredManControllerTest, Fill) {
  base::test::ScopedFeatureList enable_feature(device::kWebAuthnAndroidCredMan);
  base::HistogramTester uma_recorder;
  const std::u16string kUsername = u"test_user";
  const std::u16string kPassword = u"38kAy5Er1Sp0r38";

  std::unique_ptr<MockPasswordCredentialFiller> filler = PrepareFiller();

  base::MockCallback<base::RepeatingCallback<void(bool)>>
      mock_full_assertion_request;
  web_authn_cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/true, mock_full_assertion_request.Get());

  EXPECT_TRUE(controller().Show(web_authn_cred_man_delegate(),
                                std::move(filler),
                                /*frame_driver=*/nullptr,
                                /*is_webauthn_form=*/true));

  ON_CALL(last_filler(), ShouldTriggerSubmission()).WillByDefault(Return(true));
  EXPECT_CALL(last_filler(), FillUsernameAndPassword(kUsername, kPassword));
  web_authn_cred_man_delegate()->FillUsernameAndPassword(kUsername, kPassword);
  uma_recorder.ExpectBucketCount(
      "PasswordManager.CredMan.PasswordFormSubmissionTriggered", /*true*/ 1, 1);
}

}  // namespace password_manager
