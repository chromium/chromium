// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/authenticator_request_scheduler.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr uint8_t kUserId1[] = {'1', '2', '3', '4'};
constexpr uint8_t kUserId2[] = {'5', '6', '7', '8'};
constexpr char kUserName1[] = "John.Doe@example.com";
constexpr char kUserName2[] = "Jane.Doe@example.com";
constexpr char kDisplayName1[] = "John Doe";
constexpr char kDisplayName2[] = "Jane Doe";

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

}  // namespace

class ChromeWebAuthnCredentialsDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeWebAuthnCredentialsDelegateTest() = default;
  ~ChromeWebAuthnCredentialsDelegateTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    authenticator_request_delegate_ =
        AuthenticatorRequestScheduler::CreateRequestDelegate(
            web_contents()->GetMainFrame());
    // Setting the RPID creates the dialog model.
    authenticator_request_delegate_->SetRelyingPartyId("rpId");

    ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
        web_contents(), nullptr);
    credentials_delegate_ = std::make_unique<ChromeWebAuthnCredentialsDelegate>(
        ChromePasswordManagerClient::FromWebContents(web_contents()));
  }

  void TearDown() override {
    credentials_delegate_.reset();
    authenticator_request_delegate_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetCredList(std::vector<device::PublicKeyCredentialUserEntity> users) {
    std::vector<device::DiscoverableCredentialMetadata> creds;
    std::vector<uint8_t> cred_id(1);
    for (size_t i = 0; i < users.size(); i++) {
      cred_id[0] = static_cast<uint8_t>(i);
      creds.emplace_back(cred_id, std::move(users[i]));
    }

    dialog_model()->StartFlow(
        AuthenticatorRequestDialogModel::TransportAvailabilityInfo(),
        /*use_location_bar_bubble=*/true, /*prefer_native_api=*/false);
    dialog_model()->ReplaceCredListForTesting(std::move(creds));
  }

  raw_ptr<AuthenticatorRequestDialogModel> dialog_model() {
    return authenticator_request_delegate_->GetDialogModelForTesting();
  }

 protected:
  std::unique_ptr<ChromeWebAuthnCredentialsDelegate> credentials_delegate_;
  std::unique_ptr<ChromeAuthenticatorRequestDelegate>
      authenticator_request_delegate_;
};

// Testing retrieving suggestions when there are 2 public key credentials
// present.
TEST_F(ChromeWebAuthnCredentialsDelegateTest, RetrieveCredentials) {
  std::vector<device::PublicKeyCredentialUserEntity> users;
  users.emplace_back(device::PublicKeyCredentialUserEntity(
      UserId1(), UserName1(), DisplayName1(), absl::nullopt));
  users.emplace_back(device::PublicKeyCredentialUserEntity(
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
  std::vector<device::PublicKeyCredentialUserEntity> users;
  users.emplace_back(device::PublicKeyCredentialUserEntity(
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
  std::vector<device::PublicKeyCredentialUserEntity> users;
  users.emplace_back(device::PublicKeyCredentialUserEntity(
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
  std::vector<device::PublicKeyCredentialUserEntity> users;
  users.emplace_back(device::PublicKeyCredentialUserEntity(
      UserId1(), UserName1(), DisplayName1(), absl::nullopt));

  SetCredList(users);

  credentials_delegate_->SelectWebAuthnCredential("1234");

  auto account = dialog_model()->GetPreselectedAccountForTesting();

  EXPECT_TRUE(account.has_value());
  EXPECT_EQ(account->name, UserName1());
}
