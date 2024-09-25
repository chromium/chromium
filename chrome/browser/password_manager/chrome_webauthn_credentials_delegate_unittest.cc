// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"
#include "content/public/test/web_contents_tester.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_controller.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/authenticator_request_scheduler.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "device/fido/features.h"
#include "device/fido/fido_request_handler_base.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
#include "base/memory/raw_ptr.h"
#include "chrome/browser/webauthn/android/webauthn_request_delegate_android.h"
#endif

namespace {

using password_manager::PasskeyCredential;
using OnPasskeySelectedCallback =
    password_manager::WebAuthnCredentialsDelegate::OnPasskeySelectedCallback;
using SecurityKeyOrHybridFlowAvailable =
    ChromeWebAuthnCredentialsDelegate::SecurityKeyOrHybridFlowAvailable;

constexpr uint8_t kUserId[] = {'1', '2', '3', '4'};
constexpr char kUserName1[] = "John.Doe@example.com";
constexpr char kUserName2[] = "Jane.Doe@example.com";
constexpr uint8_t kCredId1[] = {'a', 'b', 'c', 'd'};
constexpr uint8_t kCredId2[] = {'e', 'f', 'g', 'h'};
constexpr uint8_t kCredIdGpm[] = {'a', 'd', 'e', 'm'};
constexpr char kRpId[] = "example.com";
const device::DiscoverableCredentialMetadata user1{
    device::AuthenticatorType::kOther, kRpId,
    device::fido_parsing_utils::Materialize(kCredId1),
    device::PublicKeyCredentialUserEntity(
        device::fido_parsing_utils::Materialize(kUserId),
        kUserName1,
        /*display_name=*/std::nullopt)};
const device::DiscoverableCredentialMetadata user2{
    device::AuthenticatorType::kOther, kRpId,
    device::fido_parsing_utils::Materialize(kCredId2),
    device::PublicKeyCredentialUserEntity(
        device::fido_parsing_utils::Materialize(kUserId),
        kUserName2,
        /*display_name=*/std::nullopt)};
const device::DiscoverableCredentialMetadata userGpm{
#if BUILDFLAG(IS_CHROMEOS)
    device::AuthenticatorType::kChromeOSPasskeys,
#else
    device::AuthenticatorType::kEnclave,
#endif
    kRpId, device::fido_parsing_utils::Materialize(kCredIdGpm),
    device::PublicKeyCredentialUserEntity(
        device::fido_parsing_utils::Materialize(kUserId),
        kUserName1,
        /*display_name=*/std::nullopt)};

PasskeyCredential CreatePasskey(std::vector<uint8_t> cred_id,
                                std::string username,
                                PasskeyCredential::Source source =
                                    PasskeyCredential::Source::kAndroidPhone) {
  return PasskeyCredential(
      source, PasskeyCredential::RpId(std::string(kRpId)),
      PasskeyCredential::CredentialId(std::move(cred_id)),
      PasskeyCredential::UserId(
          device::fido_parsing_utils::Materialize(kUserId)),
      PasskeyCredential::Username(std::move(username)));
}

const PasskeyCredential passkey1 =
    CreatePasskey(device::fido_parsing_utils::Materialize(kCredId1),
                  kUserName1);
const PasskeyCredential passkey2 =
    CreatePasskey(device::fido_parsing_utils::Materialize(kCredId2),
                  kUserName2);
const PasskeyCredential passkeyGpm =
    CreatePasskey(device::fido_parsing_utils::Materialize(kCredIdGpm),
                  kUserName1,
                  PasskeyCredential::Source::kGooglePasswordManager);

}  // namespace

class ChromeWebAuthnCredentialsDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeWebAuthnCredentialsDelegateTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ChromeWebAuthnCredentialsDelegateTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

#if !BUILDFLAG(IS_ANDROID)
    authenticator_request_delegate_ =
        AuthenticatorRequestScheduler::CreateRequestDelegate(
            web_contents()->GetPrimaryMainFrame());
    // Setting the RPID creates the dialog model.
    authenticator_request_delegate_->SetRelyingPartyId("rpId");
    authenticator_request_delegate_->RegisterActionCallbacks(
        base::DoNothing(), base::DoNothing(), base::DoNothing(),
        base::DoNothing(), base::DoNothing(), base::DoNothing());
#else
    delegate_ =
        WebAuthnRequestDelegateAndroid::GetRequestDelegate(web_contents());
#endif

    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("https://example.com"));
  }

  void TearDown() override {
#if !BUILDFLAG(IS_ANDROID)
    authenticator_request_delegate_.reset();
#endif

    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetCredList(std::vector<device::DiscoverableCredentialMetadata> creds) {
#if !BUILDFLAG(IS_ANDROID)
    device::FidoRequestHandlerBase::TransportAvailabilityInfo tai;
    tai.request_type = device::FidoRequestType::kGetAssertion;
    tai.recognized_credentials = std::move(creds);
    dialog_controller()->StartFlow(std::move(tai),
                                   /*is_conditional_mediation=*/true);
#else
    delegate_->OnWebAuthnRequestPending(
        main_rfh(), creds, /*is_conditional_request=*/true,
        base::BindRepeating(
            &ChromeWebAuthnCredentialsDelegateTest::OnAccountSelected,
            base::Unretained(this)),
        /*hybrid_callback=*/base::RepeatingClosure());
#endif
  }

#if !BUILDFLAG(IS_ANDROID)
  AuthenticatorRequestDialogController* dialog_controller() {
    return authenticator_request_delegate_->dialog_controller();
  }
  AuthenticatorRequestDialogModel* model() {
    return authenticator_request_delegate_->dialog_model();
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  void OnAccountSelected(const std::vector<uint8_t>& id) {
    selected_id_ = std::move(id);
  }

  std::optional<std::vector<uint8_t>> GetSelectedId() {
    return std::move(selected_id_);
  }
#endif

 protected:
  ChromeWebAuthnCredentialsDelegate* credentials_delegate() {
    return ChromeWebAuthnCredentialsDelegateFactory::GetFactory(web_contents())
        ->GetDelegateForFrame(web_contents()->GetPrimaryMainFrame());
  }
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<ChromeAuthenticatorRequestDelegate>
      authenticator_request_delegate_;
#else
  raw_ptr<WebAuthnRequestDelegateAndroid> delegate_;
  std::optional<std::vector<uint8_t>> selected_id_;
#endif
};

// Testing retrieving passkeys when there are 2 public key credentials
// present.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, RetrieveCredentials) {
  std::vector<PasskeyCredential> credentials{passkey1, passkey2};
  credentials_delegate()->OnCredentialsReceived(
      credentials, SecurityKeyOrHybridFlowAvailable(true));

  auto passkeys = credentials_delegate()->GetPasskeys();
  ASSERT_TRUE(passkeys.has_value());
  EXPECT_EQ(*passkeys, credentials);
  EXPECT_TRUE(credentials_delegate()->IsSecurityKeyOrHybridFlowAvailable());
}

TEST_F(ChromeWebAuthnCredentialsDelegateTest,
       DontOfferPasskeysFromAnotherDevice) {
  credentials_delegate()->OnCredentialsReceived(
      {}, SecurityKeyOrHybridFlowAvailable(false));

  EXPECT_FALSE(credentials_delegate()->IsSecurityKeyOrHybridFlowAvailable());
}

// Testing retrieving suggestions when the credentials are not received until
// afterward.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, RetrieveCredentialsDelayed) {
  std::vector<PasskeyCredential> credentials{passkey1, passkey2};
  credentials_delegate()->OnCredentialsReceived(
      credentials, SecurityKeyOrHybridFlowAvailable(true));

  auto passkeys = credentials_delegate()->GetPasskeys();
  ASSERT_TRUE(passkeys.has_value());
  EXPECT_EQ(*passkeys, credentials);
}

// Testing retrieving suggestions when there are no public key credentials
// present.
TEST_F(ChromeWebAuthnCredentialsDelegateTest,
       RetrieveCredentialsWithEmptyList) {
  auto suggestions = credentials_delegate()->GetPasskeys();
  EXPECT_FALSE(suggestions.has_value());
}

// Testing selection of a credential.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, SelectCredential) {
  base::MockCallback<
      password_manager::WebAuthnCredentialsDelegate::OnPasskeySelectedCallback>
      mock_callback;
  SetCredList({user1, user2});

  credentials_delegate()->OnCredentialsReceived(
      {passkey1, passkey2}, SecurityKeyOrHybridFlowAvailable(true));

#if !BUILDFLAG(IS_ANDROID)
  base::RunLoop run_loop;
  dialog_controller()->SetAccountPreselectedCallback(base::BindLambdaForTesting(
      [&](device::DiscoverableCredentialMetadata cred) {
        EXPECT_THAT(cred.cred_id, testing::ElementsAreArray(kCredId2));
        run_loop.Quit();
      }));
#endif

  EXPECT_CALL(mock_callback, Run());
  credentials_delegate()->SelectPasskey(base::Base64Encode(kCredId2),
                                        mock_callback.Get());

#if BUILDFLAG(IS_ANDROID)
  auto credential_id = GetSelectedId();
  EXPECT_THAT(*credential_id, testing::ElementsAreArray(kCredId2));
#endif
}

// Test aborting a request.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, AbortRequest) {
  credentials_delegate()->OnCredentialsReceived(
      {passkey1}, SecurityKeyOrHybridFlowAvailable(true));
  credentials_delegate()->NotifyWebAuthnRequestAborted();
  EXPECT_FALSE(credentials_delegate()->GetPasskeys());
}

// Test aborting a request when a retrieve suggestions callback is pending.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, AbortRequestPendingCallback) {
  base::test::TestFuture<void> future;
  credentials_delegate()->RetrievePasskeys(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  credentials_delegate()->NotifyWebAuthnRequestAborted();
  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(credentials_delegate()->GetPasskeys());
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeWebAuthnCredentialsDelegateTest,
       OnStepTransitionCallbackOtherSource) {
  base::MockCallback<OnPasskeySelectedCallback> mock_callback;
  SetCredList({user1});
  credentials_delegate()->OnCredentialsReceived(
      {passkey1}, SecurityKeyOrHybridFlowAvailable(true));
  dialog_controller()->SetAccountPreselectedCallback(base::DoNothing());
  EXPECT_CALL(mock_callback, Run()).Times(1);
  credentials_delegate()->SelectPasskey(base::Base64Encode(kCredId1),
                                        mock_callback.Get());
}

class GpmPasskeyChromeWebAuthnCredentialsDelegateTest
    : public ChromeWebAuthnCredentialsDelegateTest {
 private:
  base::test::ScopedFeatureList enabled{
#if BUILDFLAG(IS_CHROMEOS)
      device::kChromeOsPasskeys
#else
      device::kWebAuthnEnclaveAuthenticator
#endif
  };
};

// Regression test for crbug.com/346263461.
TEST_F(GpmPasskeyChromeWebAuthnCredentialsDelegateTest,
       IgnoreRepeatedSelectPasskey) {
  base::MockCallback<OnPasskeySelectedCallback> mock_callback;
  SetCredList({userGpm});
  credentials_delegate()->OnCredentialsReceived(
      {passkeyGpm}, SecurityKeyOrHybridFlowAvailable(true));
  dialog_controller()->SetAccountPreselectedCallback(base::DoNothing());
  EXPECT_CALL(mock_callback, Run()).Times(0);
  credentials_delegate()->SelectPasskey(base::Base64Encode(kCredIdGpm),
                                        mock_callback.Get());
  credentials_delegate()->SelectPasskey(base::Base64Encode(kCredIdGpm),
                                        mock_callback.Get());
}

TEST_F(GpmPasskeyChromeWebAuthnCredentialsDelegateTest,
       OnStepTransitionCallbackGpmSource) {
  base::MockCallback<OnPasskeySelectedCallback> mock_callback;
  SetCredList({userGpm});
  credentials_delegate()->OnCredentialsReceived(
      {passkeyGpm}, SecurityKeyOrHybridFlowAvailable(true));
  dialog_controller()->SetAccountPreselectedCallback(base::DoNothing());
  EXPECT_CALL(mock_callback, Run()).Times(0);
  credentials_delegate()->SelectPasskey(base::Base64Encode(kCredIdGpm),
                                        mock_callback.Get());
}

TEST_F(GpmPasskeyChromeWebAuthnCredentialsDelegateTest,
       OnStepTransitionCallbackGpmSourceAndUiNotDisabled) {
  base::MockCallback<OnPasskeySelectedCallback> mock_callback;
  SetCredList({userGpm});
  credentials_delegate()->OnCredentialsReceived(
      {passkeyGpm}, SecurityKeyOrHybridFlowAvailable(true));
  dialog_controller()->SetAccountPreselectedCallback(base::DoNothing());
  EXPECT_CALL(mock_callback, Run()).Times(0);
  credentials_delegate()->SelectPasskey(base::Base64Encode(kCredIdGpm),
                                        mock_callback.Get());

  model()->ui_disabled_ = true;
  credentials_delegate()->OnStepTransition();
  EXPECT_CALL(mock_callback, Run()).Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(350));

  model()->ui_disabled_ = false;
  credentials_delegate()->OnStepTransition();
  EXPECT_CALL(mock_callback, Run()).Times(1);
  task_environment()->FastForwardBy(base::Milliseconds(350));
}
#endif  // !BUILDFLAG(IS_ANDROID)
