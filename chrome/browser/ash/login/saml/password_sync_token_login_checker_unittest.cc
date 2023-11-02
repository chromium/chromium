// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/password_sync_token_login_checker.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/default_clock.h"
#include "chrome/browser/ash/login/saml/password_sync_token_checkers_collection.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace {

const char kSAMLUserId[] = "12345";
const char kSAMLUserEmail[] = "alice@corp.example.com";

const char kSyncToken[] = "sync-token-1";

constexpr base::TimeDelta kSamlTokenDelay = base::Seconds(60);

class FakeUserManagerWithLocalState : public FakeChromeUserManager {
 public:
  FakeUserManagerWithLocalState()
      : test_local_state_(std::make_unique<TestingPrefServiceSimple>()) {
    RegisterPrefs(test_local_state_->registry());
  }
  ~FakeUserManagerWithLocalState() override = default;

  PrefService* GetLocalState() const override {
    return test_local_state_.get();
  }

 private:
  std::unique_ptr<TestingPrefServiceSimple> test_local_state_;
};

}  // namespace

class PasswordSyncTokenLoginCheckerTest : public testing::Test {
 protected:
  PasswordSyncTokenLoginCheckerTest();

  void CreatePasswordSyncTokenLoginChecker();
  void DestroyPasswordSyncTokenLoginChecker();
  void OnTokenVerified(bool is_verified);

  const AccountId saml_login_account_id_ =
      AccountId::FromUserEmailGaiaId(kSAMLUserEmail, kSAMLUserId);

  content::BrowserTaskEnvironment test_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<net::BackoffEntry> sync_token_retry_backoff_;
  FakeChromeUserManager* user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<PasswordSyncTokenLoginChecker> checker_;
};

PasswordSyncTokenLoginCheckerTest::PasswordSyncTokenLoginCheckerTest() {
  std::unique_ptr<FakeChromeUserManager> fake_user_manager =
      std::make_unique<FakeUserManagerWithLocalState>();
  scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
      std::move(fake_user_manager));

  sync_token_retry_backoff_ = std::make_unique<net::BackoffEntry>(
      &PasswordSyncTokenCheckersCollection::kFetchTokenRetryBackoffPolicy);
  user_manager_ =
      static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
  user_manager_->AddUser(saml_login_account_id_);
  user_manager_->SwitchActiveUser(saml_login_account_id_);
}

void PasswordSyncTokenLoginCheckerTest::CreatePasswordSyncTokenLoginChecker() {
  DestroyPasswordSyncTokenLoginChecker();
  checker_ = std::make_unique<PasswordSyncTokenLoginChecker>(
      saml_login_account_id_, kSyncToken, sync_token_retry_backoff_.get());
}

void PasswordSyncTokenLoginCheckerTest::DestroyPasswordSyncTokenLoginChecker() {
  checker_.reset();
}

void PasswordSyncTokenLoginCheckerTest::OnTokenVerified(bool is_verified) {
  checker_->OnTokenVerified(is_verified);
}

TEST_F(PasswordSyncTokenLoginCheckerTest, SyncTokenValid) {
  CreatePasswordSyncTokenLoginChecker();
  checker_->CheckForPasswordNotInSync();
  OnTokenVerified(true);
  EXPECT_FALSE(
      user_manager_->FindUser(saml_login_account_id_)->force_online_signin());
  test_environment_.FastForwardBy(kSamlTokenDelay);
  EXPECT_TRUE(checker_->IsCheckPending());
}

TEST_F(PasswordSyncTokenLoginCheckerTest, SyncTokenInvalid) {
  CreatePasswordSyncTokenLoginChecker();
  checker_->CheckForPasswordNotInSync();
  OnTokenVerified(false);
  EXPECT_TRUE(
      user_manager_->FindUser(saml_login_account_id_)->force_online_signin());
  test_environment_.FastForwardBy(kSamlTokenDelay);
  EXPECT_FALSE(checker_->IsCheckPending());
}

TEST_F(PasswordSyncTokenLoginCheckerTest, ValidateSyncTokenHistogram) {
  base::HistogramTester histogram_tester;
  CreatePasswordSyncTokenLoginChecker();
  checker_->RecordTokenPollingStart();
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.SAML.InSessionPasswordSyncEvent", 1, 1);
}

}  // namespace ash
