// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"

#include <memory>
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
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  return PasskeyCredential(PasskeyCredential::Source::kAndroidPhone,
                           std::string(kRpId), std::move(cred_id),
                           device::fido_parsing_utils::Materialize(kUserId),
                           std::move(username));
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

    credentials_delegate_ =
        ChromeWebAuthnCredentialsDelegateFactory::GetFactory(web_contents())
            ->GetDelegateForFrame(web_contents()->GetPrimaryMainFrame());
  }

  void TearDown() override {
#if !BUILDFLAG(IS_ANDROID)
    authenticator_request_delegate_.reset();
#endif

    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetCredList(std::vector<device::DiscoverableCredentialMetadata> creds) {
#if !BUILDFLAG(IS_ANDROID)
    dialog_model()->StartFlow(
        AuthenticatorRequestDialogModel::TransportAvailabilityInfo(),
        /*is_conditional_mediation=*/true);
    dialog_model()->ReplaceCredListForTesting(std::move(creds));
#else
    delegate_->OnWebAuthnRequestPending(
        main_rfh(), creds, /*is_conditional_request=*/true,
        base::BindRepeating(
            &ChromeWebAuthnCredentialsDelegateTest::OnAccountSelected,
            base::Unretained(this)));
#endif
  }

#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<AuthenticatorRequestDialogModel> dialog_model() {
    return authenticator_request_delegate_->GetDialogModelForTesting();
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  void OnAccountSelected(const std::vector<uint8_t>& id) {
    selected_id_ = std::move(id);
  }

  absl::optional<std::vector<uint8_t>> GetSelectedId() {
    return std::move(selected_id_);
  }
#endif

 protected:
  raw_ptr<ChromeWebAuthnCredentialsDelegate> credentials_delegate_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<ChromeAuthenticatorRequestDelegate>
      authenticator_request_delegate_;
#else
  raw_ptr<WebAuthnRequestDelegateAndroid> delegate_;
  absl::optional<std::vector<uint8_t>> selected_id_;
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
  credentials_delegate_->OnCredentialsReceived(credentials);

  auto passkeys = credentials_delegate_->GetPasskeys();
  ASSERT_TRUE(passkeys.has_value());
  EXPECT_EQ(*passkeys, credentials);
}

// Testing retrieving suggestions when the credentials are not received until
// afterward.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, RetrieveCredentialsDelayed) {
  std::vector<PasskeyCredential> credentials{
      CreatePasskey(device::fido_parsing_utils::Materialize(kCredId1),
                    kUserName1),
      CreatePasskey(device::fido_parsing_utils::Materialize(kCredId2),
                    kUserName2)};
  credentials_delegate_->OnCredentialsReceived(credentials);

  auto passkeys = credentials_delegate_->GetPasskeys();
  ASSERT_TRUE(passkeys.has_value());
  EXPECT_EQ(*passkeys, credentials);
}

// Testing retrieving suggestions when there are no public key credentials
// present.
TEST_F(ChromeWebAuthnCredentialsDelegateTest,
       RetrieveCredentialsWithEmptyList) {
  auto suggestions = credentials_delegate_->GetPasskeys();
  EXPECT_FALSE(suggestions.has_value());
}

// Testing selection of a credential.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, SelectCredential) {
  std::vector<device::DiscoverableCredentialMetadata> users;
  users.emplace_back(device::AuthenticatorType::kOther, kRpId,
                     device::fido_parsing_utils::Materialize(kCredId1),
                     device::PublicKeyCredentialUserEntity(
                         device::fido_parsing_utils::Materialize(kUserId),
                         kUserName1, /*display_name=*/absl::nullopt));
  users.emplace_back(device::AuthenticatorType::kOther, kRpId,
                     device::fido_parsing_utils::Materialize(kCredId2),
                     device::PublicKeyCredentialUserEntity(
                         device::fido_parsing_utils::Materialize(kUserId),
                         kUserName2, /*display_name=*/absl::nullopt));

  SetCredList(users);

  std::vector<PasskeyCredential> credentials{
      CreatePasskey(device::fido_parsing_utils::Materialize(kCredId1),
                    kUserName1),
      CreatePasskey(device::fido_parsing_utils::Materialize(kCredId2),
                    kUserName2)};
  credentials_delegate_->OnCredentialsReceived(credentials);

#if !BUILDFLAG(IS_ANDROID)
  base::RunLoop run_loop;
  dialog_model()->SetAccountPreselectedCallback(
      base::BindLambdaForTesting([&](std::vector<uint8_t> credential_id) {
        EXPECT_THAT(credential_id, testing::ElementsAreArray(kCredId2));
        run_loop.Quit();
      }));
#endif

  credentials_delegate_->SelectPasskey(base::Base64Encode(kCredId2));

#if BUILDFLAG(IS_ANDROID)
  auto credential_id = GetSelectedId();
  EXPECT_THAT(*credential_id, testing::ElementsAreArray(kCredId2));
#endif
}

// Test aborting a request.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, AbortRequest) {
  std::vector<PasskeyCredential> credentials{CreatePasskey(
      device::fido_parsing_utils::Materialize(kCredId1), kUserName1)};
  credentials_delegate_->OnCredentialsReceived(credentials);
  credentials_delegate_->NotifyWebAuthnRequestAborted();
  EXPECT_FALSE(credentials_delegate_->GetPasskeys());
}

// Test aborting a request when a retrieve suggestions callback is pending.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, AbortRequestPendingCallback) {
  device::test::TestCallbackReceiver<> callback;
  credentials_delegate_->RetrievePasskeys(callback.callback());
  EXPECT_FALSE(callback.was_called());
  credentials_delegate_->NotifyWebAuthnRequestAborted();
  EXPECT_TRUE(callback.was_called());
  EXPECT_FALSE(credentials_delegate_->GetPasskeys());
}
