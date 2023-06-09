// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/cred_man_controller.h"
#include <memory>

#include "base/android/build_info.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "device/fido/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace password_manager {

using webauthn::WebAuthnCredManDelegate;

class TestPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_METHOD(WebAuthnCredManDelegate*,
              GetWebAuthnCredManDelegateForDriver,
              (PasswordManagerDriver*),
              (override));
};

class CredManControllerTest : public testing::Test {
 public:
  void SetUp() override {
    client_ = std::make_unique<TestPasswordManagerClient>();
    controller_ = std::make_unique<CredManController>(client_.get());
    driver_ = std::make_unique<StubPasswordManagerDriver>();
    web_authn_cred_man_delegate_ =
        std::make_unique<WebAuthnCredManDelegate>(nullptr);

    ON_CALL(*client(), GetWebAuthnCredManDelegateForDriver)
        .WillByDefault(Return(web_authn_cred_man_delegate()));
  }

  CredManController* controller() { return controller_.get(); }
  PasswordManagerDriver* driver() { return driver_.get(); }
  WebAuthnCredManDelegate* web_authn_cred_man_delegate() {
    return web_authn_cred_man_delegate_.get();
  }
  TestPasswordManagerClient* client() { return client_.get(); }

 private:
  std::unique_ptr<CredManController> controller_;
  std::unique_ptr<TestPasswordManagerClient> client_;
  std::unique_ptr<StubPasswordManagerDriver> driver_;
  std::unique_ptr<WebAuthnCredManDelegate> web_authn_cred_man_delegate_;
};

TEST_F(CredManControllerTest, DoesNotShowIfNonWebAuthnForm) {
  ASSERT_FALSE(controller()->Show(driver(), /*is_webauthn_form=*/false));
}

TEST_F(CredManControllerTest, DoesNotShowIfFeatureDisabled) {
  ASSERT_FALSE(controller()->Show(driver(), /*is_webauthn_form=*/true));
}

TEST_F(CredManControllerTest, DoesNotShowIfNoResults) {
  base::test::ScopedFeatureList enable_feature(device::kWebAuthnAndroidCredMan);

  base::MockCallback<base::RepeatingCallback<void(bool)>>
      mock_full_assertion_request;
  web_authn_cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/false, mock_full_assertion_request.Get());

  ASSERT_FALSE(controller()->Show(driver(), /*is_webauthn_form=*/true));
}

TEST_F(CredManControllerTest, ShowIfResultsExist) {
  if (!base::android::BuildInfo::GetInstance()->is_at_least_u()) {
    return;
  }
  base::test::ScopedFeatureList enable_feature(device::kWebAuthnAndroidCredMan);

  base::MockCallback<base::RepeatingCallback<void(bool)>>
      mock_full_assertion_request;
  web_authn_cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/true, mock_full_assertion_request.Get());

  ASSERT_TRUE(controller()->Show(driver(), /*is_webauthn_form=*/true));
}

}  // namespace password_manager
