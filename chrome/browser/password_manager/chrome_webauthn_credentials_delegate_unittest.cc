// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/webauthn/authenticator_request_scheduler.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/webauthn/android/conditional_ui_delegate_android.h"
#endif

namespace {

constexpr uint8_t kUserId1[] = {'1', '2', '3', '4'};
constexpr uint8_t kUserId2[] = {'5', '6', '7', '8'};
constexpr char kUserName1[] = "John.Doe@example.com";
constexpr char kUserName2[] = "Jane.Doe@example.com";
constexpr char kDisplayName1[] = "John Doe";
constexpr char kDisplayName2[] = "Jane Doe";
constexpr uint8_t kCredId1[] = {'a', 'b', 'c', 'd'};
constexpr uint8_t kCredId2[] = {'e', 'f', 'g', 'h'};

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
        ConditionalUiDelegateAndroid::GetConditionalUiDelegate(web_contents());
#endif

    ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
        web_contents(), nullptr);
    credentials_delegate_ = std::make_unique<ChromeWebAuthnCredentialsDelegate>(
        ChromePasswordManagerClient::FromWebContents(web_contents()));
  }

  void TearDown() override {
    credentials_delegate_.reset();

#if !BUILDFLAG(IS_ANDROID)
    authenticator_request_delegate_.reset();
#endif

    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetCredList(std::vector<device::DiscoverableCredentialMetadata> creds) {
#if !BUILDFLAG(IS_ANDROID)
    dialog_model()->StartFlow(
        AuthenticatorRequestDialogModel::TransportAvailabilityInfo(),
        /*use_location_bar_bubble=*/true, /*prefer_native_api=*/false);
    dialog_model()->ReplaceCredListForTesting(std::move(creds));
#else
    delegate_->OnWebAuthnRequestPending(
        creds, base::BindOnce(
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
  std::unique_ptr<ChromeWebAuthnCredentialsDelegate> credentials_delegate_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<ChromeAuthenticatorRequestDelegate>
      authenticator_request_delegate_;
#else
  raw_ptr<ConditionalUiDelegateAndroid> delegate_;
  absl::optional<std::vector<uint8_t>> selected_id_;
#endif
};

// Testing retrieving suggestions when there are 2 public key credentials
// present.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, RetrieveCredentials) {
  std::vector<device::DiscoverableCredentialMetadata> users;
  users.emplace_back(
      CredId1(), device::PublicKeyCredentialUserEntity(
                     UserId1(), UserName1(), DisplayName1(), absl::nullopt));
  users.emplace_back(
      CredId2(), device::PublicKeyCredentialUserEntity(
                     UserId2(), UserName2(), DisplayName2(), absl::nullopt));

  SetCredList(users);

  credentials_delegate_->RetrieveWebAuthnSuggestions(base::BindOnce([]() {}));
  task_environment()->RunUntilIdle();

  auto suggestions = credentials_delegate_->GetWebAuthnSuggestions();
  EXPECT_EQ(suggestions.size(), 2u);
  EXPECT_EQ(suggestions[0].main_text.value, base::UTF8ToUTF16(DisplayName1()));
  EXPECT_EQ(suggestions[0].label, base::UTF8ToUTF16(UserName1()));
  EXPECT_EQ(suggestions[1].main_text.value, base::UTF8ToUTF16(DisplayName2()));
  EXPECT_EQ(suggestions[1].label, base::UTF8ToUTF16(UserName2()));
}

// Testing retrieving suggestions when there are no public key credentials
// present.
TEST_F(ChromeWebAuthnCredentialsDelegateTest,
       RetrieveCredentialsWithEmptyList) {
  credentials_delegate_->RetrieveWebAuthnSuggestions(base::BindOnce([]() {}));
  task_environment()->RunUntilIdle();

  auto suggestions = credentials_delegate_->GetWebAuthnSuggestions();
  EXPECT_EQ(suggestions.size(), 0u);
}

// Testing retrieving suggestions when there is a public key credential present
// with no display name.
TEST_F(ChromeWebAuthnCredentialsDelegateTest,
       RetrieveCredentialsWithEmptyDisplayName) {
  std::vector<device::DiscoverableCredentialMetadata> users;
  users.emplace_back(CredId1(),
                     device::PublicKeyCredentialUserEntity(
                         UserId1(), UserName1(), std::string(), absl::nullopt));

  SetCredList(users);

  credentials_delegate_->RetrieveWebAuthnSuggestions(base::BindOnce([]() {}));
  task_environment()->RunUntilIdle();

  auto suggestions = credentials_delegate_->GetWebAuthnSuggestions();
  std::u16string error_string = u"Unknown account";
  EXPECT_EQ(suggestions.size(), 1u);
  EXPECT_EQ(suggestions[0].main_text.value, error_string);
  EXPECT_EQ(suggestions[0].label, base::UTF8ToUTF16(UserName1()));
}

// Testing retrieving suggestions when there is a public key credential present
// with missing user name.
TEST_F(ChromeWebAuthnCredentialsDelegateTest,
       RetrieveCredentialWithNoUserName) {
  std::vector<device::DiscoverableCredentialMetadata> users;
  users.emplace_back(
      CredId1(), device::PublicKeyCredentialUserEntity(
                     UserId1(), absl::nullopt, DisplayName1(), absl::nullopt));

  SetCredList(users);

  credentials_delegate_->RetrieveWebAuthnSuggestions(base::BindOnce([]() {}));
  task_environment()->RunUntilIdle();

  auto suggestions = credentials_delegate_->GetWebAuthnSuggestions();
  EXPECT_EQ(suggestions.size(), 1u);
  EXPECT_EQ(suggestions[0].main_text.value, base::UTF8ToUTF16(DisplayName1()));
  EXPECT_EQ(suggestions[0].label, std::u16string());
}

// Testing selection of a credential.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, SelectCredential) {
  std::vector<device::DiscoverableCredentialMetadata> users;
  users.emplace_back(
      CredId1(), device::PublicKeyCredentialUserEntity(
                     UserId1(), UserName1(), DisplayName1(), absl::nullopt));
  users.emplace_back(
      CredId2(), device::PublicKeyCredentialUserEntity(
                     UserId2(), UserName2(), DisplayName2(), absl::nullopt));
  SetCredList(users);

#if !BUILDFLAG(IS_ANDROID)
  base::RunLoop run_loop;
  dialog_model()->SetAccountPreselectedCallback(
      base::BindLambdaForTesting([&](std::vector<uint8_t> credential_id) {
        EXPECT_EQ(credential_id, CredId2());
        run_loop.Quit();
      }));
  // Select the credential for User2.
  credentials_delegate_->SelectWebAuthnCredential("5678");
  run_loop.Run();
#else
  // On Android, the credential ID is the suggestion backend_id, whereas on
  // desktop it is the user ID.
  credentials_delegate_->SelectWebAuthnCredential(
      base::Base64Encode(CredId2()));
  auto credential_id = GetSelectedId();
  EXPECT_EQ(credential_id, CredId2());
#endif
}
