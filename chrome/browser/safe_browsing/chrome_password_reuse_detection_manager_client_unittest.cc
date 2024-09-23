// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_password_reuse_detection_manager_client.h"

#include <array>
#include <string>
#include <utility>

#include "build/build_config.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/mock_password_reuse_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/safe_browsing/content/browser/password_protection/mock_password_protection_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

using safe_browsing::PasswordReuseDetectionManagerClient;

using testing::_;

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  MockPasswordManagerClient(
      password_manager::MockPasswordReuseManager* reuse_manager,
      signin::IdentityManager* identity_manager)
      : reuse_manager_(reuse_manager), identity_manager_(identity_manager) {
    ON_CALL(password_manager_, GetSubmittedCredentials)
        .WillByDefault(testing::Return(password_manager::PasswordForm{}));
    ON_CALL(*this, GetPasswordManager)
        .WillByDefault(testing::Return(&password_manager_));
    ON_CALL(*this, GetPasswordReuseManager)
        .WillByDefault(testing::Return(reuse_manager_));
    ON_CALL(*this, GetIdentityManager)
        .WillByDefault(testing::Return(identity_manager_));
  }
  MOCK_METHOD(const password_manager::PasswordManagerInterface*,
              GetPasswordManager,
              (),
              (const, override));
  MOCK_METHOD(password_manager::PasswordReuseManager*,
              GetPasswordReuseManager,
              (),
              (const, override));
  MOCK_METHOD(signin::IdentityManager*, GetIdentityManager, (), (override));
  MOCK_METHOD(
      base::CallbackListSubscription,
      RegisterStateCallbackOnHashPasswordManager,
      (const base::RepeatingCallback<void(const std::string& username)>&));

 private:
  password_manager::MockPasswordManager password_manager_;
  raw_ptr<password_manager::MockPasswordReuseManager> reuse_manager_;
  raw_ptr<signin::IdentityManager> identity_manager_;
};

// TODO(crbug.com/40895228): Refactor this unit test file. It's an
// antipattern to derive from the production class in the test. Add more tests
// to cover the .cc file.
class MockChromePasswordReuseDetectionManagerClient
    : public ChromePasswordReuseDetectionManagerClient {
 public:
  explicit MockChromePasswordReuseDetectionManagerClient(
      content::WebContents* web_contents,
      signin::IdentityManager* identity_manager = nullptr,
      password_manager::PasswordManagerClient* password_manager_client =
          nullptr)
      : ChromePasswordReuseDetectionManagerClient(web_contents,
                                                  identity_manager),
        password_manager_client_(password_manager_client) {
    password_protection_service_ =
        std::make_unique<safe_browsing::MockPasswordProtectionService>();
  }

  MockChromePasswordReuseDetectionManagerClient(
      const MockChromePasswordReuseDetectionManagerClient&) = delete;
  MockChromePasswordReuseDetectionManagerClient& operator=(
      const MockChromePasswordReuseDetectionManagerClient&) = delete;

  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override {
    return password_protection_service_.get();
  }

  safe_browsing::MockPasswordProtectionService* password_protection_service() {
    return password_protection_service_.get();
  }

  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override {
    InternalOnPrimaryAccountChanged(password_manager_client_, event_details);
  }

 private:
  std::unique_ptr<safe_browsing::MockPasswordProtectionService>
      password_protection_service_;
  raw_ptr<password_manager::PasswordManagerClient> password_manager_client_;
};

class ChromePasswordReuseDetectionManagerClientTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromePasswordReuseDetectionManagerClientTest() = default;
  ~ChromePasswordReuseDetectionManagerClientTest() override = default;

  void SetUp() override { ChromeRenderViewHostTestHarness::SetUp(); }
  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }
};

TEST_F(ChromePasswordReuseDetectionManagerClientTest, VerifySignin) {
  // Create the fake environment.
  signin::IdentityTestEnvironment identity_test_env;
  auto reuse_manager =
      std::make_unique<password_manager::MockPasswordReuseManager>();
  password_manager::StubPasswordManagerClient stub;
  auto password_manager_client = std::make_unique<MockPasswordManagerClient>(
      reuse_manager.get(), identity_test_env.identity_manager());

  std::unique_ptr<MockChromePasswordReuseDetectionManagerClient> client(
      std::make_unique<MockChromePasswordReuseDetectionManagerClient>(
          web_contents(), identity_test_env.identity_manager(),
          password_manager_client.get()));

  EXPECT_CALL(*reuse_manager, MaybeSavePasswordHash);

  // Trigger sign-in event.
  identity_test_env.SetPrimaryAccount("test_user@gmail.com",
                                      signin::ConsentLevel::kSignin);
  task_environment()->RunUntilIdle();
}

TEST_F(ChromePasswordReuseDetectionManagerClientTest,
       VerifyLogPasswordReuseDetectedEvent) {
  std::unique_ptr<MockChromePasswordReuseDetectionManagerClient> client(
      std::make_unique<MockChromePasswordReuseDetectionManagerClient>(
          web_contents()));
  EXPECT_CALL(*client->password_protection_service(),
              MaybeLogPasswordReuseDetectedEvent(web_contents()))
      .Times(1);
  client->MaybeLogPasswordReuseDetectedEvent();
}

TEST_F(ChromePasswordReuseDetectionManagerClientTest,
       VerifyMaybeProtectedPasswordEntryRequestCalled) {
  std::unique_ptr<MockChromePasswordReuseDetectionManagerClient> client(
      std::make_unique<MockChromePasswordReuseDetectionManagerClient>(
          web_contents()));

  EXPECT_CALL(
      *client->password_protection_service(),
      MaybeStartProtectedPasswordEntryRequest(_, _, "username", _, _, true))
      .Times(4);
  std::vector<password_manager::MatchingReusedCredential> credentials = {
      {"saved_domain.com", u"username"}};

  client->CheckProtectedPasswordEntry(
      password_manager::metrics_util::PasswordType::SAVED_PASSWORD, "username",
      credentials, true, 0, std::string());
  client->CheckProtectedPasswordEntry(
      password_manager::metrics_util::PasswordType::PRIMARY_ACCOUNT_PASSWORD,
      "username", credentials, true, 0, std::string());
  client->CheckProtectedPasswordEntry(
      password_manager::metrics_util::PasswordType::OTHER_GAIA_PASSWORD,
      "username", credentials, true, 0, std::string());
  client->CheckProtectedPasswordEntry(
      password_manager::metrics_util::PasswordType::ENTERPRISE_PASSWORD,
      "username", credentials, true, 0, std::string());
}

TEST_F(ChromePasswordReuseDetectionManagerClientTest,
       IsHistorySyncAccountEmail) {
  struct TestCase {
    std::string fake_sync_email;
    std::string input_username;
    bool expected_result;
  };
  std::array<TestCase, 3> kTestCases{{{"", "", false},
                                      {"", "user@example.org", false},
                                      {"user@example.org", "", false}}};

  std::unique_ptr<MockChromePasswordReuseDetectionManagerClient> client(
      std::make_unique<MockChromePasswordReuseDetectionManagerClient>(
          web_contents()));

  for (const TestCase& test : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "fake_sync_email=" << test.fake_sync_email
                 << " input_username=" << test.input_username);
    EXPECT_EQ(test.expected_result,
              client->IsHistorySyncAccountEmail(test.input_username));
  }
}
