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
#include "chrome/test/base/testing_browser_process.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr GaiaId::Literal kSAMLUserId("12345");
constexpr char kSAMLUserEmail[] = "alice@corp.example.com";

constexpr char kSyncToken[] = "sync-token-1";

constexpr base::TimeDelta kSamlTokenDelay = base::Seconds(60);

}  // namespace

class PasswordSyncTokenLoginCheckerTest : public testing::Test {
 protected:
  PasswordSyncTokenLoginCheckerTest();

  void SetUp() override;
  void TearDown() override;

  void OnTokenVerified(bool is_verified);

  const AccountId saml_login_account_id_ =
      AccountId::FromUserEmailGaiaId(kSAMLUserEmail, kSAMLUserId);

  content::BrowserTaskEnvironment test_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  network::TestURLLoaderFactory test_url_loader_factory_;

  std::unique_ptr<net::BackoffEntry> sync_token_retry_backoff_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<PasswordSyncTokenLoginChecker> checker_;
};

PasswordSyncTokenLoginCheckerTest::PasswordSyncTokenLoginCheckerTest() {
  fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

  sync_token_retry_backoff_ = std::make_unique<net::BackoffEntry>(
      &PasswordSyncTokenCheckersCollection::kFetchTokenRetryBackoffPolicy);
  fake_user_manager_->AddUser(saml_login_account_id_);
  fake_user_manager_->SwitchActiveUser(saml_login_account_id_);
}

void PasswordSyncTokenLoginCheckerTest::SetUp() {
  TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
      test_url_loader_factory_.GetSafeWeakWrapper());
  checker_ = std::make_unique<PasswordSyncTokenLoginChecker>(
      TestingBrowserProcess::GetGlobal()->shared_url_loader_factory(),
      saml_login_account_id_, kSyncToken, sync_token_retry_backoff_.get());
}

void PasswordSyncTokenLoginCheckerTest::TearDown() {
  checker_.reset();
  TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(nullptr);
}

void PasswordSyncTokenLoginCheckerTest::OnTokenVerified(bool is_verified) {
  checker_->OnTokenVerified(is_verified);
}

TEST_F(PasswordSyncTokenLoginCheckerTest, SyncTokenValid) {
  checker_->CheckForPasswordNotInSync();
  OnTokenVerified(true);
  EXPECT_FALSE(fake_user_manager_->FindUser(saml_login_account_id_)
                   ->force_online_signin());
  test_environment_.FastForwardBy(kSamlTokenDelay);
  EXPECT_TRUE(checker_->IsCheckPending());
}

TEST_F(PasswordSyncTokenLoginCheckerTest, SyncTokenInvalid) {
  checker_->CheckForPasswordNotInSync();
  OnTokenVerified(false);
  EXPECT_TRUE(fake_user_manager_->FindUser(saml_login_account_id_)
                  ->force_online_signin());
  test_environment_.FastForwardBy(kSamlTokenDelay);
  EXPECT_FALSE(checker_->IsCheckPending());
}

TEST_F(PasswordSyncTokenLoginCheckerTest, ValidateSyncTokenHistogram) {
  base::HistogramTester histogram_tester;
  checker_->RecordTokenPollingStart();
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.SAML.InSessionPasswordSyncEvent", 1, 1);
}

}  // namespace ash
