// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/web_contents_tester.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/webauthn/authenticator_request_scheduler.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/webauthn/android/webauthn_request_delegate_android.h"
#endif

using password_manager::PasskeyCredential;

namespace {

constexpr uint8_t kUserId[] = {'1', '2', '3', '4'};
constexpr char kUserName1[] = "John.Doe@example.com";
constexpr char kUserName2[] = "Jane.Doe@example.com";
constexpr uint8_t kCredId1[] = {'a', 'b', 'c', 'd'};
constexpr uint8_t kCredId2[] = {'e', 'f', 'g', 'h'};
constexpr char kRpId[] = "example.com";

PasskeyCredential CreatePasskey(std::vector<uint8_t> cred_id,
                                std::string username) {
  return PasskeyCredential(
      PasskeyCredential::Source::kAndroidPhone,
      PasskeyCredential::RpId(std::string(kRpId)),
      PasskeyCredential::CredentialId(std::move(cred_id)),
      PasskeyCredential::UserId(
          device::fido_parsing_utils::Materialize(kUserId)),
      PasskeyCredential::Username(std::move(username)));
}

}  // namespace

class ChromeWebAuthnCredentialsDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeWebAuthnCredentialsDelegateTest() = default;
  ~ChromeWebAuthnCredentialsDelegateTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

#if !BUILDFLAG(IS_ANDROID)
    authenticator_request_delegate_ =
        AuthenticatorRequestScheduler::CreateRequestDelegate(
            web_contents()->GetPrimaryMainFrame());
    // Setting the RPID creates the dialog model.
    authenticator_request_delegate_->SetRelyingPartyId("rpId");
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
    AuthenticatorRequestDialogModel::TransportAvailabilityInfo tai;
    tai.recognized_credentials = std::move(creds);
    dialog_model()->StartFlow(std::move(tai),
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
  AuthenticatorRequestDialogModel* dialog_model() {
    return authenticator_request_delegate_->GetDialogModelForTesting();
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
  std::vector<PasskeyCredential> credentials{
      CreatePasskey(device::fido_parsing_utils::Materialize(kCredId1),
                    kUserName1),
      CreatePasskey(device::fido_parsing_utils::Materialize(kCredId2),
                    kUserName2)};
  credentials_delegate()->OnCredentialsReceived(
      credentials, /*offer_passkey_from_another_device=*/true);

  auto passkeys = credentials_delegate()->GetPasskeys();
  ASSERT_TRUE(passkeys.has_value());
  EXPECT_EQ(*passkeys, credentials);
  EXPECT_TRUE(credentials_delegate()->OfferPasskeysFromAnotherDeviceOption());
}

TEST_F(ChromeWebAuthnCredentialsDelegateTest,
       DontOfferPasskeysFromAnotherDevice) {
  credentials_delegate()->OnCredentialsReceived(
      {}, /*offer_passkey_from_another_device=*/false);

  EXPECT_FALSE(credentials_delegate()->OfferPasskeysFromAnotherDeviceOption());
}

// Testing retrieving suggestions when the credentials are not received until
// afterward.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, RetrieveCredentialsDelayed) {
  std::vector<PasskeyCredential> credentials{
      CreatePasskey(device::fido_parsing_utils::Materialize(kCredId1),
                    kUserName1),
      CreatePasskey(device::fido_parsing_utils::Materialize(kCredId2),
                    kUserName2)};
  credentials_delegate()->OnCredentialsReceived(
      credentials,
      /*offer_passkey_from_another_device=*/true);

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
  std::vector<device::DiscoverableCredentialMetadata> users;
  users.emplace_back(device::AuthenticatorType::kOther, kRpId,
                     device::fido_parsing_utils::Materialize(kCredId1),
                     device::PublicKeyCredentialUserEntity(
                         device::fido_parsing_utils::Materialize(kUserId),
                         kUserName1, /*display_name=*/std::nullopt));
  users.emplace_back(device::AuthenticatorType::kOther, kRpId,
                     device::fido_parsing_utils::Materialize(kCredId2),
                     device::PublicKeyCredentialUserEntity(
                         device::fido_parsing_utils::Materialize(kUserId),
                         kUserName2, /*display_name=*/std::nullopt));

  SetCredList(users);

  std::vector<PasskeyCredential> credentials{
      CreatePasskey(device::fido_parsing_utils::Materialize(kCredId1),
                    kUserName1),
      CreatePasskey(device::fido_parsing_utils::Materialize(kCredId2),
                    kUserName2)};
  credentials_delegate()->OnCredentialsReceived(
      credentials,
      /*offer_passkey_from_another_device=*/true);

#if !BUILDFLAG(IS_ANDROID)
  base::RunLoop run_loop;
  dialog_model()->SetAccountPreselectedCallback(base::BindLambdaForTesting(
      [&](device::PublicKeyCredentialDescriptor cred) {
        EXPECT_THAT(cred.id, testing::ElementsAreArray(kCredId2));
        run_loop.Quit();
      }));
#endif

  credentials_delegate()->SelectPasskey(base::Base64Encode(kCredId2));

#if BUILDFLAG(IS_ANDROID)
  auto credential_id = GetSelectedId();
  EXPECT_THAT(*credential_id, testing::ElementsAreArray(kCredId2));
#endif
}

// Test aborting a request.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, AbortRequest) {
  std::vector<PasskeyCredential> credentials{CreatePasskey(
      device::fido_parsing_utils::Materialize(kCredId1), kUserName1)};
  credentials_delegate()->OnCredentialsReceived(
      credentials,
      /*offer_passkey_from_another_device=*/true);
  credentials_delegate()->NotifyWebAuthnRequestAborted();
  EXPECT_FALSE(credentials_delegate()->GetPasskeys());
}

// Test aborting a request when a retrieve suggestions callback is pending.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, AbortRequestPendingCallback) {
  device::test::TestCallbackReceiver<> callback;
  credentials_delegate()->RetrievePasskeys(callback.callback());
  EXPECT_FALSE(callback.was_called());
  credentials_delegate()->NotifyWebAuthnRequestAborted();
  EXPECT_TRUE(callback.was_called());
  EXPECT_FALSE(credentials_delegate()->GetPasskeys());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(ChromeWebAuthnCredentialsDelegateTest, AndroidHybridAvailability) {
  EXPECT_FALSE(credentials_delegate()->IsAndroidHybridAvailable());
  credentials_delegate()->SetAndroidHybridAvailable(
      ChromeWebAuthnCredentialsDelegate::AndroidHybridAvailable(true));
  EXPECT_TRUE(credentials_delegate()->IsAndroidHybridAvailable());
}
#endif  // BUILDFLAG(IS_ANDROID)
