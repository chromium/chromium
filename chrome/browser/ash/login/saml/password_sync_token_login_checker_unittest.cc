// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/password_sync_token_login_checker.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/saml/password_sync_token_checkers_collection.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/backoff_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

const char kSAMLUserId[] = "12345";
const char kSAMLUserEmail[] = "alice@corp.example.com";

const char kSyncToken[] = "sync-token-1";

constexpr base::TimeDelta kSamlTokenDelay = base::Seconds(60);

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

  ScopedTestingLocalState scoped_local_state_;
  std::unique_ptr<net::BackoffEntry> sync_token_retry_backoff_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<PasswordSyncTokenLoginChecker> checker_;
};

PasswordSyncTokenLoginCheckerTest::PasswordSyncTokenLoginCheckerTest()
    : scoped_local_state_(TestingBrowserProcess::GetGlobal()) {
  fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

  sync_token_retry_backoff_ = std::make_unique<net::BackoffEntry>(
      &PasswordSyncTokenCheckersCollection::kFetchTokenRetryBackoffPolicy);
  fake_user_manager_->AddUser(saml_login_account_id_);
  fake_user_manager_->SwitchActiveUser(saml_login_account_id_);
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
  EXPECT_FALSE(fake_user_manager_->FindUser(saml_login_account_id_)
                   ->force_online_signin());
  test_environment_.FastForwardBy(kSamlTokenDelay);
  EXPECT_TRUE(checker_->IsCheckPending());
}

TEST_F(PasswordSyncTokenLoginCheckerTest, SyncTokenInvalid) {
  CreatePasswordSyncTokenLoginChecker();
  checker_->CheckForPasswordNotInSync();
  OnTokenVerified(false);
  EXPECT_TRUE(fake_user_manager_->FindUser(saml_login_account_id_)
                  ->force_online_signin());
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
