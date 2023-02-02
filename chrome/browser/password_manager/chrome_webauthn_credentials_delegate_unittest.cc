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

constexpr uint8_t kUserId1[] = {'1', '2', '3', '4'};
constexpr uint8_t kUserId2[] = {'5', '6', '7', '8'};
constexpr char kUserName1[] = "John.Doe@example.com";
constexpr char kUserName2[] = "Jane.Doe@example.com";
constexpr char kDisplayName1[] = "John Doe";
constexpr char kDisplayName2[] = "Jane Doe";
constexpr uint8_t kCredId1[] = {'a', 'b', 'c', 'd'};
constexpr uint8_t kCredId2[] = {'e', 'f', 'g', 'h'};
constexpr char kRpId[] = "example.com";

std::vector<uint8_t> UserId1() {
  return std::vector<uint8_t>(std::begin(kUserId1), std::end(kUserId1));
}
std::vector<uint8_t> UserId2() {
  return std::vector<uint8_t>(std::begin(kUserId2), std::end(kUserId2));
}
std::string UserName1() {
  return std::string(kUserName1);
}
std::string UserName2() {
  return std::string(kUserName2);
}
std::string DisplayName1() {
  return std::string(kDisplayName1);
}
std::string DisplayName2() {
  return std::string(kDisplayName2);
}

std::vector<uint8_t> CredId1() {
  return std::vector<uint8_t>(std::begin(kCredId1), std::end(kCredId1));
}
std::vector<uint8_t> CredId2() {
  return std::vector<uint8_t>(std::begin(kCredId2), std::end(kCredId2));
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
        /*is_conditional_mediation=*/true, /*prefer_native_api=*/false);
    dialog_model()->ReplaceCredListForTesting(std::move(creds));
#else
    delegate_->OnWebAuthnRequestPending(
        main_rfh(), creds, /*is_conditional_request=*/true,
        base::BindOnce(
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
  std::vector<device::DiscoverableCredentialMetadata> users;
  users.emplace_back(kRpId, CredId1(),
                     device::PublicKeyCredentialUserEntity(
                         UserId1(), UserName1(), DisplayName1()));
  users.emplace_back(kRpId, CredId2(),
                     device::PublicKeyCredentialUserEntity(
                         UserId2(), UserName2(), DisplayName2()));

  credentials_delegate_->OnCredentialsReceived(users);

  auto passkeys = credentials_delegate_->GetPasskeys();
  ASSERT_TRUE(passkeys.has_value());
  EXPECT_THAT(
      *passkeys,
      testing::ElementsAre(
          PasskeyCredential(
              PasskeyCredential::Username(base::UTF8ToUTF16(UserName1())),
              PasskeyCredential::BackendId(base::Base64Encode(CredId1()))),
          PasskeyCredential(
              PasskeyCredential::Username(base::UTF8ToUTF16(UserName2())),
              PasskeyCredential::BackendId(base::Base64Encode(CredId2())))));
}

// Testing retrieving suggestions when the credentials are not received until
// afterward.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, RetrieveCredentialsDelayed) {
  std::vector<device::DiscoverableCredentialMetadata> users;
  users.emplace_back(kRpId, CredId1(),
                     device::PublicKeyCredentialUserEntity(
                         UserId1(), UserName1(), DisplayName1()));
  users.emplace_back(kRpId, CredId2(),
                     device::PublicKeyCredentialUserEntity(
                         UserId2(), UserName2(), DisplayName2()));

  credentials_delegate_->OnCredentialsReceived(users);

  auto passkeys = credentials_delegate_->GetPasskeys();
  ASSERT_TRUE(passkeys.has_value());
  EXPECT_THAT(
      *passkeys,
      testing::ElementsAre(
          PasskeyCredential(
              PasskeyCredential::Username(base::UTF8ToUTF16(UserName1())),
              PasskeyCredential::BackendId(base::Base64Encode(CredId1()))),
          PasskeyCredential(
              PasskeyCredential::Username(base::UTF8ToUTF16(UserName2())),
              PasskeyCredential::BackendId(base::Base64Encode(CredId2())))));
}

// Testing retrieving suggestions when there are no public key credentials
// present.
TEST_F(ChromeWebAuthnCredentialsDelegateTest,
       RetrieveCredentialsWithEmptyList) {
  auto suggestions = credentials_delegate_->GetPasskeys();
  EXPECT_FALSE(suggestions.has_value());
}

// Testing retrieving suggestions when there is a public key credential present
// with missing user name.
TEST_F(ChromeWebAuthnCredentialsDelegateTest,
       RetrieveCredentialWithNoUserName) {
  const std::u16string kErrorLabel =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
  std::vector<device::DiscoverableCredentialMetadata> users;
  users.emplace_back(kRpId, CredId1(),
                     device::PublicKeyCredentialUserEntity(
                         UserId1(), absl::nullopt, DisplayName1()));

  credentials_delegate_->OnCredentialsReceived(users);

  auto passkeys = credentials_delegate_->GetPasskeys();
  ASSERT_TRUE(passkeys.has_value());
  EXPECT_THAT(
      *passkeys,
      testing::ElementsAre(PasskeyCredential(
          PasskeyCredential::Username(kErrorLabel),
          PasskeyCredential::BackendId(base::Base64Encode(CredId1())))));
}

// Testing selection of a credential.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, SelectCredential) {
  std::vector<device::DiscoverableCredentialMetadata> users;
  users.emplace_back(kRpId, CredId1(),
                     device::PublicKeyCredentialUserEntity(
                         UserId1(), UserName1(), DisplayName1()));
  users.emplace_back(kRpId, CredId2(),
                     device::PublicKeyCredentialUserEntity(
                         UserId2(), UserName2(), DisplayName2()));

  SetCredList(users);
  credentials_delegate_->OnCredentialsReceived(users);

#if !BUILDFLAG(IS_ANDROID)
  base::RunLoop run_loop;
  dialog_model()->SetAccountPreselectedCallback(
      base::BindLambdaForTesting([&](std::vector<uint8_t> credential_id) {
        EXPECT_EQ(credential_id, CredId2());
        run_loop.Quit();
      }));
#endif

  credentials_delegate_->SelectPasskey(base::Base64Encode(CredId2()));

#if BUILDFLAG(IS_ANDROID)
  auto credential_id = GetSelectedId();
  EXPECT_EQ(credential_id, CredId2());
#endif
}

// Test aborting a request.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, AbortRequest) {
  std::vector<device::DiscoverableCredentialMetadata> users;
  users.emplace_back(kRpId, CredId1(),
                     device::PublicKeyCredentialUserEntity(
                         UserId1(), UserName1(), DisplayName1()));
  credentials_delegate_->OnCredentialsReceived(users);
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
