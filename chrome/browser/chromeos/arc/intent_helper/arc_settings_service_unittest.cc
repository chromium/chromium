// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/intent_helper/arc_settings_service.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/settings/stats_reporting_controller.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/arc/test/fake_backup_settings_instance.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"

namespace arc {

namespace {

constexpr char kActionLocaionEnabled[] =
    "org.chromium.arc.intent_helper.SET_LOCATION_SERVICE_ENABLED";

class ArcSettingsServiceTest : public BrowserWithTestWindowTest {
 public:
  ArcSettingsServiceTest()
      : user_manager_enabler_(
            std::make_unique<chromeos::FakeChromeUserManager>()) {}
  ~ArcSettingsServiceTest() override = default;

  // BrowserWithTestWindowTest:
  void SetUp() override {
    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    ArcSessionManager::SetUiEnabledForTesting(false);
    chromeos::DBusThreadManager::Initialize();
    chromeos::NetworkHandler::Initialize();
    chromeos::StatsReportingController::RegisterLocalStatePrefs(
        local_state_.registry());
    chromeos::StatsReportingController::Initialize(&local_state_);

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ =
        std::make_unique<ArcSessionManager>(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));

    BrowserWithTestWindowTest::SetUp();
    arc_service_manager_->set_browser_context(profile());

    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "1234567890"));
    user_manager()->AddUser(account_id);
    user_manager()->LoginUser(account_id);

    arc_session_manager()->SetProfile(profile());
    arc_session_manager()->Initialize();

    arc_intent_helper_bridge_ = std::make_unique<ArcIntentHelperBridge>(
        profile(), arc_bridge_service());
    ArcSettingsService* arc_settings_service =
        ArcSettingsService::GetForBrowserContext(profile());
    DCHECK(arc_settings_service);

    // These prefs are set in negotiator.
    profile()->GetPrefs()->SetBoolean(prefs::kArcLocationServiceEnabled, true);
    profile()->GetPrefs()->SetBoolean(prefs::kArcBackupRestoreEnabled, true);
  }

  void TearDown() override {
    arc_bridge_service()->intent_helper()->CloseInstance(
        &intent_helper_instance_);
    arc_bridge_service()->backup_settings()->CloseInstance(
        &backup_settings_instance_);
    arc_intent_helper_bridge_.reset();
    arc_session_manager()->Shutdown();

    arc_service_manager_->set_browser_context(nullptr);
    BrowserWithTestWindowTest::TearDown();

    arc_session_manager_.reset();
    arc_service_manager_.reset();

    chromeos::StatsReportingController::Shutdown();
    chromeos::NetworkHandler::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
  }

  void SetInstances() {
    arc_bridge_service()->backup_settings()->SetInstance(
        &backup_settings_instance_);
    WaitForInstanceReady(arc_bridge_service()->backup_settings());

    arc_bridge_service()->intent_helper()->SetInstance(
        &intent_helper_instance_);
    WaitForInstanceReady(arc_bridge_service()->intent_helper());
  }

  chromeos::FakeChromeUserManager* user_manager() {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }
  ArcBridgeService* arc_bridge_service() {
    return arc_service_manager_->arc_bridge_service();
  }
  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }
  FakeIntentHelperInstance* intent_helper_instance() {
    return &intent_helper_instance_;
  }
  FakeBackupSettingsInstance* backup_settings_instance() {
    return &backup_settings_instance_;
  }

 private:
  TestingPrefServiceSimple local_state_;
  user_manager::ScopedUserManager user_manager_enabler_;
  std::unique_ptr<ArcIntentHelperBridge> arc_intent_helper_bridge_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  FakeIntentHelperInstance intent_helper_instance_;
  FakeBackupSettingsInstance backup_settings_instance_;

  DISALLOW_COPY_AND_ASSIGN(ArcSettingsServiceTest);
};

}  // namespace

// Initial settings applied in case intent helper instance is set after
// provisioning.
TEST_F(ArcSettingsServiceTest,
       InitialSettingsAppliedForInstanceAfterProvisioning) {
  arc_session_manager()->RequestEnable();
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  arc_session_manager()->StartArcForTesting();

  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcInitialSettingsPending));

  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);

  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcInitialSettingsPending));
  EXPECT_EQ(0, backup_settings_instance()->set_backup_enabled_count());
  EXPECT_TRUE(intent_helper_instance()
                  ->GetBroadcastsForAction(kActionLocaionEnabled)
                  .empty());

  SetInstances();
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcInitialSettingsPending));

  EXPECT_EQ(1, backup_settings_instance()->set_backup_enabled_count());
  EXPECT_EQ(1U, intent_helper_instance()
                    ->GetBroadcastsForAction(kActionLocaionEnabled)
                    .size());
}

// Initial settings applied in case intent helper instance is set before
// provisioning.

// TODO(crbug.com/1004630) Disabled due to flake.
TEST_F(ArcSettingsServiceTest,
       DISABLED_InitialSettingsAppliedForInstanceBeforeProvisioning) {
  arc_session_manager()->RequestEnable();
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  arc_session_manager()->StartArcForTesting();

  SetInstances();
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcInitialSettingsPending));

  EXPECT_EQ(0, backup_settings_instance()->set_backup_enabled_count());
  EXPECT_TRUE(intent_helper_instance()
                  ->GetBroadcastsForAction(kActionLocaionEnabled)
                  .empty());

  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);

  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcInitialSettingsPending));
  EXPECT_EQ(1, backup_settings_instance()->set_backup_enabled_count());
  EXPECT_EQ(1U, intent_helper_instance()
                    ->GetBroadcastsForAction(kActionLocaionEnabled)
                    .size());
}

// Initial settings are applied in case intent helper instance was not set in
// the first session when OptIn happened but for set in the next session.
TEST_F(ArcSettingsServiceTest, InitialSettingsPendingAppliedNextSession) {
  profile()->GetPrefs()->SetBoolean(prefs::kArcInitialSettingsPending, true);
  profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
  profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager()->RequestEnable();
  SetInstances();

  EXPECT_EQ(1, backup_settings_instance()->set_backup_enabled_count());
  EXPECT_EQ(1U, intent_helper_instance()
                    ->GetBroadcastsForAction(kActionLocaionEnabled)
                    .size());
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcInitialSettingsPending));
}

// Initial settings are not applied in case intent helper instance is set in the
// next sessions and we don't have pending request..
TEST_F(ArcSettingsServiceTest, InitialSettingsNotAppliedNextSession) {
  profile()->GetPrefs()->SetBoolean(prefs::kArcInitialSettingsPending, false);
  profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
  profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager()->RequestEnable();
  SetInstances();

  EXPECT_EQ(0, backup_settings_instance()->set_backup_enabled_count());
  EXPECT_EQ(0U, intent_helper_instance()
                    ->GetBroadcastsForAction(kActionLocaionEnabled)
                    .size());
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcInitialSettingsPending));
}

TEST_F(ArcSettingsServiceTest, SplitSettingsDisablesFontSize) {
  constexpr char kSetFontScale[] =
      "org.chromium.arc.intent_helper.SET_FONT_SCALE";

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kSplitSettings);

  // Initial broadcast resets to 100%.
  arc_session_manager()->RequestEnable();
  SetInstances();
  FakeIntentHelperInstance* intent_helper = intent_helper_instance();
  auto broadcasts = intent_helper->GetBroadcastsForAction(kSetFontScale);
  ASSERT_EQ(1U, broadcasts.size());
  EXPECT_EQ("{\"scale\":1.0}", broadcasts[0].extras);

  // No broadcast after update.
  intent_helper->clear_broadcasts();
  profile()->GetPrefs()->SetInteger(::prefs::kWebKitDefaultFontSize, 20);
  EXPECT_EQ(0U, intent_helper->GetBroadcastsForAction(kSetFontScale).size());
}

TEST_F(ArcSettingsServiceTest, SplitSettingsDisablesPageZoom) {
  constexpr char kSetPageZoom[] =
      "org.chromium.arc.intent_helper.SET_PAGE_ZOOM";

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kSplitSettings);

  // Initial broadcast resets to 100%.
  arc_session_manager()->RequestEnable();
  SetInstances();
  FakeIntentHelperInstance* intent_helper = intent_helper_instance();
  auto broadcasts = intent_helper->GetBroadcastsForAction(kSetPageZoom);
  ASSERT_EQ(1U, broadcasts.size());
  EXPECT_EQ("{\"zoomFactor\":1.0}", broadcasts[0].extras);

  // No broadcast after update.
  intent_helper->clear_broadcasts();
  profile()->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(150.0);
  EXPECT_EQ(0U, intent_helper->GetBroadcastsForAction(kSetPageZoom).size());
}

}  // namespace arc
