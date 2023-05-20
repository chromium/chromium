// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_controller.h"
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/brightness/unified_brightness_slider_controller.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/unified/notification_hidden_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_observer.h"

namespace ash {

namespace {

void SetSessionState(const session_manager::SessionState& state) {
  SessionInfo info;
  info.state = state;
  Shell::Get()->session_controller()->SetSessionInfo(info);
}

}  // anonymous namespace

class UnifiedSystemTrayControllerTest : public AshTestBase,
                                        public views::ViewObserver {
 public:
  UnifiedSystemTrayControllerTest() = default;

  UnifiedSystemTrayControllerTest(const UnifiedSystemTrayControllerTest&) =
      delete;
  UnifiedSystemTrayControllerTest& operator=(
      const UnifiedSystemTrayControllerTest&) = delete;

  ~UnifiedSystemTrayControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndDisableFeature(features::kQsRevamp);
    network_config_helper_ =
        std::make_unique<network_config::CrosNetworkConfigTestHelper>();
    AshTestBase::SetUp();
    // Networking stubs may have asynchronous initialization.
    base::RunLoop().RunUntilIdle();

    model_ = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);
    controller_ = std::make_unique<UnifiedSystemTrayController>(model());
  }

  void TearDown() override {
    DCHECK(view_) << "Must call InitializeView() during the tests";

    view_->RemoveObserver(this);

    view_.reset();
    controller_.reset();
    model_.reset();

    AshTestBase::TearDown();
  }

  void TurnUpVolume() {
    CrasAudioHandler::Get()->SetOutputVolumePercent(10);
    controller_->volume_slider_controller_->SliderValueChanged(
        nullptr, 0.9, 0.1, views::SliderChangeReason::kByUser);
  }

  void TurnDownVolume() {
    CrasAudioHandler::Get()->SetOutputVolumePercent(90);
    controller_->volume_slider_controller_->SliderValueChanged(
        nullptr, 0.1, 0.9, views::SliderChangeReason::kByUser);
  }

  void PressOnVolumeButton() {
    controller_->volume_slider_controller_->SliderButtonPressed();
  }

  void ChangeBrightness(float target_value) {
    controller_->brightness_slider_controller_->SliderValueChanged(
        nullptr, target_value, 0.0, views::SliderChangeReason::kByUser);
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override {
    view_->SetBoundsRect(gfx::Rect(view_->GetPreferredSize()));
    views::test::RunScheduledLayout(view_.get());
    ++preferred_size_changed_count_;
  }

 protected:
  void WaitForAnimation(UnifiedSystemTrayController* controller) {
    while (controller->animation_->is_animating())
      base::RunLoop().RunUntilIdle();
  }

  int preferred_size_changed_count() const {
    return preferred_size_changed_count_;
  }

  void InitializeView() {
    view_ = controller_->CreateUnifiedQuickSettingsView();

    view_->AddObserver(this);
    OnViewPreferredSizeChanged(view());

    preferred_size_changed_count_ = 0;
  }

  UnifiedSystemTrayModel* model() { return model_.get(); }
  UnifiedSystemTrayController* controller() { return controller_.get(); }
  UnifiedSystemTrayView* view() { return view_.get(); }

  bool PrimarySystemTrayIsExpandedOnOpen() {
    return GetPrimaryUnifiedSystemTray()->model()->IsExpandedOnOpen();
  }

 private:
  std::unique_ptr<network_config::CrosNetworkConfigTestHelper>
      network_config_helper_;
  scoped_refptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<UnifiedSystemTrayController> controller_;
  std::unique_ptr<UnifiedSystemTrayView> view_;

  int preferred_size_changed_count_ = 0;

  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

class QsRevampUnifiedSystemTrayControllerTest : public AshTestBase {
 public:
  QsRevampUnifiedSystemTrayControllerTest()
      : scoped_feature_list_(features::kQsRevamp) {}
  QsRevampUnifiedSystemTrayControllerTest(
      const QsRevampUnifiedSystemTrayControllerTest&) = delete;
  QsRevampUnifiedSystemTrayControllerTest& operator=(
      const QsRevampUnifiedSystemTrayControllerTest&) = delete;
  ~QsRevampUnifiedSystemTrayControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    network_config_helper_ =
        std::make_unique<network_config::CrosNetworkConfigTestHelper>();
    AshTestBase::SetUp();
    // Networking stubs may have asynchronous initialization.
    base::RunLoop().RunUntilIdle();

    model_ = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);
    controller_ = std::make_unique<UnifiedSystemTrayController>(model_.get());
  }

  void TearDown() override {
    DCHECK(quick_settings_view_);
    widget_.reset();
    quick_settings_view_ = nullptr;
    controller_.reset();
    model_.reset();

    AshTestBase::TearDown();
  }

  void InitializeQuickSettingsView() {
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    quick_settings_view_ =
        widget_->SetContentsView(controller_->CreateQuickSettingsView(600));
  }

  std::unique_ptr<network_config::CrosNetworkConfigTestHelper>
      network_config_helper_;
  scoped_refptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<UnifiedSystemTrayController> controller_;
  std::unique_ptr<views::Widget> widget_;

  // Owned by `widget_`.
  raw_ptr<QuickSettingsView, ExperimentalAsh> quick_settings_view_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that setting the `UnifiedSystemTrayModel::StateOnOpen` pref to
// collapsed is a no-op with the QSRevamp enabled.
TEST_F(QsRevampUnifiedSystemTrayControllerTest, ExpandedPrefIsNoOp) {
  // Set the pref to collapsed, there should be no effect.
  model_->set_expanded_on_open(UnifiedSystemTrayModel::StateOnOpen::COLLAPSED);

  InitializeQuickSettingsView();

  EXPECT_TRUE(model_->IsExpandedOnOpen());
  EXPECT_TRUE(controller_->IsExpanded());
}

TEST_F(UnifiedSystemTrayControllerTest, ToggleExpanded) {
  InitializeView();

  EXPECT_TRUE(model()->IsExpandedOnOpen());
  const int expanded_height = view()->GetPreferredSize().height();

  controller()->ToggleExpanded();
  WaitForAnimation(controller());

  const int collapsed_height = view()->GetPreferredSize().height();
  EXPECT_LT(collapsed_height, expanded_height);
  EXPECT_FALSE(model()->IsExpandedOnOpen());

  EXPECT_EQ(expanded_height, view()->GetExpandedSystemTrayHeight());
}

TEST_F(UnifiedSystemTrayControllerTest, UMATracking) {
  // No metrics logged before rendering on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.Visible",
                                     /*count=*/0);

  InitializeView();
  EXPECT_TRUE(model()->IsExpandedOnOpen());

  // Should show network pod.
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.Visible",
      QsFeatureCatalogName::kNetwork,
      /*expected_count=*/1);

  // Should not show cast pod since no casting service is setup.
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.Visible", QsFeatureCatalogName::kCast,
      /*expected_count=*/0);

  GetPrimaryUnifiedSystemTray()->CloseBubble();
  GetPrimaryUnifiedSystemTray()->ShowBubble();

  // Should show network pod again.
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.Visible",
      QsFeatureCatalogName::kNetwork,
      /*expected_count=*/2);
}

TEST_F(UnifiedSystemTrayControllerTest, EnsureExpanded_UserChooserShown) {
  InitializeView();
  EXPECT_FALSE(view()->detailed_view_container()->GetVisible());

  // Show the user chooser view.
  controller()->ShowUserChooserView();
  EXPECT_TRUE(view()->detailed_view_container()->GetVisible());

  // Calling EnsureExpanded() should hide the detailed view (e.g. this can
  // happen when changing the brightness or volume).
  controller()->EnsureExpanded();
  EXPECT_FALSE(view()->detailed_view_container()->GetVisible());
}

TEST_F(UnifiedSystemTrayControllerTest, PreferredSizeChanged) {
  InitializeView();

  // Checks PreferredSizeChanged is not called too frequently.
  EXPECT_EQ(0, preferred_size_changed_count());
  view()->SetExpandedAmount(0.0);
  EXPECT_EQ(1, preferred_size_changed_count());
  view()->SetExpandedAmount(0.25);
  EXPECT_EQ(2, preferred_size_changed_count());
  view()->SetExpandedAmount(0.75);
  EXPECT_EQ(3, preferred_size_changed_count());
  view()->SetExpandedAmount(1.0);
  EXPECT_EQ(4, preferred_size_changed_count());
}

TEST_F(UnifiedSystemTrayControllerTest, NotificationHiddenView_ModeShow) {
  AshMessageCenterLockScreenController::OverrideModeForTest(
      AshMessageCenterLockScreenController::Mode::SHOW);
  SetSessionState(session_manager::SessionState::LOCKED);
  InitializeView();

  EXPECT_TRUE(AshMessageCenterLockScreenController::IsAllowed());
  EXPECT_TRUE(AshMessageCenterLockScreenController::IsEnabled());
  EXPECT_FALSE(view()->notification_hidden_view_for_testing()->GetVisible());
}

TEST_F(UnifiedSystemTrayControllerTest, NotificationHiddenView_ModeHide) {
  AshMessageCenterLockScreenController::OverrideModeForTest(
      AshMessageCenterLockScreenController::Mode::HIDE);
  SetSessionState(session_manager::SessionState::LOCKED);
  InitializeView();

  EXPECT_TRUE(AshMessageCenterLockScreenController::IsAllowed());
  EXPECT_FALSE(AshMessageCenterLockScreenController::IsEnabled());
  EXPECT_TRUE(view()->notification_hidden_view_for_testing()->GetVisible());
  EXPECT_NE(nullptr, view()
                         ->notification_hidden_view_for_testing()
                         ->change_button_for_testing());
}

TEST_F(UnifiedSystemTrayControllerTest,
       NotificationHiddenView_ModeHideSensitive) {
  AshMessageCenterLockScreenController::OverrideModeForTest(
      AshMessageCenterLockScreenController::Mode::HIDE_SENSITIVE);
  SetSessionState(session_manager::SessionState::LOCKED);
  InitializeView();

  EXPECT_TRUE(AshMessageCenterLockScreenController::IsAllowed());
  EXPECT_TRUE(AshMessageCenterLockScreenController::IsEnabled());
  EXPECT_FALSE(view()->notification_hidden_view_for_testing()->GetVisible());
}

TEST_F(UnifiedSystemTrayControllerTest, NotificationHiddenView_ModeProhibited) {
  AshMessageCenterLockScreenController::OverrideModeForTest(
      AshMessageCenterLockScreenController::Mode::PROHIBITED);
  SetSessionState(session_manager::SessionState::LOCKED);
  InitializeView();

  EXPECT_FALSE(AshMessageCenterLockScreenController::IsAllowed());
  EXPECT_FALSE(AshMessageCenterLockScreenController::IsEnabled());
  EXPECT_TRUE(view()->notification_hidden_view_for_testing()->GetVisible());
  EXPECT_EQ(nullptr, view()
                         ->notification_hidden_view_for_testing()
                         ->change_button_for_testing());
}

TEST_F(UnifiedSystemTrayControllerTest, SystemTrayCollapsePref) {
  InitializeView();
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  UnifiedSystemTrayController* controller =
      GetPrimaryUnifiedSystemTray()->bubble()->unified_system_tray_controller();
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();

  // System tray is initially expanded when no pref is set.
  EXPECT_FALSE(prefs->HasPrefPath(prefs::kSystemTrayExpanded));
  EXPECT_TRUE(PrimarySystemTrayIsExpandedOnOpen());

  // Toggle collapsed state.
  controller->ToggleExpanded();
  WaitForAnimation(controller);
  EXPECT_FALSE(PrimarySystemTrayIsExpandedOnOpen());

  // Close bubble and assert pref has been set.
  GetPrimaryUnifiedSystemTray()->CloseBubble();
  EXPECT_TRUE(prefs->HasPrefPath(prefs::kSystemTrayExpanded));

  // Reopen bubble to load `kSystemTrayExpanded` pref.
  GetPrimaryUnifiedSystemTray()->ShowBubble();
  EXPECT_FALSE(PrimarySystemTrayIsExpandedOnOpen());
}

TEST_F(UnifiedSystemTrayControllerTest, SliderUMA) {
  InitializeView();
  GetPrimaryUnifiedSystemTray()->ShowBubble();

  // No metrics logged before making any action.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.Slider.Up",
                                     /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.Slider.Down",
                                     /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.Slider.EnableFeature",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.Slider.DisableFeature",
      /*count=*/0);

  TurnUpVolume();

  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.Slider.Down",
                                     /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.Slider.Up",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount("Ash.UnifiedSystemView.Slider.Up",
                                      QsSliderCatalogName::kVolume,
                                      /*expected_count=*/1);

  TurnDownVolume();

  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.Slider.Down",
                                     /*count=*/1);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.Slider.Up",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount("Ash.UnifiedSystemView.Slider.Down",
                                      QsSliderCatalogName::kVolume,
                                      /*expected_count=*/1);

  // Set the current state to unmute.
  CrasAudioHandler::Get()->SetOutputMute(false);

  // Mute.
  PressOnVolumeButton();

  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.Slider.EnableFeature",
      /*count=*/1);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.Slider.DisableFeature",
      /*count=*/0);
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.Slider.EnableFeature",
      QsSliderCatalogName::kVolume,
      /*expected_count=*/1);

  // Press again to unmute.
  PressOnVolumeButton();

  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.Slider.EnableFeature",
      /*count=*/1);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.Slider.DisableFeature",
      /*count=*/1);
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.Slider.DisableFeature",
      QsSliderCatalogName::kVolume,
      /*expected_count=*/1);

  // Turn down the brightness. The init value should be 1.0 (100%).
  ChangeBrightness(/*target_value=*/0.5);

  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.Slider.Down",
                                     /*count=*/2);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.Slider.Up",
                                     /*count=*/1);
  histogram_tester->ExpectBucketCount("Ash.UnifiedSystemView.Slider.Down",
                                      QsSliderCatalogName::kBrightness,
                                      /*expected_count=*/1);

  // Turn up the brightness.
  ChangeBrightness(/*target_value=*/0.9);

  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.Slider.Down",
                                     /*count=*/2);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.Slider.Up",
                                     /*count=*/2);
  histogram_tester->ExpectBucketCount("Ash.UnifiedSystemView.Slider.Up",
                                      QsSliderCatalogName::kBrightness,
                                      /*expected_count=*/1);
}

}  // namespace ash
