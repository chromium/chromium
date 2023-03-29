// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_password_reuse_detection_manager_client.h"

#include <string>
#include <utility>

#include "build/build_config.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/safe_browsing/content/browser/password_protection/mock_password_protection_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

using safe_browsing::PasswordReuseDetectionManagerClient;

using testing::_;

// TODO(https://crbug.com/1419602): Refactor this unit test file. It's an
// antipattern to derive from the production class in the test. Add more tests
// to cover the .cc file.
class MockChromePasswordReuseDetectionManagerClient
    : public ChromePasswordReuseDetectionManagerClient {
 public:
  explicit MockChromePasswordReuseDetectionManagerClient(
      content::WebContents* web_contents)
      : ChromePasswordReuseDetectionManagerClient(web_contents) {
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

 private:
  std::unique_ptr<safe_browsing::MockPasswordProtectionService>
      password_protection_service_;
};

class ChromePasswordReuseDetectionManagerClientTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromePasswordReuseDetectionManagerClientTest() = default;
  ~ChromePasswordReuseDetectionManagerClientTest() override = default;

  void SetUp() override;
};

void ChromePasswordReuseDetectionManagerClientTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  ChromePasswordReuseDetectionManagerClient::CreateForWebContents(
      web_contents());
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

TEST_F(ChromePasswordReuseDetectionManagerClientTest, IsSyncAccountEmail) {
  const struct {
    std::string fake_sync_email;
    std::string input_username;
    bool expected_result;
  } kTestCases[] = {{"", "", false},
                    {"", "user@example.org", false},
                    {"user@example.org", "", false}};

  std::unique_ptr<MockChromePasswordReuseDetectionManagerClient> client(
      std::make_unique<MockChromePasswordReuseDetectionManagerClient>(
          web_contents()));

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    EXPECT_EQ(kTestCases[i].expected_result,
              client->IsSyncAccountEmail(kTestCases[i].input_username));
  }
}
