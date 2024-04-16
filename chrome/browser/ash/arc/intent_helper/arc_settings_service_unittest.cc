// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/intent_helper/arc_settings_service.h"

#include <memory>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/metrics/arc_metrics_service.h"
#include "ash/components/arc/metrics/stability_metrics_manager.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_host.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/components/arc/test/fake_backup_settings_instance.h"
#include "ash/constants/ash_pref_names.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_provisioning_result.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "components/arc/test/fake_intent_helper_host.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/language/core/browser/pref_names.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace arc {

namespace {

constexpr char kActionLocaionEnabled[] =
    "org.chromium.arc.intent_helper.SET_LOCATION_SERVICE_ENABLED";

bool IsSameCaptionColor(const arc::mojom::CaptionColor* l,
                        const arc::mojom::CaptionColor* r) {
  return l->red == r->red && l->blue == r->blue && l->green == r->green &&
         l->alpha == r->alpha;
}
MATCHER_P(VerifyCaptionColor, color, "") {
  return IsSameCaptionColor(arg.get(), color);
}

class ArcSettingsServiceTest : public BrowserWithTestWindowTest {
 public:
  ArcSettingsServiceTest() = default;
  ArcSettingsServiceTest(const ArcSettingsServiceTest&) = delete;
  ArcSettingsServiceTest& operator=(const ArcSettingsServiceTest&) = delete;
  ~ArcSettingsServiceTest() override = default;

  // BrowserWithTestWindowTest:
  void SetUp() override {
    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    ArcSessionManager::SetUiEnabledForTesting(false);
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    network_handler_test_helper_ =
        std::make_unique<ash::NetworkHandlerTestHelper>();
    network_config_helper_ =
        std::make_unique<ash::network_config::CrosNetworkConfigTestHelper>();
    ash::StatsReportingController::RegisterLocalStatePrefs(
        local_state_.registry());
    ash::StatsReportingController::Initialize(&local_state_);

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));

    BrowserWithTestWindowTest::SetUp();
    arc_service_manager_->set_browser_context(profile());

    arc::prefs::RegisterLocalStatePrefs(local_state_.registry());
    arc::StabilityMetricsManager::Initialize(&local_state_);

    ArcMetricsService::GetForBrowserContextForTesting(profile())
        ->SetHistogramNamerCallback(
            base::BindRepeating([](const std::string& base_name) {
              return arc::GetHistogramNameByUserTypeForPrimaryProfile(
                  base_name);
            }));

    arc_session_manager()->SetProfile(profile());
    arc_session_manager()->Initialize();

    intent_helper_host_ = std::make_unique<FakeIntentHelperHost>(
        arc_bridge_service()->intent_helper());
    app_host_ = std::make_unique<FakeAppHost>(arc_bridge_service()->app());
    app_instance_ = std::make_unique<FakeAppInstance>(app_host_.get());
    ArcSettingsService* arc_settings_service =
        ArcSettingsService::GetForBrowserContext(profile());
    DCHECK(arc_settings_service);

    // These prefs are set in negotiator.
    profile()->GetPrefs()->SetBoolean(prefs::kArcLocationServiceEnabled, true);
    profile()->GetPrefs()->SetBoolean(prefs::kArcBackupRestoreEnabled, true);
  }

  void TearDown() override {
    arc::StabilityMetricsManager::Shutdown();
    arc_bridge_service()->intent_helper()->CloseInstance(
        &intent_helper_instance_);
    arc_bridge_service()->backup_settings()->CloseInstance(
        &backup_settings_instance_);
    arc_bridge_service()->app()->CloseInstance(app_instance_.get());
    app_instance_.reset();
    app_host_.reset();
    intent_helper_host_.reset();
    arc_session_manager()->Shutdown();

    arc_service_manager_->set_browser_context(nullptr);
    network_config_helper_.reset();
    BrowserWithTestWindowTest::TearDown();

    arc_session_manager_.reset();
    arc_service_manager_.reset();

    ash::StatsReportingController::Shutdown();
    network_handler_test_helper_.reset();
    ash::ConciergeClient::Shutdown();
  }

  void SetInstances() {
    arc_bridge_service()->backup_settings()->SetInstance(
        &backup_settings_instance_);
    WaitForInstanceReady(arc_bridge_service()->backup_settings());

    arc_bridge_service()->intent_helper()->SetInstance(
        &intent_helper_instance_);
    WaitForInstanceReady(arc_bridge_service()->intent_helper());

    arc_bridge_service()->app()->SetInstance(app_instance_.get());
    WaitForInstanceReady(arc_bridge_service()->app());
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
  std::unique_ptr<ash::NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<ash::network_config::CrosNetworkConfigTestHelper>
      network_config_helper_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<FakeIntentHelperHost> intent_helper_host_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  FakeIntentHelperInstance intent_helper_instance_;
  FakeBackupSettingsInstance backup_settings_instance_;
  std::unique_ptr<FakeAppHost> app_host_;
  std::unique_ptr<FakeAppInstance> app_instance_;
};

}  // namespace

// Initial settings applied in case intent helper instance is set after
// provisioning.
TEST_F(ArcSettingsServiceTest,
       InitialSettingsAppliedForInstanceAfterProvisioning) {
  arc_session_manager()->RequestEnable();
  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();

  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcInitialSettingsPending));

  arc::mojom::ArcSignInResultPtr result =
      arc::mojom::ArcSignInResult::NewSuccess(
          arc::mojom::ArcSignInSuccess::SUCCESS);
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));

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

// TODO(crbug.com/40099107) Disabled due to flake.
TEST_F(ArcSettingsServiceTest,
       DISABLED_InitialSettingsAppliedForInstanceBeforeProvisioning) {
  arc_session_manager()->RequestEnable();
  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();

  SetInstances();
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcInitialSettingsPending));

  EXPECT_EQ(0, backup_settings_instance()->set_backup_enabled_count());
  EXPECT_TRUE(intent_helper_instance()
                  ->GetBroadcastsForAction(kActionLocaionEnabled)
                  .empty());

  arc::mojom::ArcSignInResultPtr result =
      arc::mojom::ArcSignInResult::NewSuccess(
          arc::mojom::ArcSignInSuccess::SUCCESS);
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));

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

TEST_F(ArcSettingsServiceTest, DisablesFontSize) {
  constexpr char kSetFontScale[] =
      "org.chromium.arc.intent_helper.SET_FONT_SCALE";

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

TEST_F(ArcSettingsServiceTest, DisablesPageZoom) {
  constexpr char kSetPageZoom[] =
      "org.chromium.arc.intent_helper.SET_PAGE_ZOOM";

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

TEST_F(ArcSettingsServiceTest, SetCaptionStyle) {
  arc_session_manager()->RequestEnable();
  SetInstances();
  FakeIntentHelperInstance* intent_helper = intent_helper_instance();

  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetString(::prefs::kAccessibilityCaptionsTextSize, "200%");
  pref_service->SetString(::prefs::kAccessibilityCaptionsTextColor, "10,20,30");
  pref_service->SetInteger(::prefs::kAccessibilityCaptionsTextOpacity, 90);
  pref_service->SetString(::prefs::kAccessibilityCaptionsBackgroundColor,
                          "40,50,60");
  pref_service->SetInteger(::prefs::kAccessibilityCaptionsBackgroundOpacity,
                           80);
  pref_service->SetString(::prefs::kAccessibilityCaptionsTextShadow,
                          "-2px -2px 4px rgba(0, 0, 0, 0.5)");
  pref_service->SetString(::language::prefs::kApplicationLocale, "my_locale");

  auto style = intent_helper->GetCaptionStyle();

  ASSERT_TRUE(style);
  EXPECT_EQ(2.0f, style->font_scale);
  // Alpha value from 0.9 * 255.
  EXPECT_THAT(arc::mojom::CaptionColor::New(230, 10, 20, 30),
              VerifyCaptionColor(style->text_color.get()));
  // Alpha value from 0.8 * 255.
  EXPECT_THAT(arc::mojom::CaptionColor::New(204, 40, 50, 60),
              VerifyCaptionColor(style->background_color.get()));
  EXPECT_EQ("my_locale", style->user_locale);
  EXPECT_EQ(arc::mojom::CaptionTextShadowType::kRaised,
            style->text_shadow_type);
}

TEST_F(ArcSettingsServiceTest, CaptionStyleNotSetReturnEmpty) {
  arc_session_manager()->RequestEnable();
  SetInstances();
  FakeIntentHelperInstance* intent_helper = intent_helper_instance();

  auto style = intent_helper->GetCaptionStyle();

  ASSERT_TRUE(style);
  EXPECT_EQ(1.0f, style->font_scale);
  EXPECT_EQ(nullptr, style->text_color.get());
  EXPECT_EQ(nullptr, style->background_color.get());
  EXPECT_EQ("", style->user_locale);
  EXPECT_EQ(arc::mojom::CaptionTextShadowType::kNone, style->text_shadow_type);
}

TEST_F(ArcSettingsServiceTest, EnableAccessibilityFeatures) {
  arc_session_manager()->RequestEnable();
  SetInstances();
  FakeIntentHelperInstance* intent_helper = intent_helper_instance();

  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityFocusHighlightEnabled,
                           true);
  pref_service->SetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled,
                           true);
  pref_service->SetBoolean(ash::prefs::kAccessibilitySelectToSpeakEnabled,
                           true);
  pref_service->SetBoolean(ash::prefs::kAccessibilitySpokenFeedbackEnabled,
                           false);
  pref_service->SetBoolean(ash::prefs::kAccessibilitySwitchAccessEnabled,
                           false);
  pref_service->SetBoolean(ash::prefs::kDockedMagnifierEnabled, false);

  auto features = intent_helper->GetAccessibilityFeatures();

  ASSERT_TRUE(features->focus_highlight_enabled);
  ASSERT_TRUE(features->screen_magnifier_enabled);
  ASSERT_TRUE(features->select_to_speak_enabled);
  ASSERT_FALSE(features->spoken_feedback_enabled);
  ASSERT_FALSE(features->switch_access_enabled);
  ASSERT_FALSE(features->docked_magnifier_enabled);
}

}  // namespace arc
