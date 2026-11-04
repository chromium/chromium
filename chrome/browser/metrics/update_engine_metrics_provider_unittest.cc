// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/update_engine_metrics_provider.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/signin/fake_identity_manager_provider.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/user_manager/user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

class UpdateEngineMetricsProviderTest : public testing::Test {
 public:
  UpdateEngineMetricsProviderTest() = default;
  ~UpdateEngineMetricsProviderTest() override = default;

  void SetUp() override {
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(
            TestingBrowserProcess::GetGlobal()->local_state());
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    identity_manager_provider_ =
        std::make_unique<ash::FakeIdentityManagerProvider>();
    provider_ = std::make_unique<UpdateEngineMetricsProvider>(
        session_manager::SessionManager::Get());
  }

  void TearDown() override {
    provider_.reset();
    identity_manager_provider_.reset();
    identity_test_env_.reset();
    user_session_test_environment_.reset();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ash::ScopedStubInstallAttributes install_attributes_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  std::unique_ptr<ash::FakeIdentityManagerProvider> identity_manager_provider_;
  std::unique_ptr<UpdateEngineMetricsProvider> provider_;
};

TEST_F(UpdateEngineMetricsProviderTest,
       RecordsMetricOnlyWhileIdentityManagerIsAvailable) {
  AccountInfo account_info = identity_test_env_->MakePrimaryAccountAvailable(
      "owner@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_toggle_auto_updates(true);
  signin::UpdateAccountInfoForAccount(identity_test_env_->identity_manager(),
                                      account_info);

  const AccountId account_id = AccountId::FromUserEmailGaiaId(
      account_info.GetEmail(), account_info.GetGaiaId());
  ASSERT_TRUE(user_session_test_environment_->AddRegularUser(account_id));
  user_manager::UserManager::Get()->SetOwnerId(account_id);
  identity_manager_provider_->SetIdentityManagerForAccount(
      account_id, identity_test_env_->identity_manager());

  user_session_test_environment_->LogIn(account_id);

  base::HistogramTester histogram_tester;
  // Before the user's profile is loaded, the metric should not be recorded.
  provider_->ProvideCurrentSessionData(nullptr);
  histogram_tester.ExpectTotalCount("UpdateEngine.ConsumerAutoUpdate", 0);

  // Once the user's profile is loaded, the metric should be recorded.
  session_manager::SessionManager::Get()->NotifyUserProfileLoaded(account_id);
  provider_->ProvideCurrentSessionData(nullptr);
  histogram_tester.ExpectUniqueSample("UpdateEngine.ConsumerAutoUpdate", false,
                                      1);

  // After IdentityManager shuts down, the metric should not be recorded.
  identity_manager_provider_->SetIdentityManagerForAccount(account_id, nullptr);
  identity_test_env_.reset();
  provider_->ProvideCurrentSessionData(nullptr);
  histogram_tester.ExpectUniqueSample("UpdateEngine.ConsumerAutoUpdate", false,
                                      1);
}
