// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_service.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/lobster/mock/mock_snapper_provider.h"
#include "chrome/browser/ash/lobster/mock_lobster_system_state_provider.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_consent_status.h"
#include "components/account_id/account_id.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/startup_visibility.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/variations/service/test_variations_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class LobsterServiceTest : public ChromeAshTestBase {
 public:
  LobsterServiceTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~LobsterServiceTest() override = default;

  void SetUp() override {
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        TestingBrowserProcess::GetGlobal()->local_state(),
        &metrics_enabled_state_provider_,
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath(),
        metrics::StartupVisibility::kUnknown);
    variations_service_ = std::make_unique<variations::TestVariationsService>(
        TestingBrowserProcess::GetGlobal()->local_state(),
        metrics_state_manager_.get());
    TestingBrowserProcess::GetGlobal()->SetVariationsService(
        variations_service_.get());

    ChromeAshTestBase::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());

    auto* user = fake_user_manager_->AddUser(user_manager::StubAccountId());
    fake_user_manager_->LoginUser(user->GetAccountId());
    testing_profile_ = profile_manager_.CreateTestingProfile("profile");
    ash::AnnotatedAccountId::Set(testing_profile_.get(), user->GetAccountId());
    lobster_service_ = std::make_unique<LobsterService>(
        TestingBrowserProcess::GetGlobal()->variations_service(),
        testing_profile_.get(),
        IdentityManagerFactory::GetForProfile(testing_profile_.get()),
        std::make_unique<MockSnapperProvider>());
  }

  void TearDown() override {
    ChromeAshTestBase::TearDown();
    lobster_service_.reset();
    testing_profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
    fake_user_manager_.Reset();
    TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);
    variations_service_.reset();
    metrics_state_manager_.reset();
  }

  TestingProfile* profile() { return testing_profile_; }

  LobsterService* lobster_service() { return lobster_service_.get(); }

  void ToggleLobsterSettings(bool value) {
    profile()->GetPrefs()->SetBoolean(ash::prefs::kLobsterEnabled, value);
  }

  chromeos::editor_menu::EditorConsentStatus GetLobsterConsentStatus() {
    return chromeos::editor_menu::GetConsentStatusFromInteger(
        profile()->GetPrefs()->GetInteger(ash::prefs::kOrcaConsentStatus));
  }

 private:
  metrics::TestEnabledStateProvider metrics_enabled_state_provider_{
      /*consent=*/false,
      /*enabled=*/false};
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> variations_service_;
  raw_ptr<TestingProfile> testing_profile_ = nullptr;
  TestingProfileManager profile_manager_;
  std::unique_ptr<LobsterService> lobster_service_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
};

TEST_F(LobsterServiceTest,
       SwitchingOnSettingToggleWillResetConsentWhichWasPreviouslyDeclined) {
  ToggleLobsterSettings(false);
  profile()->GetPrefs()->SetInteger(
      ash::prefs::kOrcaConsentStatus,
      std::to_underlying(
          chromeos::editor_menu::EditorConsentStatus::kDeclined));

  // Turn on the Lobster Settings
  ToggleLobsterSettings(true);

  EXPECT_EQ(GetLobsterConsentStatus(),
            chromeos::editor_menu::EditorConsentStatus::kUnset);
}

TEST_F(LobsterServiceTest,
       SwitchingOnSettingToggleWillNotResetConsentWhichWasPreviouslyApproved) {
  ToggleLobsterSettings(true);
  profile()->GetPrefs()->SetInteger(
      ash::prefs::kOrcaConsentStatus,
      std::to_underlying(
          chromeos::editor_menu::EditorConsentStatus::kApproved));

  // Turn off the Lobster Settings
  ToggleLobsterSettings(false);
  EXPECT_EQ(GetLobsterConsentStatus(),
            chromeos::editor_menu::EditorConsentStatus::kApproved);

  // Turn on the Lobster Settings
  ToggleLobsterSettings(true);
  EXPECT_EQ(GetLobsterConsentStatus(),
            chromeos::editor_menu::EditorConsentStatus::kApproved);
}

TEST_F(LobsterServiceTest, CanNotShowLobsterFeatureSettingsToggle) {
  std::unique_ptr<MockLobsterSystemStateProvider> mock_system_state_provider =
      std::make_unique<MockLobsterSystemStateProvider>();

  ON_CALL(*mock_system_state_provider, GetSystemState)
      .WillByDefault(testing::Return(ash::LobsterSystemState(
          ash::LobsterStatus::kBlocked, /*failed_checks=*/{
              ash::LobsterSystemCheck::kInvalidAccountCapabilities,
              ash::LobsterSystemCheck::kInvalidRegion})));

  lobster_service()->set_lobster_system_state_provider_for_testing(
      std::move(mock_system_state_provider));

  EXPECT_FALSE(lobster_service()->CanShowFeatureSettingsToggle());
}

TEST_F(LobsterServiceTest, CanShowLobsterFeatureSettingsToggle) {
  std::unique_ptr<MockLobsterSystemStateProvider> mock_system_state_provider =
      std::make_unique<MockLobsterSystemStateProvider>();

  ON_CALL(*mock_system_state_provider, GetSystemState)
      .WillByDefault(testing::Return(ash::LobsterSystemState(
          ash::LobsterStatus::kBlocked,
          /*failed_checks=*/{ash::LobsterSystemCheck::kInvalidInputField,
                             ash::LobsterSystemCheck::kInvalidInputMethod,
                             ash::LobsterSystemCheck::kNoInternetConnection})));

  lobster_service()->set_lobster_system_state_provider_for_testing(
      std::move(mock_system_state_provider));

  EXPECT_TRUE(lobster_service()->CanShowFeatureSettingsToggle());
}

}  // namespace
