// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/cred_man_controller.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/content/browser/mock_keyboard_replacing_surface_visibility_controller.h"
#include "components/password_manager/core/browser/mock_password_credential_filler.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/webauthn/android/cred_man_support.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

using testing::_;
using testing::Return;

using webauthn::CredManSupport;
using webauthn::WebAuthnCredManDelegate;

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(bool,
              IsReauthBeforeFillingRequired,
              (device_reauth::DeviceAuthenticator*),
              (override));
  MOCK_METHOD(std::unique_ptr<device_reauth::DeviceAuthenticator>,
              GetDeviceAuthenticator,
              (),
              (override));
};

class CredManControllerTest : public testing::Test {
 public:
  void SetUp() override {
    auto mock_authenticator =
        std::make_unique<device_reauth::MockDeviceAuthenticator>();
    device_authenticator_ = mock_authenticator.get();
    EXPECT_CALL(*password_client(), GetDeviceAuthenticator)
        .WillOnce(Return(testing::ByMove(std::move(mock_authenticator))))
        .RetiresOnSaturation();
    controller_ = std::make_unique<CredManController>(
        visibility_controller_.AsWeakPtr(), &password_client_);
    driver_ = std::make_unique<StubPasswordManagerDriver>();
    web_authn_cred_man_delegate_ =
        std::make_unique<WebAuthnCredManDelegate>(nullptr);
    WebAuthnCredManDelegate::override_cred_man_support_for_testing(
        CredManSupport::FULL_UNLESS_INAPPLICABLE);
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
  MockPasswordManagerClient* password_client() { return &password_client_; }
  device_reauth::MockDeviceAuthenticator* device_authenticator() {
    return device_authenticator_;
  }

 private:
  std::unique_ptr<CredManController> controller_;
  std::unique_ptr<StubPasswordManagerDriver> driver_;
  std::unique_ptr<WebAuthnCredManDelegate> web_authn_cred_man_delegate_;
  raw_ptr<MockPasswordCredentialFiller> last_filler_;
  MockKeyboardReplacingSurfaceVisibilityController visibility_controller_;
  MockPasswordManagerClient password_client_;
  raw_ptr<device_reauth::MockDeviceAuthenticator> device_authenticator_;
};

TEST_F(CredManControllerTest, DoesNotShowIfNonWebAuthnForm) {
  std::unique_ptr<MockPasswordCredentialFiller> filler = PrepareFiller();
  EXPECT_CALL(visibility_controller(), SetVisible(_)).Times(0);
  EXPECT_FALSE(controller().Show(
      web_authn_cred_man_delegate(), std::move(filler),
      /*frame_driver=*/nullptr,
      /*is_webauthn_form=*/false, CredManController::PasskeyDelayCallback()));
}

TEST_F(CredManControllerTest, DoesNotShowIfFeatureDisabled) {
  WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      CredManSupport::DISABLED);
  std::unique_ptr<MockPasswordCredentialFiller> filler = PrepareFiller();
  EXPECT_CALL(visibility_controller(), SetVisible(_)).Times(0);
  EXPECT_FALSE(controller().Show(
      web_authn_cred_man_delegate(), std::move(filler),
      /*frame_driver=*/nullptr,
      /*is_webauthn_form=*/true, CredManController::PasskeyDelayCallback()));
}

TEST_F(CredManControllerTest, DoesNotShowIfGpmNotInCredMan) {
  WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      CredManSupport::PARALLEL_WITH_FIDO_2);
  std::unique_ptr<MockPasswordCredentialFiller> filler = PrepareFiller();
  EXPECT_CALL(visibility_controller(), SetVisible(_)).Times(0);
  EXPECT_FALSE(controller().Show(
      web_authn_cred_man_delegate(), std::move(filler),
      /*frame_driver=*/nullptr,
      /*is_webauthn_form=*/true, CredManController::PasskeyDelayCallback()));
}

TEST_F(CredManControllerTest, DoesNotShowIfNoResults) {
  std::unique_ptr<MockPasswordCredentialFiller> filler = PrepareFiller();

  base::MockCallback<base::RepeatingCallback<void(bool)>>
      mock_full_assertion_request;
  web_authn_cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/false, mock_full_assertion_request.Get());

  EXPECT_CALL(visibility_controller(), SetVisible(_)).Times(0);
  EXPECT_FALSE(controller().Show(
      web_authn_cred_man_delegate(), std::move(filler),
      /*frame_driver=*/nullptr,
      /*is_webauthn_form=*/true, CredManController::PasskeyDelayCallback()));
}

TEST_F(CredManControllerTest, ShowIfResultsExist) {
  WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      CredManSupport::FULL_UNLESS_INAPPLICABLE);
  std::unique_ptr<MockPasswordCredentialFiller> filler = PrepareFiller();

  base::MockCallback<base::RepeatingCallback<void(bool)>>
      mock_full_assertion_request;
  web_authn_cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/true, mock_full_assertion_request.Get());

  EXPECT_CALL(visibility_controller(), SetVisible(_));
  EXPECT_TRUE(controller().Show(
      web_authn_cred_man_delegate(), std::move(filler),
      /*frame_driver=*/nullptr,
      /*is_webauthn_form=*/true, CredManController::PasskeyDelayCallback()));
}

TEST_F(CredManControllerTest, Fill) {
  WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      CredManSupport::FULL_UNLESS_INAPPLICABLE);
  base::HistogramTester uma_recorder;
  const std::u16string kUsername = u"test_user";
  const std::u16string kPassword = u"38kAy5Er1Sp0r38";

  std::unique_ptr<MockPasswordCredentialFiller> filler = PrepareFiller();

  base::MockCallback<base::RepeatingCallback<void(bool)>>
      mock_full_assertion_request;
  web_authn_cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/true, mock_full_assertion_request.Get());

  EXPECT_TRUE(controller().Show(
      web_authn_cred_man_delegate(), std::move(filler),
      /*frame_driver=*/nullptr,
      /*is_webauthn_form=*/true, CredManController::PasskeyDelayCallback()));

  ON_CALL(last_filler(), ShouldTriggerSubmission()).WillByDefault(Return(true));
  EXPECT_CALL(last_filler(), FillUsernameAndPassword(kUsername, kPassword, _))
      .WillOnce(base::test::RunOnceCallback<2>(/*triggered_submission=*/true));
  web_authn_cred_man_delegate()->FillUsernameAndPassword(kUsername, kPassword);
  uma_recorder.ExpectBucketCount(
      "PasswordManager.CredMan.PasswordFormSubmissionTriggered", /*true*/ 1, 1);
}

TEST_F(CredManControllerTest, FillsPasswordIfReauthSuccessfull) {
  const std::u16string kUsername = u"test_user";
  const std::u16string kPassword = u"38kAy5Er1Sp0r38";

  std::unique_ptr<MockPasswordCredentialFiller> filler = PrepareFiller();

  base::MockCallback<base::RepeatingCallback<void(bool)>>
      mock_full_assertion_request;
  web_authn_cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/true, mock_full_assertion_request.Get());

  ASSERT_TRUE(controller().Show(
      web_authn_cred_man_delegate(), std::move(filler),
      /*frame_driver=*/nullptr,
      /*is_webauthn_form=*/true, CredManController::PasskeyDelayCallback()));

  ON_CALL(*password_client(), IsReauthBeforeFillingRequired)
      .WillByDefault(Return(true));
  EXPECT_CALL(*device_authenticator(), AuthenticateWithMessage)
      .WillOnce(base::test::RunOnceCallback<1>(/*auth_succeeded=*/true));
  EXPECT_CALL(last_filler(), FillUsernameAndPassword(kUsername, kPassword, _));

  web_authn_cred_man_delegate()->FillUsernameAndPassword(kUsername, kPassword);
}

TEST_F(CredManControllerTest, DoesNotFillIfReauthFailed) {
  const std::u16string kUsername = u"test_user";
  const std::u16string kPassword = u"38kAy5Er1Sp0r38";

  std::unique_ptr<MockPasswordCredentialFiller> filler = PrepareFiller();

  base::MockCallback<base::RepeatingCallback<void(bool)>>
      mock_full_assertion_request;
  web_authn_cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/true, mock_full_assertion_request.Get());

  ASSERT_TRUE(controller().Show(
      web_authn_cred_man_delegate(), std::move(filler),
      /*frame_driver=*/nullptr,
      /*is_webauthn_form=*/true, CredManController::PasskeyDelayCallback()));

  ON_CALL(*password_client(), IsReauthBeforeFillingRequired)
      .WillByDefault(Return(true));
  EXPECT_CALL(*device_authenticator(), AuthenticateWithMessage)
      .WillOnce(base::test::RunOnceCallback<1>(/*auth_succeeded=*/false));
  EXPECT_CALL(last_filler(), FillUsernameAndPassword).Times(0);
  web_authn_cred_man_delegate()->FillUsernameAndPassword(kUsername, kPassword);
}

TEST_F(CredManControllerTest, InvokeDelayCallbackWhenNotReady) {
  bool delay_callback_invoked = false;
  bool credential_availability_notified = false;
  CredManController::PasskeyDelayCallback delay_callback =
      base::BindLambdaForTesting(
          [&delay_callback_invoked, &credential_availability_notified](
              base::OnceCallback<void(base::OnceClosure)> callback) {
            delay_callback_invoked = true;
            base::OnceClosure notification_closure = base::BindLambdaForTesting(
                [&credential_availability_notified]() {
                  credential_availability_notified = true;
                });
            std::move(callback).Run(std::move(notification_closure));
          });

  EXPECT_CALL(visibility_controller(), SetVisible).Times(0);
  EXPECT_TRUE(controller().Show(web_authn_cred_man_delegate(), PrepareFiller(),
                                /*frame_driver=*/nullptr,
                                /*is_webauthn_form=*/true,
                                std::move(delay_callback)));
  EXPECT_TRUE(delay_callback_invoked);

  base::MockCallback<base::RepeatingCallback<void(bool)>>
      mock_full_assertion_request;
  web_authn_cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/true, mock_full_assertion_request.Get());

  EXPECT_TRUE(credential_availability_notified);
}

// Verify that if a passkey list has been received, but it is empty, the
// `delay_callback` is not invoked, even when it is present.
TEST_F(CredManControllerTest, DelayCallbackNotInvokedForEmptyPasskeyList) {
  bool delay_callback_invoked = false;
  CredManController::PasskeyDelayCallback delay_callback =
      base::BindLambdaForTesting(
          [&delay_callback_invoked](
              base::OnceCallback<void(base::OnceClosure)> callback) {
            delay_callback_invoked = true;
          });

  base::MockCallback<base::RepeatingCallback<void(bool)>>
      mock_full_assertion_request;
  web_authn_cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/false, mock_full_assertion_request.Get());

  EXPECT_CALL(visibility_controller(), SetVisible).Times(0);
  EXPECT_FALSE(controller().Show(web_authn_cred_man_delegate(), PrepareFiller(),
                                 /*frame_driver=*/nullptr,
                                 /*is_webauthn_form=*/true,
                                 std::move(delay_callback)));
  EXPECT_FALSE(delay_callback_invoked);
}

}  // namespace password_manager
