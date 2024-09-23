// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/vc_tile_ui_controller.h"

#include <memory>
#include <optional>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/video_conference/effects/fake_video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_utils.h"
#include "ash/test/ash_test_base.h"
#include "ash/utility/haptics_tracking_test_input_controller.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash::video_conference {

// A class that waits for a given `FeatureTile`'s download state to change.
class FeatureTileDownloadStateWaiter : public FeatureTile::Observer {
 public:
  FeatureTileDownloadStateWaiter() = default;
  FeatureTileDownloadStateWaiter(const FeatureTileDownloadStateWaiter&) =
      delete;
  FeatureTileDownloadStateWaiter& operator=(
      const FeatureTileDownloadStateWaiter&) = delete;
  ~FeatureTileDownloadStateWaiter() override = default;

  // Waits for `tile`'s download state to change before proceeding. Note: this
  // method should only be called when a change to `tile`'s download state is
  // actually expected to happen. If no change happens, then this waits forever
  // (until the test times out).
  void Wait(FeatureTile* tile, FeatureTile::DownloadState expected_state) {
    base::ScopedObservation<FeatureTile, FeatureTile::Observer>
        scoped_observation{this};
    scoped_observation.Observe(tile);
    future_.Clear();
    EXPECT_EQ(expected_state, future_.Take());
  }

 private:
  // Featuretile::Observer:
  void OnDownloadStateChanged(FeatureTile::DownloadState download_state,
                              int progress) override {
    future_.SetValue(download_state);
  }

  base::test::TestFuture<FeatureTile::DownloadState> future_;
};

class VcTileUiControllerTest : public AshTestBase {
 public:
  VcTileUiControllerTest() = default;
  VcTileUiControllerTest(const VcTileUiControllerTest&) = delete;
  VcTileUiControllerTest& operator=(const VcTileUiControllerTest&) = delete;
  ~VcTileUiControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_
        .InitWithFeatures(/*enabled_features=*/
                          {features::kFeatureManagementVideoConference,
                           features::kVcDlcUi},
                          /*disabled_features=*/{});
    DlcserviceClient::InitializeFake();

    // Tests need to use a fake controller for the video conference tray, which
    // is present when `features::IsVideoConferenceEnabled()` is true. A
    // `FakeVideoConferenceTrayEffectsManager` is also used so that the set of
    // DLCs associated with a VC effect can be configured in the tests.
    fake_vc_effect_manager_ =
        std::make_unique<FakeVideoConferenceTrayEffectsManager>();
    fake_vc_tray_controller_ =
        std::make_unique<FakeVideoConferenceTrayController>();
    fake_vc_tray_controller_->SetEffectsManager(fake_vc_effect_manager_.get());

    AshTestBase::SetUp();

    // Create a test VC toggle effect.
    toggle_effect_ = std::make_unique<VcHostedEffect>(
        /*type=*/VcEffectType::kToggle,
        /*get_state_callback=*/
        base::BindRepeating(&VcTileUiControllerTest::GetEffectState,
                            base::Unretained(this)),
        /*effect_id=*/VcEffectId::kTestEffect);
    toggle_effect_->AddState(std::make_unique<VcEffectState>(
        /*icon=*/&kVideoConferenceNoiseCancellationOnIcon,
        /*label_text=*/u"Dummy label",
        /*accessible_name_id=*/
        IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION,
        /*button_callback=*/
        base::BindRepeating(&VcTileUiControllerTest::ButtonCallback,
                            base::Unretained(this))));
    test_controller_ =
        std::make_unique<VcTileUiController>(toggle_effect_.get());
    haptics_tracker_ = std::make_unique<HapticsTrackingTestInputController>();

    // Create a test `views::Widget` and place the test tile in it.
    test_widget_ = CreateFramelessTestWidget();
    test_widget_->SetBounds(gfx::Rect(0, 0, 100, 100));
    auto test_tile = test_controller()->CreateTile();
    test_tile_ = test_tile->GetWeakPtr();
    test_widget_->SetContentsView(std::move(test_tile));
  }
  void TearDown() override {
    test_widget_.reset();
    haptics_tracker_.reset();
    test_controller_.reset();
    AshTestBase::TearDown();
    fake_vc_tray_controller_.reset();
    fake_vc_effect_manager_.reset();
    DlcserviceClient::Shutdown();
  }

  VcEffectId GetTestEffectId() { return test_controller()->effect_id_; }

  // Returns the count of haptics effects since the test started. If `toggle_on`
  // is true then the returned count corresponds to the "toggle on" haptic, and
  // if it is false then the returned count corresponds to the "toggle off"
  // haptic.
  int GetHapticsToggleCount(bool toggle_on) {
    return haptics_tracker_->GetSentHapticCount(
        toggle_on ? ui::HapticTouchpadEffect::kToggleOn
                  : ui::HapticTouchpadEffect::kToggleOff,
        ui::HapticTouchpadEffectStrength::kMedium);
  }

  // Returns the expected tooltip text given the toggle state.
  std::u16string GetToggleButtonExpectedTooltipText(bool toggle_on) {
    return l10n_util::GetStringFUTF16(
        VIDEO_CONFERENCE_TOGGLE_BUTTON_TOOLTIP,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION),
        l10n_util::GetStringUTF16(
            toggle_on ? VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_ON
                      : VIDEO_CONFERENCE_TOGGLE_BUTTON_STATE_OFF));
  }

  void TrackToggleUMA(bool target_toggle_state) {
    test_controller()->TrackToggleUMA(target_toggle_state);
  }

  void PlayToggleHaptic(bool target_toggle_state) {
    test_controller()->PlayToggleHaptic(target_toggle_state);
  }

  void SetInitialToggleState(bool target_toggle_state) {
    initial_toggle_state_ = target_toggle_state;
  }

  void ButtonCallback() { ++button_callback_invocation_count_; }

  size_t GetButtonCallbackInvocationCount() {
    return button_callback_invocation_count_;
  }

  void SetDlcError(std::string_view id, std::string_view error) {
    static_cast<FakeDlcserviceClient*>(DlcserviceClient::Get())
        ->set_get_dlc_state_error(id, error);
  }

  void SetDlcState(std::string_view id,
                   dlcservice::DlcState_State state,
                   double progress,
                   bool notify = false) {
    FakeDlcserviceClient* fake_dlcservice_client =
        static_cast<FakeDlcserviceClient*>(DlcserviceClient::Get());
    dlcservice::DlcState dlc_state;
    dlc_state.set_id(std::string(id));
    dlc_state.set_state(state);
    dlc_state.set_progress(progress);
    fake_dlcservice_client->set_dlc_state(id, dlc_state);
    if (notify) {
      fake_dlcservice_client->NotifyObserversForTest(dlc_state);
    }
  }

  FeatureTile* test_tile() { return test_tile_.get(); }
  VcTileUiController* test_controller() { return test_controller_.get(); }
  FakeVideoConferenceTrayEffectsManager* effects_manager() {
    return fake_vc_effect_manager_.get();
  }

 private:
  std::optional<int> GetEffectState() { return initial_toggle_state_; }

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<FakeVideoConferenceTrayEffectsManager>
      fake_vc_effect_manager_;
  std::unique_ptr<FakeVideoConferenceTrayController> fake_vc_tray_controller_;
  std::unique_ptr<VcHostedEffect> toggle_effect_;
  std::unique_ptr<views::Widget> test_widget_;
  std::unique_ptr<VcTileUiController> test_controller_;
  std::unique_ptr<HapticsTrackingTestInputController> haptics_tracker_;
  base::WeakPtr<FeatureTile> test_tile_;
  bool initial_toggle_state_ = false;
  size_t button_callback_invocation_count_ = 0;
};

// Tests that the tile's initial toggle state matches the state of the
// corresponding VC effect.
TEST_F(VcTileUiControllerTest, InitialToggleState) {
  // Explicitly set the initial effect state to be off.
  SetInitialToggleState(false);

  // Verify that a tile initialized with this state is toggled-off.
  EXPECT_FALSE(test_controller()->CreateTile()->IsToggled());

  // Explicitly set the initial effect state to be on.
  SetInitialToggleState(true);

  // Verify that a tile initialized with this state is toggled-on.
  EXPECT_TRUE(test_controller()->CreateTile()->IsToggled());
}

// Tests that the tile's initial tooltip matches the state of the corresponding
// VC effect.
TEST_F(VcTileUiControllerTest, InitialTooltip) {
  // Explicitly set the initial effect state to be off.
  SetInitialToggleState(false);

  // Verify that a tile initialized with this state is using a "toggled-off"
  // tooltip.
  EXPECT_EQ(GetToggleButtonExpectedTooltipText(/*toggle_on=*/false),
            test_controller()->CreateTile()->GetTooltipText());

  // Explicitly set the initial effect state to be on.
  SetInitialToggleState(true);

  // Verify that a tile initialized with this state is using a "toggled-on"
  // tooltip.
  EXPECT_EQ(GetToggleButtonExpectedTooltipText(/*toggle_on=*/true),
            test_controller()->CreateTile()->GetTooltipText());
}

// Tests that toggling the tile invokes the effect state's button callback.
TEST_F(VcTileUiControllerTest, EffectStateCallbackInvokedWhenToggled) {
  // Verify that the callback has not been called yet.
  ASSERT_EQ(0u, GetButtonCallbackInvocationCount());

  // Toggle the tile.
  LeftClickOn(test_tile());

  // Verify that the callback has been invoked once.
  ASSERT_EQ(1u, GetButtonCallbackInvocationCount());

  // Toggle the tile again.
  LeftClickOn(test_tile());

  // Verify that the callback has been invoked twice.
  ASSERT_EQ(2u, GetButtonCallbackInvocationCount());
}

// Tests that pressing the tile causes it to change its toggle state.
TEST_F(VcTileUiControllerTest, TogglesWhenPressed) {
  // Verify that the test tile is not toggled.
  ASSERT_FALSE(test_tile()->IsToggled());

  // Press the test tile.
  LeftClickOn(test_tile());

  // Verify that the test tile is now toggled.
  EXPECT_TRUE(test_tile()->IsToggled());

  // Press the test tile again.
  LeftClickOn(test_tile());

  // Verify that the test tile is now not toggled.
  EXPECT_FALSE(test_tile()->IsToggled());
}

// Tests that the tooltip is updated when the tile is toggled.
TEST_F(VcTileUiControllerTest, UpdatesTooltipWhenToggled) {
  // Toggle the test tile on.
  LeftClickOn(test_tile());
  ASSERT_TRUE(test_tile()->IsToggled());

  // Verify that the "toggled-on" tooltip is being used.
  EXPECT_EQ(GetToggleButtonExpectedTooltipText(/*toggle_on=*/true),
            test_tile()->GetTooltipText());

  // Toggle the test tile off.
  LeftClickOn(test_tile());

  // Verify that the "toggled-off" tooltip is now being used.
  EXPECT_EQ(GetToggleButtonExpectedTooltipText(/*toggle_on=*/false),
            test_tile()->GetTooltipText());
}

// Tests the download state of a tile whose effect is not associated with any
// DLC.
TEST_F(VcTileUiControllerTest, NoDownloadStateWhenNoDlc) {
  // Remove any potential associations with DLC from the tile's effect.
  effects_manager()->SetDlcIdsForEffectId(VcEffectId::kTestEffect, {});

  // Verify that a tile created for the effect has a download state of
  // `FeatureTile::DownloadState::kNone`.
  EXPECT_EQ(FeatureTile::DownloadState::kNone,
            test_controller()->CreateTile()->download_state_for_testing());
}

// Tests the initial download states of a tile whose effect is associated with a
// single DLC.
TEST_F(VcTileUiControllerTest, InitialDownloadStatesForSingleDlc) {
  // Associate the tile's effect with a single DLC.
  const std::string dlc_id = "test-dlc";
  effects_manager()->SetDlcIdsForEffectId(VcEffectId::kTestEffect, {dlc_id});

  // Case: DLC has an error.
  // Expectation: Tile starts in `FeatureTile::DownloadState::kError` state.
  SetDlcError(dlc_id, dlcservice::kErrorNeedReboot);
  std::unique_ptr<FeatureTile> tile = test_controller()->CreateTile();

  FeatureTileDownloadStateWaiter tile_waiter;
  tile_waiter.Wait(tile.get(),
                   /*expected_state=*/FeatureTile::DownloadState::kError);
  EXPECT_EQ(FeatureTile::DownloadState::kError,
            tile->download_state_for_testing());

  // Reset the DLC error for the following test cases.
  SetDlcError(dlc_id, dlcservice::kErrorNone);

  // Case: DLC is downloading.
  // Expectation: Tile starts in `FeatureTile::DownloadState::kDownloading`
  // state.
  SetDlcState(/*id=*/dlc_id,
              /*state=*/::dlcservice::DlcState_State::DlcState_State_INSTALLING,
              /*progress=*/0.42);
  tile = test_controller()->CreateTile();
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kDownloading);
  EXPECT_EQ(FeatureTile::DownloadState::kDownloading,
            tile->download_state_for_testing());
  EXPECT_EQ(42, tile->download_progress_for_testing());

  // Case: DLC successfully finished downloading.
  // Expectation: Tile starts in `FeatureTile::DownloadState::kDownloaded`
  // state.
  SetDlcState(/*id=*/"test-dlc",
              /*state=*/::dlcservice::DlcState_State::DlcState_State_INSTALLED,
              /*progress=*/1);
  tile = test_controller()->CreateTile();
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kDownloaded);
  EXPECT_EQ(FeatureTile::DownloadState::kDownloaded,
            tile->download_state_for_testing());
}

// Tests the initial download states of a tile whose effect is associated with
// multiple DLCs.
TEST_F(VcTileUiControllerTest, InitialDownloadStatesForMultipleDlcs) {
  // Associate the tile's effect with multiple DLCs.
  const std::string dlc_id_1 = "test-dlc-1";
  const std::string dlc_id_2 = "test-dlc-2";
  effects_manager()->SetDlcIdsForEffectId(VcEffectId::kTestEffect,
                                          {dlc_id_1, dlc_id_2});

  // Case: One of the DLCs has an error.
  // Expectation: Tile starts in `FeatureTile::DownloadState::kError` state.
  SetDlcError(dlc_id_1, dlcservice::kErrorNeedReboot);
  std::unique_ptr<FeatureTile> tile = test_controller()->CreateTile();
  FeatureTileDownloadStateWaiter tile_waiter;
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kError);
  EXPECT_EQ(FeatureTile::DownloadState::kError,
            tile->download_state_for_testing());

  // Case: Some DLCs have an error, some are fully installed.
  // Expectation: Tile starts in `FeatureTile::DownloadState::kError` state.
  SetDlcState(dlc_id_2, dlcservice::DlcState_State::DlcState_State_INSTALLED,
              /*progress=*/1);
  tile = test_controller()->CreateTile();
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kError);
  EXPECT_EQ(FeatureTile::DownloadState::kError,
            tile->download_state_for_testing());

  // Case: All of the DLCs have errors.
  // Expectation: Tile starts in `FeatureTile::DownloadState::kError` state.
  SetDlcError(dlc_id_2, dlcservice::kErrorNeedReboot);
  tile = test_controller()->CreateTile();
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kError);
  EXPECT_EQ(FeatureTile::DownloadState::kError,
            tile->download_state_for_testing());

  // Reset all DLC errors for the following test cases.
  SetDlcError(dlc_id_1, dlcservice::kErrorNone);
  SetDlcError(dlc_id_2, dlcservice::kErrorNone);

  // Case: All DLCs installed.
  // Expectation: Tile starts in `FeatureTile::DownloadState::kDownloaded`
  // state.
  SetDlcState(dlc_id_1, dlcservice::DlcState_State::DlcState_State_INSTALLED,
              /*progress=*/1);
  SetDlcState(dlc_id_2, dlcservice::DlcState_State::DlcState_State_INSTALLED,
              /*progress=*/1);
  tile = test_controller()->CreateTile();
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kDownloaded);
  EXPECT_EQ(FeatureTile::DownloadState::kDownloaded,
            tile->download_state_for_testing());

  // Case: Some DLCs installed, some still in progress.
  // Expectation: Tile starts in `FeatureTile::DownloadState::kDownloading`
  // state.
  SetDlcState(dlc_id_1, dlcservice::DlcState_State::DlcState_State_INSTALLED,
              /*progress=*/1);
  SetDlcState(dlc_id_2, dlcservice::DlcState_State::DlcState_State_INSTALLING,
              /*progress=*/0.5);
  tile = test_controller()->CreateTile();
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kDownloading);
  EXPECT_EQ(FeatureTile::DownloadState::kDownloading,
            tile->download_state_for_testing());
  EXPECT_EQ(75, tile->download_progress_for_testing());

  // Case: All DLCs still in progress.
  // Expectation: Tile starts in `FeatureTile::DownloadState::kDownloading`
  // state.
  SetDlcState(dlc_id_1, dlcservice::DlcState_State::DlcState_State_INSTALLING,
              /*progress=*/0.25);
  SetDlcState(dlc_id_2, dlcservice::DlcState_State::DlcState_State_INSTALLING,
              /*progress=*/0.75);
  tile = test_controller()->CreateTile();
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kDownloading);
  EXPECT_EQ(FeatureTile::DownloadState::kDownloading,
            tile->download_state_for_testing());
  EXPECT_EQ(50, tile->download_progress_for_testing());
}

// Tests that a tile's download state changes to reflect DLC updates, when the
// tile's effect is associated with a single DLC.
TEST_F(VcTileUiControllerTest, ChangingDownloadStateForSingleDlc) {
  // Associate the tile's effect with a single DLC.
  const std::string dlc_id = "test-dlc";
  effects_manager()->SetDlcIdsForEffectId(VcEffectId::kTestEffect, {dlc_id});

  // Case: DLC is installing, then completes successfully.
  // Expectation: Tile changes to `FeatureTile::DownloadState::kDownloaded`
  // state.
  SetDlcState(dlc_id, dlcservice::DlcState_State::DlcState_State_INSTALLING,
              /*progress=*/0.42);
  std::unique_ptr<FeatureTile> tile = test_controller()->CreateTile();
  FeatureTileDownloadStateWaiter tile_waiter;
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kDownloading);
  ASSERT_EQ(FeatureTile::DownloadState::kDownloading,
            tile->download_state_for_testing());
  SetDlcState(dlc_id, dlcservice::DlcState_State::DlcState_State_INSTALLED,
              /*progress=*/1, /*notify=*/true);
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kDownloaded);
  EXPECT_EQ(FeatureTile::DownloadState::kDownloaded,
            tile->download_state_for_testing());

  // Case: DLC is installing, then has an error.
  // Expectation: Tile changes to `FeatureTile::DownloadState::kError` state.
  SetDlcState(dlc_id, dlcservice::DlcState_State::DlcState_State_INSTALLING,
              /*progress=*/0.42);
  tile = test_controller()->CreateTile();
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kDownloading);
  ASSERT_EQ(FeatureTile::DownloadState::kDownloading,
            tile->download_state_for_testing());
  SetDlcError(dlc_id, dlcservice::kErrorNeedReboot);
  SetDlcState(dlc_id, dlcservice::DlcState_State::DlcState_State_NOT_INSTALLED,
              /*progress=*/0, /*notify=*/true);
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kError);
  EXPECT_EQ(FeatureTile::DownloadState::kError,
            tile->download_state_for_testing());
}

// Tests that a tile's download state changes to reflect DLC updates, when the
// tile's effect is associated with multiple DLCs.
TEST_F(VcTileUiControllerTest, ChangingDownloadStateForMultipleDlcs) {
  // Associate the tile's effect with multiple DLCs.
  const std::string dlc_id_1 = "test-dlc-1";
  const std::string dlc_id_2 = "test-dlc-2";
  effects_manager()->SetDlcIdsForEffectId(VcEffectId::kTestEffect,
                                          {dlc_id_1, dlc_id_2});

  // Case: All DLCs are installing, then just one completes successfully (others
  // are still installing).
  // Expectation: Tile remains in `FeatureTile::DownloadState::kDownloading`
  // state.
  SetDlcState(dlc_id_1, dlcservice::DlcState_State_INSTALLING,
              /*progress=*/0.42);
  SetDlcState(dlc_id_2, dlcservice::DlcState_State_INSTALLING,
              /*progress=*/0.58);
  std::unique_ptr<FeatureTile> tile = test_controller()->CreateTile();
  FeatureTileDownloadStateWaiter tile_waiter;
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kDownloading);
  ASSERT_EQ(FeatureTile::DownloadState::kDownloading,
            tile->download_state_for_testing());
  SetDlcState(dlc_id_1, dlcservice::DlcState_State_INSTALLED, /*progress=*/1,
              /*notify=*/true);
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kDownloading);
  EXPECT_EQ(FeatureTile::DownloadState::kDownloading,
            tile->download_state_for_testing());

  // Case: Some DLCs are installed and others are still installing, then the
  // still-installing DLCs complete successfully.
  // Expectation: Tile changes to `FeatureTile::DownloadState::kDownloaded`
  // state.
  SetDlcState(dlc_id_1, dlcservice::DlcState_State_INSTALLING,
              /*progress=*/0.42);
  SetDlcState(dlc_id_2, dlcservice::DlcState_State_INSTALLED,
              /*progress=*/1);
  tile = test_controller()->CreateTile();
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kDownloading);
  ASSERT_EQ(FeatureTile::DownloadState::kDownloading,
            tile->download_state_for_testing());
  SetDlcState(dlc_id_1, dlcservice::DlcState_State_INSTALLED, /*progress=*/1,
              /*notify=*/true);
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kDownloaded);
  EXPECT_EQ(FeatureTile::DownloadState::kDownloaded,
            tile->download_state_for_testing());

  // Case: Some DLCs are installed and others are still installing, then the
  // still-installing DLCs have an error.
  // Expectation: Tile changes to `FeatureTile::DownloadState::kError` state.
  SetDlcState(dlc_id_1, dlcservice::DlcState_State_INSTALLING,
              /*progress=*/0.42);
  SetDlcState(dlc_id_2, dlcservice::DlcState_State_INSTALLED,
              /*progress=*/1);
  tile = test_controller()->CreateTile();
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kDownloading);
  ASSERT_EQ(FeatureTile::DownloadState::kDownloading,
            tile->download_state_for_testing());
  SetDlcError(dlc_id_1, dlcservice::kErrorNeedReboot);
  SetDlcState(dlc_id_1,
              dlcservice::DlcState_State::DlcState_State_NOT_INSTALLED,
              /*progress=*/0, /*notify=*/true);
  tile_waiter.Wait(tile.get(), FeatureTile::DownloadState::kError);
  EXPECT_EQ(FeatureTile::DownloadState::kError,
            tile->download_state_for_testing());
}

// Tests that `VcTileUiController` records toggle metrics when it is instructed
// to.
TEST_F(VcTileUiControllerTest, RecordsHistogramForToggle) {
  // "Toggle-off" test case.
  {
    base::HistogramTester histogram_tester;

    // Instruct the tile controller to track a "toggle-off".
    TrackToggleUMA(/*target_toggle_state=*/false);

    // Verify that the "toggle-off" was recorded.
    histogram_tester.ExpectUniqueSample(
        video_conference_utils::GetEffectHistogramNameForClick(
            GetTestEffectId()),
        /*sample=*/0, /*expected_bucket_count=*/1);
  }

  // "Toggle-on" test case.
  {
    base::HistogramTester histogram_tester;

    // Instruct the tile controller to track a "toggle-on".
    TrackToggleUMA(/*target_toggle_state=*/true);

    // Verify that the "toggle-on" was recorded.
    histogram_tester.ExpectUniqueSample(
        video_conference_utils::GetEffectHistogramNameForClick(
            GetTestEffectId()),
        /*sample=*/1, /*expected_bucket_count=*/1);
  }
}

// Tests that `VcTileUiController` plays haptic toggle effects when it is
// instructed to.
TEST_F(VcTileUiControllerTest, PlaysHapticsForToggle) {
  // Assert there have been no haptics so far.
  ASSERT_EQ(0, GetHapticsToggleCount(/*toggle_on=*/false));
  ASSERT_EQ(0, GetHapticsToggleCount(/*toggle_on=*/true));

  // Instruct the tile controller to play a "toggle-off" haptic.
  PlayToggleHaptic(/*target_toggle_state=*/false);

  // Verify that the "toggle-off" haptic was played.
  EXPECT_EQ(1, GetHapticsToggleCount(/*toggle_on=*/false));
  EXPECT_EQ(0, GetHapticsToggleCount(/*toggle_on=*/true));

  // Instruct the tile controller to play a "toggle-on" haptic.
  PlayToggleHaptic(/*target_toggle_state=*/true);

  // Verify that the "toggle-on" haptic was played.
  EXPECT_EQ(1, GetHapticsToggleCount(/*toggle_on=*/false));
  EXPECT_EQ(1, GetHapticsToggleCount(/*toggle_on=*/true));
}

}  // namespace ash::video_conference
