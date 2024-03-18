// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_context.h"

#include <cstdint>
#include <memory>
#include <string>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/game_dashboard/game_dashboard_button.h"
#include "ash/game_dashboard/game_dashboard_constants.h"
#include "ash/game_dashboard/game_dashboard_context_test_api.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/game_dashboard/game_dashboard_main_menu_view.h"
#include "ash/game_dashboard/game_dashboard_metrics.h"
#include "ash/game_dashboard/game_dashboard_test_base.h"
#include "ash/game_dashboard/game_dashboard_toolbar_view.h"
#include "ash/game_dashboard/game_dashboard_utils.h"
#include "ash/game_dashboard/test_game_dashboard_delegate.h"
#include "ash/public/cpp/arc_game_controls_flag.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/color_palette_controller.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/switch.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_util.h"
#include "base/check.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/timer/timer.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/wm/window_util.h"
#include "components/ukm/test_ukm_recorder.h"
#include "extensions/common/constants.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

using ToolbarSnapLocation = GameDashboardContext::ToolbarSnapLocation;

// Sub-label strings.
const std::u16string& hidden_label = u"Hidden";
const std::u16string& visible_label = u"Visible";

// Metrics entry names which should be kept in sync with the event names  in
// tools/metrics/ukm.xml.
constexpr char kEntryNameToggleMainMenu[] = "GameDashboard.ToggleMainMenu";
constexpr char kEntryNameToolbarToggleState[] =
    "GameDashboard.ToolbarToggleState";
constexpr char kEntryNameRecordingStartSource[] =
    "GameDashboard.RecordingStartSource";
constexpr char kEntryNameScreenshotTakeSource[] =
    "GameDashboard.ScreenshotTakeSource";
constexpr char kEntryNameGameControlsEditWithEmptyState[] =
    "GameDashboard.EditControlsWithEmptyState";

enum class Movement { kTouch, kMouse };

// Verifies histogram values related to toggling main menu. `histograms_values`
// is related to enum `GameDashboardMainMenuToggleMethod` with the same order.
void VerifyToggleMainMenuHistogram(const base::HistogramTester& histograms,
                                   const std::string& histogram_name,
                                   const std::vector<int>& histograms_values) {
  DCHECK_EQ(7u, histograms_values.size());
  histograms.ExpectBucketCount(
      histogram_name, GameDashboardMainMenuToggleMethod::kGameDashboardButton,
      histograms_values[0]);
  histograms.ExpectBucketCount(histogram_name,
                               GameDashboardMainMenuToggleMethod::kSearchPlusG,
                               histograms_values[1]);
  histograms.ExpectBucketCount(histogram_name,
                               GameDashboardMainMenuToggleMethod::kEsc,
                               histograms_values[2]);
  histograms.ExpectBucketCount(
      histogram_name, GameDashboardMainMenuToggleMethod::kActivateNewFeature,
      histograms_values[3]);
  histograms.ExpectBucketCount(histogram_name,
                               GameDashboardMainMenuToggleMethod::kOverview,
                               histograms_values[4]);
  histograms.ExpectBucketCount(histogram_name,
                               GameDashboardMainMenuToggleMethod::kOthers,
                               histograms_values[5]);
  histograms.ExpectBucketCount(histogram_name,
                               GameDashboardMainMenuToggleMethod::kTabletMode,
                               histograms_values[6]);
}

void VerifyToggleToolbarHistogram(const base::HistogramTester& histograms,
                                  const std::vector<int>& histograms_values) {
  DCHECK_EQ(2u, histograms_values.size());
  const std::string histogram_name = BuildGameDashboardHistogramName(
      kGameDashboardToolbarToggleStateHistogram);
  histograms.ExpectBucketCount(histogram_name, false, histograms_values[0]);
  histograms.ExpectBucketCount(histogram_name, true, histograms_values[1]);
}

void VerifyStartRecordingHistogram(const base::HistogramTester& histograms,
                                   const std::vector<int>& histograms_values) {
  const std::string histogram_name = BuildGameDashboardHistogramName(
      kGameDashboardRecordingStartSourceHistogram);
  DCHECK_EQ(2u, histograms_values.size());
  histograms.ExpectBucketCount(histogram_name, GameDashboardMenu::kMainMenu,
                               histograms_values[0]);
  histograms.ExpectBucketCount(histogram_name, GameDashboardMenu::kToolbar,
                               histograms_values[1]);
}

void VerifyTakeScreenshotHistogram(const base::HistogramTester& histograms,
                                   const std::vector<int>& histograms_values) {
  DCHECK_EQ(2u, histograms_values.size());
  const std::string histogram_name = BuildGameDashboardHistogramName(
      kGameDashboardScreenshotTakeSourceHistogram);
  histograms.ExpectBucketCount(histogram_name, GameDashboardMenu::kMainMenu,
                               histograms_values[0]);
  histograms.ExpectBucketCount(histogram_name, GameDashboardMenu::kToolbar,
                               histograms_values[1]);
}

void VerifyGameControlsEditControlsWithEmptyStateHistogram(
    const base::HistogramTester& histograms,
    const std::vector<int>& histograms_values) {
  DCHECK_EQ(2u, histograms_values.size());
  const std::string histogram_name = BuildGameDashboardHistogramName(
      kGameDashboardEditControlsWithEmptyStateHistogram);
  histograms.ExpectBucketCount(histogram_name, false, histograms_values[0]);
  histograms.ExpectBucketCount(histogram_name, true, histograms_values[1]);
}

// Verifies UKM event entry size of ToggleMainMenu is `expect_entry_size` and
// the last event entry metric values match `expect_histograms_values`.
void VerifyToggleMainMenuLastUkmHistogram(
    const ukm::TestAutoSetUkmRecorder& ukm_recorder,
    size_t expect_entry_size,
    const std::vector<int64_t>& expect_histograms_values) {
  auto ukm_entries = ukm_recorder.GetEntriesByName(kEntryNameToggleMainMenu);
  EXPECT_EQ(expect_entry_size, ukm_entries.size());
  EXPECT_EQ(2u, expect_histograms_values.size());
  const size_t last_index = expect_entry_size - 1;
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[last_index],
      ukm::builders::GameDashboard_ToggleMainMenu::kToggleOnName,
      expect_histograms_values[0]);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[last_index],
      ukm::builders::GameDashboard_ToggleMainMenu::kToggleMethodName,
      expect_histograms_values[1]);
}

// Verifies UKM event entry size of ToolbarToggleState is `expect_entry_size`
// and the last event entry metric value matches `expect_histograms_value`.
void VerifyToolbarToggleStateLastUkmHistogram(
    const ukm::TestAutoSetUkmRecorder& ukm_recorder,
    size_t expect_entry_size,
    int64_t expect_histograms_value) {
  auto ukm_entries =
      ukm_recorder.GetEntriesByName(kEntryNameToolbarToggleState);
  EXPECT_EQ(expect_entry_size, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[expect_entry_size - 1],
      ukm::builders::GameDashboard_ToolbarToggleState::kToggleOnName,
      expect_histograms_value);
}

// Verifies UKM event entry size of RecordingStartSource is `expect_entry_size`
// and the last event entry metric value matches `expect_histograms_value`.
void VerifyRecordingStartSourceLastUkmHistogram(
    const ukm::TestAutoSetUkmRecorder& ukm_recorder,
    size_t expect_entry_size,
    int64_t expect_histograms_value) {
  auto ukm_entries =
      ukm_recorder.GetEntriesByName(kEntryNameRecordingStartSource);
  EXPECT_EQ(expect_entry_size, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[expect_entry_size - 1],
      ukm::builders::GameDashboard_RecordingStartSource::kSourceName,
      expect_histograms_value);
}

// Verifies UKM event entry size of ScreenshotTakeSource is `expect_entry_size`
// and the last event entry metric value matches `expect_histograms_value`.
void VerifyScreenshotTakeSourceLastUkmHistogram(
    const ukm::TestAutoSetUkmRecorder& ukm_recorder,
    size_t expect_entry_size,
    int64_t expect_histograms_value) {
  auto ukm_entries =
      ukm_recorder.GetEntriesByName(kEntryNameScreenshotTakeSource);
  EXPECT_EQ(expect_entry_size, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[expect_entry_size - 1],
      ukm::builders::GameDashboard_ScreenshotTakeSource::kSourceName,
      expect_histograms_value);
}

// Verifies UKM event entry size of ControlsEditControlsWithEmptyState is
// `expect_entry_size` and the last event entry metric value matches
// `expect_histograms_value`.
void VerifyGameControlsEditControlsWithEmptyStateLastUkmHistogram(
    const ukm::TestAutoSetUkmRecorder& ukm_recorder,
    size_t expect_entry_size,
    int64_t expect_histograms_value) {
  auto ukm_entries =
      ukm_recorder.GetEntriesByName(kEntryNameGameControlsEditWithEmptyState);
  EXPECT_EQ(expect_entry_size, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[expect_entry_size - 1],
      ukm::builders::GameDashboard_EditControlsWithEmptyState::kEmptyName,
      expect_histograms_value);
}

// Records the last mouse event for testing.
class EventCapturer : public ui::EventHandler {
 public:
  EventCapturer() = default;
  EventCapturer(const EventCapturer&) = delete;
  EventCapturer& operator=(const EventCapturer&) = delete;
  ~EventCapturer() override {}

  void Reset() { last_mouse_event_.reset(); }

  ui::MouseEvent* last_mouse_event() { return last_mouse_event_.get(); }

 private:
  void OnMouseEvent(ui::MouseEvent* event) override {
    last_mouse_event_ = std::make_unique<ui::MouseEvent>(*event);
  }

  std::unique_ptr<ui::MouseEvent> last_mouse_event_;
};

}  // namespace

class GameDashboardContextTest : public GameDashboardTestBase {
 public:
  GameDashboardContextTest() = default;
  GameDashboardContextTest(const GameDashboardContextTest&) = delete;
  GameDashboardContextTest& operator=(const GameDashboardContextTest&) = delete;
  ~GameDashboardContextTest() override = default;

  void SetUp() override {
    GameDashboardTestBase::SetUp();
    // Disable the welcome dialog by default.
    active_user_prefs_ =
        Shell::Get()->session_controller()->GetActivePrefService();
    ASSERT_TRUE(active_user_prefs_);
    SetShowWelcomeDialog(false);
    SetShowToolbar(false);
    GetContext()->AddPostTargetHandler(&post_target_event_capturer_);
  }

  void TearDown() override {
    active_user_prefs_ = nullptr;
    GetContext()->RemovePostTargetHandler(&post_target_event_capturer_);
    CloseGameWindow();
    GameDashboardTestBase::TearDown();
  }

  void CloseGameWindow() {
    game_window_.reset();
    test_api_.reset();
    frame_header_height_ = 0;
  }

  const gfx::Rect app_bounds() const { return app_bounds_; }

  void SetAppBounds(gfx::Rect app_bounds) {
    CHECK(!game_window_)
        << "App bounds cannot be changed after creating window. To set the app "
           "bounds, call CloseWindow() and re-call this function.";
    app_bounds_ = app_bounds;
  }

  int GetToolbarHeight() {
    auto* widget = test_api_->GetToolbarWidget();
    CHECK(widget) << "The toolbar must be opened first before trying to "
                     "retrieve its height.";
    return widget->GetNativeWindow()->GetBoundsInScreen().height();
  }

  // Starts the video recording from `CaptureModeBarView`.
  void ClickOnStartRecordingButtonInCaptureModeBarView() {
    PillButton* start_recording_button = GetStartRecordingButton();
    ASSERT_TRUE(start_recording_button);
    LeftClickOn(start_recording_button);
    WaitForRecordingToStart();
    EXPECT_TRUE(CaptureModeController::Get()->is_recording_in_progress());
  }

  // Sets the `pref` boolean preference with `value`.
  // NOTE: This function should be called before CreateGameWindow() is called.
  void SetBooleanPref(const std::string& pref, bool value) {
    CHECK(!game_window_) << "\"" << pref
                         << "\" should be changed before "
                            "creating the window. To set this param, call this "
                            "function before CreateGameWindow() is called.";
    active_user_prefs_->SetBoolean(pref, value);
    ASSERT_EQ(active_user_prefs_->GetBoolean(pref), value);
  }

  // Sets whether the welcome dialog should be displayed when a game window
  // opens, which is determined by the `show_dialog` param.
  void SetShowWelcomeDialog(bool show_dialog) {
    SetBooleanPref(prefs::kGameDashboardShowWelcomeDialog, show_dialog);
  }

  // Sets whether the toolbar should be displayed when a game window opens,
  // which is determined by the `show_toolbar` param.
  void SetShowToolbar(bool show_toolbar) {
    SetBooleanPref(prefs::kGameDashboardShowToolbar, show_toolbar);
  }

  // If `is_arc_window` is true, this function creates the window as an ARC
  // game window. Otherwise, it creates the window as a GeForceNow window.
  // For ARC game windows, if `set_arc_game_controls_flags_prop` is true, then
  // the `kArcGameControlsFlagsKey` window property will be set to
  // `ArcGameControlsFlag::kKnown`, otherwise the property will not be set.
  void CreateGameWindow(bool is_arc_window,
                        bool set_arc_game_controls_flags_prop = true) {
    ASSERT_FALSE(game_window_);
    ASSERT_FALSE(test_api_);
    game_window_ = CreateAppWindow(
        (is_arc_window ? TestGameDashboardDelegate::kGameAppId
                       : extension_misc::kGeForceNowAppId),
        (is_arc_window ? AppType::ARC_APP : AppType::NON_APP), app_bounds());
    auto* context = GameDashboardController::Get()->GetGameDashboardContext(
        game_window_.get());
    ASSERT_TRUE(context);
    test_api_ = std::make_unique<GameDashboardContextTestApi>(
        context, GetEventGenerator());
    ASSERT_TRUE(test_api_);
    frame_header_height_ =
        game_dashboard_utils::GetFrameHeaderHeight(game_window_.get());
    DCHECK_GT(frame_header_height_, 0);

    if (is_arc_window && set_arc_game_controls_flags_prop) {
      // Initially, Game Controls is not available.
      game_window_->SetProperty(kArcGameControlsFlagsKey,
                                ArcGameControlsFlag::kKnown);
    }

    auto* game_dashboard_button_widget =
        test_api_->GetGameDashboardButton()->GetWidget();
    CHECK(game_dashboard_button_widget);
    ASSERT_FALSE(game_dashboard_button_widget->CanActivate());
    ASSERT_FALSE(game_dashboard_button_widget->IsActive());

    // Using `prefs::kGameDashboardShowWelcomeDialog`, verify whether the
    // welcome dialog should be shown.
    if (active_user_prefs_->GetBoolean(
            prefs::kGameDashboardShowWelcomeDialog) &&
        game_dashboard_utils::ShouldEnableFeatures()) {
      ASSERT_TRUE(test_api_->GetWelcomeDialogWidget());
    } else {
      ASSERT_FALSE(test_api_->GetWelcomeDialogWidget());
    }
  }

  // Opens the main menu and toolbar, and checks Game Controls UI states. At the
  // end of the test, closes the main menu and toolbar.
  // `hint_tile_states` is about feature tile states, {expect_exists,
  // expect_enabled, expect_on}.
  // `details_row_states` is about the Game Controls details row states,
  // {expect_exists, expect_enabled}. `feature_switch_states` is about feature
  // switch button states, {expect_exists, expect_toggled}. `setup_exists` shows
  // if setup button exists.
  void OpenMenuCheckGameControlsUIState(
      std::array<bool, 3> hint_tile_states,
      std::array<bool, 2> details_row_states,
      std::array<bool, 2> feature_switch_states,
      bool setup_exists) {
    test_api_->OpenTheMainMenu();

    if (const auto* tile = test_api_->GetMainMenuGameControlsTile();
        hint_tile_states[0]) {
      ASSERT_TRUE(tile);
      EXPECT_EQ(hint_tile_states[1], tile->GetEnabled());
      EXPECT_EQ(hint_tile_states[2], tile->IsToggled());
    } else {
      EXPECT_FALSE(tile);
    }

    auto* details_row = test_api_->GetMainMenuGameControlsDetailsButton();
    EXPECT_EQ(details_row_states[0], !!details_row);
    if (details_row) {
      EXPECT_EQ(details_row_states[1], details_row->GetEnabled());
    }

    if (const auto* switch_button =
            test_api_->GetMainMenuGameControlsFeatureSwitch();
        feature_switch_states[0]) {
      ASSERT_TRUE(switch_button);
      EXPECT_EQ(feature_switch_states[1], switch_button->GetIsOn());
    } else {
      EXPECT_FALSE(switch_button);
    }

    auto* setup_button = test_api_->GetMainMenuGameControlsSetupButton();
    ASSERT_EQ(!!setup_button, setup_exists);
    if (setup_button) {
      EXPECT_EQ(details_row_states[1], setup_button->GetEnabled());
    }

    // Open toolbar and check the toolbar's Game Controls button state.
    test_api_->OpenTheToolbar();
    // The button state has the same state as the hint tile on the main menu.
    if (const auto* game_controls_button =
            test_api_->GetToolbarGameControlsButton();
        hint_tile_states[0]) {
      ASSERT_TRUE(game_controls_button);
      EXPECT_EQ(hint_tile_states[1], game_controls_button->GetEnabled());
      EXPECT_EQ(hint_tile_states[2], game_controls_button->toggled());
    } else {
      EXPECT_FALSE(game_controls_button);
    }

    test_api_->CloseTheToolbar();
    test_api_->CloseTheMainMenu();
  }

  void VerifyToolbarDrag(Movement move_type) {
    test_api_->OpenTheMainMenu();
    test_api_->OpenTheToolbar();
    const auto window_bounds = game_window_->GetBoundsInScreen();
    const auto window_center_point = window_bounds.CenterPoint();
    const int x_offset = window_bounds.width() / 4;
    const int y_offset = window_bounds.height() / 4;

    // Verify that be default the snap position should be `kTopRight` and
    // toolbar is placed in the top right quadrant.
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
              ToolbarSnapLocation::kTopRight);

    // Move toolbar but not outside of the top right quadrant. Tests that even
    // though the snap position does not change, the toolbar is snapped back to
    // its previous position.
    DragToolbarToPoint(move_type, {window_center_point.x() + x_offset,
                                   window_center_point.y() - y_offset});
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
              ToolbarSnapLocation::kTopRight);

    // Move toolbar to bottom right quadrant and verify snap location is
    // updated.
    DragToolbarToPoint(move_type, {window_center_point.x() + x_offset,
                                   window_center_point.y() + y_offset});
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
              ToolbarSnapLocation::kBottomRight);

    // Move toolbar to bottom left quadrant and verify snap location is updated.
    DragToolbarToPoint(move_type, {window_center_point.x() - x_offset,
                                   window_center_point.y() + y_offset});
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
              ToolbarSnapLocation::kBottomLeft);

    // Move toolbar to top left quadrant and verify snap location is updated.
    DragToolbarToPoint(move_type, {window_center_point.x() - x_offset,
                                   window_center_point.y() - y_offset});
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
              ToolbarSnapLocation::kTopLeft);
  }

  // Verifies the Game Dashboard button is in the respective state for the given
  // `test_api`. If `is_recording` is true, then the Game Dashboard button must
  // be in the recording state, and the recording timer is running. Otherwise,
  // it should be in the default state and the timer should not be running.
  void VerifyGameDashboardButtonState(GameDashboardContextTestApi* test_api,
                                      bool is_recording) {
    EXPECT_EQ(is_recording, test_api->GetGameDashboardButton()->is_recording());

    std::u16string expected_title;
    if (is_recording) {
      expected_title = l10n_util::GetStringFUTF16(
          IDS_ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_RECORDING,
          test_api->GetRecordingDuration());
    } else {
      expected_title = l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_TITLE);
    }
    EXPECT_EQ(expected_title,
              test_api->GetGameDashboardButtonTitle()->GetText());
  }

  void VerifyGameDashboardButtonState(bool is_recording) {
    VerifyGameDashboardButtonState(test_api_.get(), is_recording);
  }

  // Starts recording `recording_window_test_api`'s window, and verifies its
  // record game buttons are enabled and toggled on, while the record game
  // buttons in `other_window_test_api` are disabled and toggled off.
  void RecordGameAndVerifyButtons(
      GameDashboardContextTestApi* recording_window_test_api,
      GameDashboardContextTestApi* other_window_test_api) {
    // Verify the initial state of the record buttons.
    for (auto* test_api : {recording_window_test_api, other_window_test_api}) {
      wm::ActivateWindow(test_api->context()->game_window());

      test_api->OpenTheMainMenu();
      const auto* record_game_tile = test_api->GetMainMenuRecordGameTile();
      ASSERT_TRUE(record_game_tile);
      EXPECT_TRUE(record_game_tile->GetEnabled());
      EXPECT_FALSE(record_game_tile->IsToggled());

      test_api->OpenTheToolbar();
      const auto* record_game_button = test_api->GetToolbarRecordGameButton();
      ASSERT_TRUE(record_game_button);
      EXPECT_TRUE(record_game_button->GetEnabled());
      EXPECT_FALSE(record_game_button->toggled());
    }
    const auto& recording_window_timer =
        recording_window_test_api->GetRecordingTimer();
    const auto& other_window_timer = other_window_test_api->GetRecordingTimer();

    // Verify the recording timer is not running in both windows.
    EXPECT_FALSE(recording_window_timer.IsRunning());
    EXPECT_FALSE(other_window_timer.IsRunning());

    // Verify the game dashboard buttons are not in the recording state.
    VerifyGameDashboardButtonState(recording_window_test_api,
                                   /*is_recording=*/false);
    VerifyGameDashboardButtonState(other_window_test_api,
                                   /*is_recording=*/false);

    // Activate the recording_window.
    auto* recording_window =
        recording_window_test_api->context()->game_window();
    ASSERT_TRUE(recording_window);
    wm::ActivateWindow(recording_window);

    // Start recording recording_window.
    recording_window_test_api->OpenTheMainMenu();
    LeftClickOn(recording_window_test_api->GetMainMenuRecordGameTile());
    // Clicking on the record game tile closes the main menu, and asynchronously
    // starts the capture session. Run until idle to ensure that the posted task
    // runs synchronously and completes before proceeding.
    base::RunLoop().RunUntilIdle();
    ClickOnStartRecordingButtonInCaptureModeBarView();

    // Reopen the recording window's main menu, because clicking on the button
    // closed it.
    recording_window_test_api->OpenTheMainMenu();

    // Verify the recording timer is only running in `recording_window`.
    EXPECT_TRUE(recording_window_timer.IsRunning());
    EXPECT_FALSE(other_window_timer.IsRunning());

    // Verify the game dashboard button state.
    VerifyGameDashboardButtonState(recording_window_test_api,
                                   /*is_recording=*/true);
    VerifyGameDashboardButtonState(other_window_test_api,
                                   /*is_recording=*/false);

    // Retrieve the record game buttons for the `recording_window` and verify
    // they're enabled and toggled on.
    VerifyRecordGameStatus(
        recording_window_test_api->GetMainMenuRecordGameTile(),
        recording_window_test_api->GetToolbarRecordGameButton(),
        /*enabled=*/true, /*toggled=*/true);

    // Retrieve the record game buttons for the `other_window`.
    auto* other_window = other_window_test_api->context()->game_window();
    wm::ActivateWindow(other_window);
    other_window_test_api->OpenTheMainMenu();

    // Retrieve the record game buttons for the `other_window` and verify
    // they're disabled and toggled off.
    VerifyRecordGameStatus(other_window_test_api->GetMainMenuRecordGameTile(),
                           other_window_test_api->GetToolbarRecordGameButton(),
                           /*enabled=*/false, /*toggled=*/false);

    // Stop the video recording session.
    wm::ActivateWindow(recording_window);
    recording_window_test_api->OpenTheMainMenu();
    LeftClickOn(recording_window_test_api->GetMainMenuRecordGameTile());
    EXPECT_FALSE(CaptureModeController::Get()->is_recording_in_progress());
    WaitForCaptureFileToBeSaved();

    // TODO(b/286889161): Update the record game button pointers after the bug
    // has been addressed. The main menu will no longer remain open, which makes
    // button pointers invalid.
    // Verify all the record game buttons for the `recording_window` are enabled
    // and toggled off.
    VerifyRecordGameStatus(
        recording_window_test_api->GetMainMenuRecordGameTile(),
        recording_window_test_api->GetToolbarRecordGameButton(),
        /*enabled=*/true, /*toggled=*/false);

    // Verify all the `other_window` buttons are enabled and toggled off.
    wm::ActivateWindow(other_window);
    other_window_test_api->OpenTheMainMenu();
    VerifyRecordGameStatus(other_window_test_api->GetMainMenuRecordGameTile(),
                           other_window_test_api->GetToolbarRecordGameButton(),
                           /*enabled=*/true, /*toggled=*/false);

    // Verify the recording timer is not running in both windows.
    EXPECT_FALSE(recording_window_timer.IsRunning());
    EXPECT_FALSE(other_window_timer.IsRunning());

    // Verify the game dashboard buttons are no longer in the recording state.
    VerifyGameDashboardButtonState(recording_window_test_api,
                                   /*is_recording=*/false);
    VerifyGameDashboardButtonState(other_window_test_api,
                                   /*is_recording=*/false);

    // Close the toolbar and main menu in the `other_window`, which is currently
    // open.
    other_window_test_api->CloseTheToolbar();
    other_window_test_api->CloseTheMainMenu();

    // Open the main menu of the recording window to close the toolbar and then
    // the main menu.
    wm::ActivateWindow(recording_window);
    recording_window_test_api->OpenTheMainMenu();
    recording_window_test_api->CloseTheToolbar();
    recording_window_test_api->CloseTheMainMenu();
  }

  void VerifyRecordGameStatus(FeatureTile* game_tile,
                              IconButton* game_button,
                              bool enabled,
                              bool toggled) {
    ASSERT_TRUE(game_tile);
    ASSERT_TRUE(game_button);
    EXPECT_EQ(enabled, game_tile->GetEnabled());
    EXPECT_EQ(enabled, game_button->GetEnabled());
    EXPECT_EQ(toggled, game_tile->IsToggled());
    EXPECT_EQ(toggled, game_button->toggled());
  }

  void PressKeyAndVerify(ui::KeyboardCode key,
                         ToolbarSnapLocation desired_location) {
    GetEventGenerator()->PressAndReleaseKey(key);
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(), desired_location);
  }

 protected:
  void DragToolbarToPoint(Movement move_type,
                          const gfx::Point& new_location,
                          bool drop = true) {
    const auto* widget = test_api_->GetToolbarWidget();
    DCHECK(widget) << "Cannot drag toolbar because it's unavailable on screen.";
    const auto toolbar_bounds = widget->GetNativeWindow()->GetBoundsInScreen();
    auto* event_generator = GetEventGenerator();
    event_generator->set_current_screen_location(toolbar_bounds.CenterPoint());

    switch (move_type) {
      case Movement::kMouse:
        event_generator->PressLeftButton();
        event_generator->MoveMouseTo(new_location);
        if (drop) {
          event_generator->ReleaseLeftButton();
        }
        break;
      case Movement::kTouch:
        event_generator->PressTouch();
        // Move the touch by an enough amount in X to make sure it generates a
        // series of gesture scroll events instead of a fling event.
        event_generator->MoveTouchBy(50, 0);
        event_generator->MoveTouch(new_location);
        if (drop) {
          event_generator->ReleaseTouch();
        }
        break;
    }

    // Dragging the toolbar causes the main menu to close asynchronously. Run
    // until idle to ensure that this posted task runs synchronously and
    // completes before proceeding.
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<aura::Window> game_window_;
  std::unique_ptr<GameDashboardContextTestApi> test_api_;
  int frame_header_height_ = 0;
  // Post-target handler that captures the last mouse event.
  EventCapturer post_target_event_capturer_;

 private:
  gfx::Rect app_bounds_ = gfx::Rect(50, 50, 800, 400);
  raw_ptr<PrefService> active_user_prefs_;
};

// Verifies Game Controls tile state.
// - The tile exists when Game Controls is available.
// - The tile is disabled if Game Controls has empty actions.
// - The tile can only be toggled when Game Controls has at least one action and
//   Game Controls feature is enabled.
TEST_F(GameDashboardContextTest, GameControlsMenuState) {
  CreateGameWindow(/*is_arc_window=*/true,
                   /*set_arc_game_controls_flags_prop=*/true);

  // Game Controls is not available (GC is optout).
  OpenMenuCheckGameControlsUIState(
      /*hint_tile_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/false, /*expect_on=*/false},
      /*details_row_exists=*/{/*expect_exists=*/true, /*expect_enabled=*/false},
      /*feature_switch_states=*/
      {/*expect_exists=*/false, /*expect_toggled=*/false},
      /*setup_exists=*/true);

  // Game Controls is available, not empty, but not enabled.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                       ArcGameControlsFlag::kAvailable));
  OpenMenuCheckGameControlsUIState(
      /*hint_tile_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/false, /*expect_on=*/false},
      /*details_row_exists=*/{/*expect_exists=*/true, /*expect_enabled=*/true},
      /*feature_switch_states=*/
      {/*expect_exists=*/true, /*expect_toggled=*/false},
      /*setup_exists=*/false);

  // Game Controls is available, but empty. Even Game Controls is set enabled,
  // the tile is disabled and can't be toggled.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(
          ArcGameControlsFlag::kKnown | ArcGameControlsFlag::kAvailable |
          ArcGameControlsFlag::kEmpty | ArcGameControlsFlag::kEnabled));
  OpenMenuCheckGameControlsUIState(
      /*hint_tile_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/false, /*expect_on=*/false},
      /*details_row_exists=*/{/*expect_exists=*/true, /*expect_enabled=*/true},
      /*feature_switch_states=*/
      {/*expect_exists=*/false, /*expect_toggled=*/false},
      /*setup_exists=*/true);

  // Game controls is available, not empty, enabled and no mapping hint.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                       ArcGameControlsFlag::kAvailable |
                                       ArcGameControlsFlag::kEnabled));
  OpenMenuCheckGameControlsUIState(
      /*hint_tile_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/true, /*expect_on=*/false},
      /*details_row_exists=*/{/*expect_exists=*/true, /*expect_enabled=*/true},
      /*feature_switch_states=*/
      {/*expect_exists=*/true, /*expect_toggled=*/true},
      /*setup_exists=*/false);

  // Game controls is available, not empty, enabled and has mapping hint on.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(
          ArcGameControlsFlag::kKnown | ArcGameControlsFlag::kAvailable |
          ArcGameControlsFlag::kEnabled | ArcGameControlsFlag::kHint));
  OpenMenuCheckGameControlsUIState(
      /*hint_tile_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/true, /*expect_on=*/true},
      /*details_row_exists=*/{/*expect_exists=*/true, /*expect_enabled=*/true},
      /*feature_switch_states=*/
      {/*expect_exists=*/true, /*expect_toggled=*/true},
      /*setup_exists=*/false);
}

TEST_F(GameDashboardContextTest, GameControlsSetupNudge) {
  CreateGameWindow(/*is_arc_window=*/true);

  // Test setup nudge for non-O4C games.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(
          ArcGameControlsFlag::kKnown | ArcGameControlsFlag::kAvailable |
          ArcGameControlsFlag::kEmpty | ArcGameControlsFlag::kEnabled));

  test_api_->OpenTheMainMenu();
  EXPECT_TRUE(test_api_->GetGameControlsSetupNudge());
  task_environment()->FastForwardBy(
      AnchoredNudgeManagerImpl::kNudgeMediumDuration);
  EXPECT_FALSE(test_api_->GetGameControlsSetupNudge());
  test_api_->CloseTheMainMenu();

  // Test setup nudge for O4C games.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(
          ArcGameControlsFlag::kKnown | ArcGameControlsFlag::kAvailable |
          ArcGameControlsFlag::kEmpty | ArcGameControlsFlag::kEnabled |
          ArcGameControlsFlag::kO4C));
  test_api_->OpenTheMainMenu();
  EXPECT_FALSE(test_api_->GetGameControlsSetupNudge());
}

// Verifies Game Controls button logics.
TEST_F(GameDashboardContextTest, GameControlsMenuFunctions) {
  CreateGameWindow(/*is_arc_window=*/true);

  // Game Controls is available, not empty, enabled and hint on.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(
          ArcGameControlsFlag::kKnown | ArcGameControlsFlag::kAvailable |
          ArcGameControlsFlag::kEnabled | ArcGameControlsFlag::kHint));
  EXPECT_FALSE(game_dashboard_utils::IsFlagSet(
      game_window_->GetProperty(kArcGameControlsFlagsKey),
      ArcGameControlsFlag::kMenu));

  test_api_->OpenTheMainMenu();
  // Disable Game Controls.
  EXPECT_TRUE(game_dashboard_utils::IsFlagSet(
      game_window_->GetProperty(kArcGameControlsFlagsKey),
      ArcGameControlsFlag::kMenu));
  test_api_->OpenTheToolbar();

  auto* detail_row = test_api_->GetMainMenuGameControlsDetailsButton();
  auto* switch_button = test_api_->GetMainMenuGameControlsFeatureSwitch();
  auto* game_controls_button = test_api_->GetToolbarGameControlsButton();
  EXPECT_TRUE(detail_row->GetEnabled());
  EXPECT_TRUE(switch_button->GetEnabled());
  EXPECT_TRUE(switch_button->GetIsOn());
  EXPECT_TRUE(game_controls_button->GetEnabled());
  EXPECT_TRUE(game_controls_button->toggled());
  // Disable Game Controls.
  LeftClickOn(switch_button);
  EXPECT_TRUE(detail_row->GetEnabled());
  EXPECT_TRUE(switch_button->GetEnabled());
  EXPECT_FALSE(switch_button->GetIsOn());
  // Toolbar button should also get updated.
  EXPECT_FALSE(game_controls_button->GetEnabled());

  EXPECT_FALSE(game_dashboard_utils::IsFlagSet(
      game_window_->GetProperty(kArcGameControlsFlagsKey),
      ArcGameControlsFlag::kHint));

  // Since Game Controls is disabled, press on `detail_row` should not turn on
  // `kEdit` flag.
  LeftClickOn(detail_row);
  EXPECT_FALSE(game_dashboard_utils::IsFlagSet(
      game_window_->GetProperty(kArcGameControlsFlagsKey),
      ArcGameControlsFlag::kEdit));

  test_api_->CloseTheToolbar();
  test_api_->CloseTheMainMenu();
  EXPECT_FALSE(game_dashboard_utils::IsFlagSet(
      game_window_->GetProperty(kArcGameControlsFlagsKey),
      ArcGameControlsFlag::kMenu));

  // Open the main menu again to check if the states are preserved and close it.
  OpenMenuCheckGameControlsUIState(
      /*hint_tile_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/false, /*expect_on=*/false},
      /*details_row_exists=*/{/*expect_exists=*/true, /*expect_enabled=*/true},
      /*feature_switch_states=*/
      {/*expect_exists=*/true, /*expect_toggled=*/false},
      /*setup_exists=*/false);

  // Open the main menu and toolbar. Enable Game Controls and switch hint button
  // off.
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  detail_row = test_api_->GetMainMenuGameControlsDetailsButton();
  switch_button = test_api_->GetMainMenuGameControlsFeatureSwitch();
  game_controls_button = test_api_->GetToolbarGameControlsButton();
  const auto* game_controls_tile = test_api_->GetMainMenuGameControlsTile();
  // Enable Game Controls.
  LeftClickOn(switch_button);
  EXPECT_TRUE(detail_row->GetEnabled());
  EXPECT_TRUE(switch_button->GetEnabled());
  EXPECT_TRUE(switch_button->GetIsOn());
  EXPECT_TRUE(game_controls_button->GetEnabled());
  EXPECT_TRUE(game_controls_button->toggled());
  EXPECT_TRUE(game_controls_tile->IsToggled());
  // Switch hint off.
  LeftClickOn(game_controls_tile);
  test_api_->CloseTheToolbar();
  test_api_->CloseTheMainMenu();

  // Open the main menu again to check if the states are preserved and close it.
  OpenMenuCheckGameControlsUIState(
      /*hint_tile_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/true, /*expect_on=*/false},
      /*details_row_exists=*/{/*expect_exists=*/true, /*expect_enabled=*/true},
      /*feature_switch_states=*/
      {/*expect_exists=*/true, /*expect_toggled=*/true},
      /*setup_exists=*/false);
}

// Verify Game Dashboard button is disabled and toolbar hides in the edit mode.
TEST_F(GameDashboardContextTest, GameControlsEditMode) {
  CreateGameWindow(/*is_arc_window=*/true);
  // Game Controls is available, not empty, enabled and hint on.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(
          ArcGameControlsFlag::kKnown | ArcGameControlsFlag::kAvailable |
          ArcGameControlsFlag::kEnabled | ArcGameControlsFlag::kHint));
  auto* game_dashboard_button = test_api_->GetGameDashboardButton();
  EXPECT_TRUE(game_dashboard_button->GetEnabled());
  LeftClickOn(game_dashboard_button);
  EXPECT_TRUE(test_api_->GetMainMenuWidget());
  // Show the toolbar.
  test_api_->OpenTheToolbar();
  auto* tool_bar_widget = test_api_->GetToolbarWidget();
  EXPECT_TRUE(tool_bar_widget);
  EXPECT_TRUE(tool_bar_widget->IsVisible());

  // Enter Game Controls edit mode.
  LeftClickOn(test_api_->GetMainMenuGameControlsDetailsButton());
  EXPECT_TRUE(game_dashboard_utils::IsFlagSet(
      game_window_->GetProperty(kArcGameControlsFlagsKey),
      ArcGameControlsFlag::kEdit));
  EXPECT_FALSE(test_api_->GetMainMenuWidget());
  EXPECT_FALSE(tool_bar_widget->IsVisible());
  // In the edit mode, Game Dashboard button is disabled and it doesn't show
  // menu after clicked. The toolbar is also hidden if it shows up.
  EXPECT_FALSE(game_dashboard_button->GetEnabled());
  LeftClickOn(game_dashboard_button);
  EXPECT_FALSE(test_api_->GetMainMenuWidget());
  // Exit edit mode and verify Game Dashboard button and toolbar are resumed.
  ArcGameControlsFlag flags =
      game_window_->GetProperty(kArcGameControlsFlagsKey);
  flags = game_dashboard_utils::UpdateFlag(flags, ArcGameControlsFlag::kEdit,
                                           /*enable_flag=*/false);
  game_window_->SetProperty(kArcGameControlsFlagsKey, flags);
  EXPECT_TRUE(game_dashboard_button->GetEnabled());
  LeftClickOn(game_dashboard_button);
  EXPECT_TRUE(test_api_->GetMainMenuWidget());
  EXPECT_TRUE(tool_bar_widget->IsVisible());
}

TEST_F(GameDashboardContextTest,
       RecordEditControlsWithEmptyStateHistogramTest) {
  CreateGameWindow(/*is_arc_window=*/true);
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Game Controls is available, not empty, enabled and hint on.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(
          ArcGameControlsFlag::kKnown | ArcGameControlsFlag::kAvailable |
          ArcGameControlsFlag::kEnabled | ArcGameControlsFlag::kHint));
  test_api_->OpenTheMainMenu();
  LeftClickOn(test_api_->GetMainMenuGameControlsDetailsButton());
  VerifyGameControlsEditControlsWithEmptyStateHistogram(
      histograms, std::vector<int>{/*not_setup=*/1, 0});
  VerifyGameControlsEditControlsWithEmptyStateLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/1u, /*expect_histograms_value=*/0);

  // Game Controls is available, empty, enabled and hint on.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(
          ArcGameControlsFlag::kKnown | ArcGameControlsFlag::kAvailable |
          ArcGameControlsFlag::kEnabled | ArcGameControlsFlag::kEmpty));
  test_api_->OpenTheMainMenu();
  LeftClickOn(test_api_->GetMainMenuGameControlsDetailsButton());
  VerifyGameControlsEditControlsWithEmptyStateHistogram(
      histograms, std::vector<int>{1, /*is_setup=*/1});
  VerifyGameControlsEditControlsWithEmptyStateLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/2u, /*expect_histograms_value=*/1);
}

TEST_F(GameDashboardContextTest, CompatModeArcGame) {
  // Create an ARC game window that supports Compat Mode.
  CreateGameWindow(/*is_arc_window=*/true);
  game_window_->SetProperty(kArcResizeLockTypeKey,
                            ArcResizeLockType::RESIZE_ENABLED_TOGGLABLE);

  test_api_->OpenTheMainMenu();

  const auto* screen_size_button =
      test_api_->GetMainMenuScreenSizeSettingsButton();
  ASSERT_TRUE(screen_size_button);
  EXPECT_TRUE(screen_size_button->GetEnabled());
}

TEST_F(GameDashboardContextTest, NonCompatModeArcGame) {
  // Create an ARC game window that doesn't support Compat Mode.
  CreateGameWindow(/*is_arc_window=*/true);
  game_window_->SetProperty(kArcResizeLockTypeKey,
                            ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE);

  test_api_->OpenTheMainMenu();

  const auto* screen_size_button =
      test_api_->GetMainMenuScreenSizeSettingsButton();
  ASSERT_TRUE(screen_size_button);
  EXPECT_FALSE(screen_size_button->GetEnabled());
  EXPECT_EQ(u"This app supports only this size.",
            screen_size_button->GetTooltipText());
}

// Verifies the Main Menu View closes when the Screen Size row is selected.
TEST_F(GameDashboardContextTest, SelectScreenSizeButton) {
  // Create an ARC game window.
  CreateGameWindow(/*is_arc_window=*/true);
  game_window_->SetProperty(kArcResizeLockTypeKey,
                            ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE);

  test_api_->OpenTheMainMenu();

  const auto* screen_size_button =
      test_api_->GetMainMenuScreenSizeSettingsButton();
  ASSERT_TRUE(screen_size_button);
  ASSERT_TRUE(screen_size_button->GetEnabled());

  LeftClickOn(screen_size_button);

  EXPECT_FALSE(test_api_->GetMainMenuWidget());
}

// Verifies that when one game window starts a recording session, it's
// record game buttons are enabled and the other game's record game buttons
// are disabled.
TEST_F(GameDashboardContextTest, TwoGameWindowsRecordingState) {
  // Create an ARC game window.
  CreateGameWindow(/*is_arc_window=*/true);
  // Create a GFN game window.
  auto gfn_game_window =
      CreateAppWindow(extension_misc::kGeForceNowAppId, AppType::NON_APP,
                      gfx::Rect(50, 50, 400, 200));
  auto* gfn_game_context =
      GameDashboardController::Get()->GetGameDashboardContext(
          gfn_game_window.get());
  ASSERT_TRUE(gfn_game_context);
  auto gfn_window_test_api =
      GameDashboardContextTestApi(gfn_game_context, GetEventGenerator());

  // Start recording the ARC game window, and verify both windows' record game
  // button states.
  RecordGameAndVerifyButtons(
      /*recording_window_test_api=*/test_api_.get(),
      /*other_window_test_api=*/&gfn_window_test_api);

  // Start recording the GFN game window, and verify both windows' "record
  // game" button states.
  RecordGameAndVerifyButtons(
      /*recording_window_test_api=*/&gfn_window_test_api,
      /*other_window_test_api=*/test_api_.get());
}

TEST_F(GameDashboardContextTest, RecordingTimerStringFormat) {
  // Create an ARC game window.
  CreateGameWindow(/*is_arc_window=*/true);

  // Verify recording duration is 0, by default.
  EXPECT_EQ(u"00:00", test_api_->GetRecordingDuration());

  // Start recording the game window.
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  const auto* record_game_button = test_api_->GetToolbarRecordGameButton();
  ASSERT_TRUE(record_game_button);
  LeftClickOn(record_game_button);
  ClickOnStartRecordingButtonInCaptureModeBarView();

  // Get timer and verify it's running.
  const auto& timer = test_api_->GetRecordingTimer();
  EXPECT_TRUE(timer.IsRunning());

  // Verify initial time of 0 seconds.
  EXPECT_EQ(u"00:00", test_api_->GetRecordingDuration());

  // Advance clock by 1 minute, and verify overflow from seconds to minutes.
  AdvanceClock(base::Minutes(1));
  EXPECT_EQ(u"01:00", test_api_->GetRecordingDuration());

  // Advance clock by 30 seconds.
  AdvanceClock(base::Seconds(30));
  EXPECT_EQ(u"01:30", test_api_->GetRecordingDuration());

  // Advance clock by 50 minutes.
  AdvanceClock(base::Minutes(50));
  EXPECT_EQ(u"51:30", test_api_->GetRecordingDuration());

  // Advance clock by 9 minutes, and verify overflow from minutes to hours.
  AdvanceClock(base::Minutes(9));
  EXPECT_EQ(u"1:00:30", test_api_->GetRecordingDuration());

  // Advance clock by 23 hours, and verify hours doesn't overflow to days.
  AdvanceClock(base::Hours(23));
  EXPECT_EQ(u"24:00:30", test_api_->GetRecordingDuration());

  // Stop the recording.
  LeftClickOn(record_game_button);

  // Verify recording duration is reset to 0.
  EXPECT_EQ(u"00:00", test_api_->GetRecordingDuration());
}

// Verifies the welcome dialog displays when the game window first opens and
// disappears after 4 seconds.
TEST_F(GameDashboardContextTest, WelcomeDialogAutoDismisses) {
  // Open the game window with the welcome dialog enabled.
  SetShowWelcomeDialog(true);
  CreateGameWindow(/*is_arc_window=*/true,
                   /*set_arc_game_controls_flags_prop=*/true);

  // Verify the welcome dialog is initially shown and is right aligned in the
  // app window.
  gfx::Rect welcome_dialog_bounds =
      test_api_->GetWelcomeDialogWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(welcome_dialog_bounds.x(),
            (game_window_->GetBoundsInScreen().right() -
             game_dashboard::kWelcomeDialogEdgePadding -
             game_dashboard::kWelcomeDialogFixedWidth));

  // Dismiss welcome dialog after 4 seconds and verify the dialog is no longer
  // visible.
  task_environment()->FastForwardBy(base::Seconds(4));
  EXPECT_FALSE(test_api_->GetWelcomeDialogWidget());
}

// Verifies the welcome dialog disappears when the main menu view is opened.
TEST_F(GameDashboardContextTest, WelcomeDialogDismissOnMainMenuOpening) {
  // Open the game window with the welcome dialog enabled.
  SetShowWelcomeDialog(true);
  CreateGameWindow(/*is_arc_window=*/true,
                   /*set_arc_game_controls_flags_prop=*/true);

  // Open the main menu and verify the welcome dialog dismisses.
  test_api_->OpenTheMainMenu();
  EXPECT_FALSE(test_api_->GetWelcomeDialogWidget());
}

// Verifies the welcome dialog is centered when the app window width is small
// enough.
TEST_F(GameDashboardContextTest, WelcomeDialogWithSmallWindow) {
  // Open a new game window with a width of 450.
  SetShowWelcomeDialog(true);
  SetAppBounds(gfx::Rect(50, 50, 450, 400));
  CreateGameWindow(/*is_arc_window=*/true,
                   /*set_arc_game_controls_flags_prop=*/true);

  // Verify the welcome dialog is centered.
  gfx::Rect welcome_dialog_bounds =
      test_api_->GetWelcomeDialogWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(welcome_dialog_bounds.x(),
            (game_window_->GetBoundsInScreen().x() +
             (game_window_->GetBoundsInScreen().width() -
              game_dashboard::kWelcomeDialogFixedWidth) /
                 2));
}

TEST_F(GameDashboardContextTest, MainMenuCursorHandlerEventLocation) {
  // Create an ARC game window.
  SetAppBounds(gfx::Rect(50, 50, 800, 700));
  CreateGameWindow(/*is_arc_window=*/true,
                   /*set_arc_game_controls_flags_prop=*/true);

  auto* event_generator = GetEventGenerator();
  auto* cursor_manager = Shell::Get()->cursor_manager();

  // Move the mouse to the center of the window and verify the cursor is
  // visible.
  event_generator->MoveMouseToCenterOf(game_window_.get());
  ASSERT_TRUE(cursor_manager->IsCursorVisible());

  // Hide the cursor and verify it's hidden.
  cursor_manager->HideCursor();
  ASSERT_FALSE(cursor_manager->IsCursorVisible());

  // Open the main menu and verify `GameDashboardMainMenuCursorHandler` exists
  // and the cursor is visible.
  ASSERT_FALSE(test_api_->GetMainMenuCursorHandler());
  test_api_->OpenTheMainMenu();
  ASSERT_TRUE(test_api_->GetMainMenuCursorHandler());
  ASSERT_TRUE(cursor_manager->IsCursorVisible());

  // Move the cursor inside the window frame header, half way between the left
  // edge of the window and `GameDashboardMainMenuButton`.
  const auto window_bounds = game_window_->GetBoundsInScreen();
  const auto gd_button_bounds_x =
      test_api_->GetGameDashboardButton()->GetBoundsInScreen().x();
  gfx::Point new_mouse_location =
      gfx::Point((window_bounds.x() + gd_button_bounds_x) / 2,
                 window_bounds.y() + frame_header_height_ / 2);
  event_generator->MoveMouseTo(new_mouse_location);

  // Verify the mouse event was not consumed by
  // `GameDashboardMainMenuCursorHandler`.
  auto* last_mouse_event = post_target_event_capturer_.last_mouse_event();
  ASSERT_TRUE(last_mouse_event);
  ASSERT_FALSE(last_mouse_event->handled());
  ASSERT_FALSE(last_mouse_event->stopped_propagation());

  // Move the mouse to the enter of the window, and below the main menu.
  new_mouse_location.set_x(window_bounds.CenterPoint().x());
  const auto main_menu_bounds =
      test_api_->GetMainMenuView()->GetBoundsInScreen();
  new_mouse_location.set_y(main_menu_bounds.y() + main_menu_bounds.height() +
                           50);

  // Verify the mouse event was consumed by
  // `GameDashboardMainMenuCursorHandler`.
  post_target_event_capturer_.Reset();
  event_generator->MoveMouseTo(new_mouse_location);
  ASSERT_FALSE(post_target_event_capturer_.last_mouse_event());
}

TEST_F(GameDashboardContextTest, GameDashboardButtonFullscreen) {
  // Create an ARC game window.
  SetAppBounds(gfx::Rect(50, 50, 800, 700));
  CreateGameWindow(/*is_arc_window=*/true,
                   /*set_arc_game_controls_flags_prop=*/true);

  AcceleratorControllerImpl* controller =
      Shell::Get()->accelerator_controller();
  ui::Accelerator gd_accelerator(ui::VKEY_G, ui::EF_COMMAND_DOWN);
  auto* window_state = WindowState::Get(game_window_.get());
  auto* button_widget = test_api_->GetGameDashboardButtonWidget();
  CHECK(button_widget);

  // Initial state.
  ASSERT_FALSE(window_state->IsFullscreen());
  ASSERT_TRUE(button_widget->IsVisible());

  // Switch to fullscreen and verify Game Dashboard button widget is visible.
  ToggleFullScreen(window_state, /*delegate=*/nullptr);
  ASSERT_TRUE(window_state->IsFullscreen());
  ASSERT_FALSE(button_widget->IsVisible());

  // Open the Game Dashboard menu with the accelerator and verify the game
  // dashboard button widget is visible.
  ASSERT_TRUE(controller->Process(gd_accelerator));
  ASSERT_TRUE(button_widget->IsVisible());

  // Close the Game Dashboard menu with the accelerator and verify the game
  // dashboard button widget is still visible.
  ASSERT_TRUE(controller->Process(gd_accelerator));
  ASSERT_TRUE(button_widget->IsVisible());

  // Move the mouse to the center of the game window and verify the game
  // dashboard button widget is not visible.
  GetEventGenerator()->MoveMouseTo(
      game_window_->GetBoundsInScreen().CenterPoint());
  ASSERT_FALSE(button_widget->IsVisible());

  // Exit fullscreen and verify Game Dashboard button widget is visible.
  ToggleFullScreen(window_state, /*delegate=*/nullptr);
  ASSERT_FALSE(window_state->IsFullscreen());
  ASSERT_TRUE(button_widget->IsVisible());
}

TEST_F(GameDashboardContextTest, GameDashboardButtonFullscreenWithMainMenu) {
  // Create an ARC game window.
  SetAppBounds(gfx::Rect(50, 50, 800, 700));
  CreateGameWindow(/*is_arc_window=*/true,
                   /*set_arc_game_controls_flags_prop=*/true);

  AcceleratorControllerImpl* controller =
      Shell::Get()->accelerator_controller();
  ui::Accelerator gd_accelerator(ui::VKEY_G, ui::EF_COMMAND_DOWN);
  auto* window_state = WindowState::Get(game_window_.get());
  auto* button_widget = test_api_->GetGameDashboardButtonWidget();
  CHECK(button_widget);

  // Initial state.
  ASSERT_FALSE(window_state->IsFullscreen());
  ASSERT_TRUE(button_widget->IsVisible());
  GetEventGenerator()->MoveMouseTo(
      game_window_->GetBoundsInScreen().CenterPoint());

  // Open the main menu using the accelerator
  ASSERT_TRUE(controller->Process(gd_accelerator));

  // Switch to fullscreen and verify Game Dashboard button widget is visible.
  ToggleFullScreen(window_state, /*delegate=*/nullptr);
  ASSERT_TRUE(window_state->IsFullscreen());
  ASSERT_TRUE(button_widget->IsVisible());

  // Close the main menu using the accelerator and verify the Game Dashboard
  // button widget is visible.
  ASSERT_TRUE(controller->Process(gd_accelerator));
  ASSERT_TRUE(button_widget->IsVisible());

  // Move the mouse slightly and verify the Game Dashboard button widget is not
  // visible.
  GetEventGenerator()->MoveMouseBy(/*x=*/1, /*y=*/1);
  ASSERT_FALSE(button_widget->IsVisible());
}

TEST_F(GameDashboardContextTest, GameDashboardButtonFullscreen_MouseOver) {
  // Create an ARC game window.
  SetAppBounds(gfx::Rect(50, 50, 800, 700));
  CreateGameWindow(/*is_arc_window=*/true,
                   /*set_arc_game_controls_flags_prop=*/true);

  auto* event_generator = GetEventGenerator();
  auto app_bounds = game_window_->GetBoundsInScreen();
  auto* window_state = WindowState::Get(game_window_.get());
  ASSERT_TRUE(window_state->IsNormalStateType());
  views::Widget* button_widget = test_api_->GetGameDashboardButtonWidget();
  CHECK(button_widget);

  // Set initial state to fullscreen and verify Game Dashboard button widget is
  // not visible.
  ASSERT_FALSE(test_api_->GetGameDashboardButtonRevealController());
  ToggleFullScreen(window_state, /*delegate=*/nullptr);
  ASSERT_TRUE(window_state->IsFullscreen());
  ASSERT_FALSE(button_widget->IsVisible());
  ASSERT_TRUE(test_api_->GetGameDashboardButtonRevealController());
  ASSERT_FALSE(test_api_->GetGameDashboardButtonWidget()->IsVisible());

  // Move mouse to top edge of window.
  event_generator->MoveMouseTo(app_bounds.top_center());
  base::OneShotTimer& top_edge_hover_timer =
      test_api_->GetRevealControllerTopEdgeHoverTimer();
  ASSERT_TRUE(top_edge_hover_timer.IsRunning());
  top_edge_hover_timer.FireNow();
  ASSERT_TRUE(button_widget->IsVisible());
  ASSERT_TRUE(test_api_->GetGameDashboardButtonWidget()->IsVisible());

  // Move mouse to the center of the app, and verify Game Dashboard button
  // widget is not visible.
  event_generator->MoveMouseTo(app_bounds.CenterPoint());
  ASSERT_FALSE(test_api_->GetGameDashboardButtonWidget()->IsVisible());
  ASSERT_FALSE(button_widget->IsVisible());
}

// -----------------------------------------------------------------------------
// GameTypeGameDashboardContextTest:
// Test fixture to test both ARC and GeForceNow game window depending on the
// test param (true for ARC game window, false for GeForceNow window).
class GameTypeGameDashboardContextTest
    : public GameDashboardContextTest,
      public testing::WithParamInterface<bool> {
 public:
  GameTypeGameDashboardContextTest() = default;
  ~GameTypeGameDashboardContextTest() override = default;

  // GameDashboardContextTest:
  void SetUp() override {
    GameDashboardContextTest::SetUp();
    CreateGameWindow(IsArcGame());
  }

 protected:
  bool IsArcGame() const { return GetParam(); }

  void VerifyFeaturesEnabled(bool expect_enabled,
                             bool toolbar_visible = false) {
    auto* event_generator = GetEventGenerator();
    auto* gd_button_widget = test_api_->GetGameDashboardButtonWidget();
    EXPECT_TRUE(gd_button_widget);

    if (expect_enabled) {
      EXPECT_TRUE(gd_button_widget->IsVisible());
      event_generator->PressAndReleaseKey(ui::VKEY_G, ui::EF_COMMAND_DOWN);
      EXPECT_TRUE(test_api_->GetMainMenuWidget());
      test_api_->CloseTheMainMenu();
    } else {
      EXPECT_FALSE(gd_button_widget->IsVisible());
      event_generator->PressAndReleaseKey(ui::VKEY_G, ui::EF_COMMAND_DOWN);
      EXPECT_FALSE(test_api_->GetMainMenuWidget());
    }
    auto* toolbar_widget = test_api_->GetToolbarWidget();
    if (toolbar_visible) {
      EXPECT_TRUE(toolbar_widget);
      EXPECT_TRUE(toolbar_widget->IsVisible());
    } else {
      EXPECT_TRUE(!toolbar_widget || !toolbar_widget->IsVisible());
    }
  }
};

// GameTypeGameDashboardContextTest Tests
// -----------------------------------------------------------------------
// Verifies the initial location of the Game Dashboard button widget relative to
// the game window.
TEST_P(GameTypeGameDashboardContextTest,
       GameDashboardButtonWidget_InitialLocation) {
  const gfx::Point expected_button_center_point(
      game_window_->GetBoundsInScreen().top_center().x(),
      app_bounds().y() + frame_header_height_ / 2);
  EXPECT_EQ(expected_button_center_point,
            test_api_->GetGameDashboardButtonWidget()
                ->GetNativeWindow()
                ->GetBoundsInScreen()
                .CenterPoint());
}

// Verifies the Game Dashboard button widget bounds are updated, relative to the
// game window.
TEST_P(GameTypeGameDashboardContextTest,
       GameDashboardButtonWidget_MoveWindowAndVerifyLocation) {
  const auto move_vector = gfx::Vector2d(100, 200);
  const auto* native_window =
      test_api_->GetGameDashboardButtonWidget()->GetNativeWindow();
  const auto expected_widget_location =
      native_window->GetBoundsInScreen() + move_vector;

  game_window_->SetBoundsInScreen(
      game_window_->GetBoundsInScreen() + move_vector, GetPrimaryDisplay());

  EXPECT_EQ(expected_widget_location, native_window->GetBoundsInScreen());
}

// Verifies clicking the Game Dashboard button will open the main menu widget.
TEST_P(GameTypeGameDashboardContextTest, OpenGameDashboardButtonWidget) {
  // Close the window and create a new game window without setting the
  // `kArcGameControlsFlagsKey` property.
  CloseGameWindow();
  CreateGameWindow(IsArcGame(), /*set_arc_game_controls_flags_prop=*/false);

  // Verifies the main menu is closed.
  EXPECT_FALSE(test_api_->GetMainMenuWidget());

  if (IsArcGame()) {
    // Game Dashboard button is not enabled util the Game Controls state is
    // known.
    EXPECT_FALSE(test_api_->GetGameDashboardButton()->GetEnabled());
    LeftClickOn(test_api_->GetGameDashboardButton());
    EXPECT_FALSE(test_api_->GetMainMenuWidget());
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }

  // Open the main menu dialog and verify the main menu is open.
  test_api_->OpenTheMainMenu();
}

// Verifies Game Controls UIs only show up on the ARC games.
TEST_P(GameTypeGameDashboardContextTest, GameControlsUiExistence) {
  const bool is_arc_game = IsArcGame();
  if (is_arc_game) {
    // The ARC game has Game Controls optout in this test.
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }

  OpenMenuCheckGameControlsUIState(
      /*hint_tile_states=*/
      {/*expect_exists=*/is_arc_game, /*expect_enabled=*/false,
       /*expect_on=*/false},
      /*details_row_exists=*/
      {/*expect_exists=*/is_arc_game, /*expect_enabled=*/false},
      /*feature_switch_states=*/
      {/*expect_exists=*/false, /*expect_toggled=*/false},
      /*setup_exists=*/is_arc_game);
}

// Verifies clicking the Game Dashboard button will close the main menu widget
// if it's already open.
TEST_P(GameTypeGameDashboardContextTest, CloseGameDashboardButtonWidget) {
  // Open the main menu widget and verify the main menu open.
  test_api_->OpenTheMainMenu();

  // Close the main menu dialog and verify the main menu is closed.
  test_api_->CloseTheMainMenu();
}

// Verifies clicking outside the main menu view will close the main menu
// widget. Then, clicking on the main menu button will still toggle the main
// menu widget visibility.
TEST_P(GameTypeGameDashboardContextTest, CloseMainMenuOutsideButtonWidget) {
  // Open the main menu widget and verify the main menu open.
  test_api_->OpenTheMainMenu();

  // Close the main menu dialog by clicking outside the main menu view bounds.
  auto* event_generator = GetEventGenerator();
  gfx::Rect game_bounds = app_bounds();
  const gfx::Point& new_location = {game_bounds.x() + game_bounds.width(),
                                    game_bounds.y() + game_bounds.height()};
  event_generator->set_current_screen_location(new_location);
  event_generator->ClickLeftButton();

  // Clicking outside the main menu causes the main menu to close
  // asynchronously. Run until idle to ensure that this posted task runs
  // synchronously and completes before proceeding.
  base::RunLoop().RunUntilIdle();

  // Open the main menu widget via the main menu button.
  test_api_->OpenTheMainMenu();

  // Close the main menu widget via the main menu button.
  test_api_->CloseTheMainMenu();
}

// Verifies the main menu shows all items allowed.
TEST_P(GameTypeGameDashboardContextTest,
       MainMenuDialogWidget_AvailableFeatures) {
  if (IsArcGame()) {
    game_window_->SetProperty(
        kArcGameControlsFlagsKey,
        static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                         ArcGameControlsFlag::kAvailable));
  }

  test_api_->OpenTheMainMenu();

  // Verify whether each element available in the main menu is available as
  // expected.
  EXPECT_TRUE(test_api_->GetMainMenuToolbarTile());
  EXPECT_TRUE(test_api_->GetMainMenuRecordGameTile());
  EXPECT_TRUE(test_api_->GetMainMenuScreenshotTile());
  EXPECT_TRUE(test_api_->GetMainMenuFeedbackButton());
  EXPECT_TRUE(test_api_->GetMainMenuHelpButton());
  EXPECT_TRUE(test_api_->GetMainMenuSettingsButton());
  if (IsArcGame()) {
    EXPECT_TRUE(test_api_->GetMainMenuGameControlsTile());
    EXPECT_TRUE(test_api_->GetMainMenuScreenSizeSettingsButton());
  } else {
    EXPECT_FALSE(test_api_->GetMainMenuGameControlsTile());
    EXPECT_FALSE(test_api_->GetMainMenuScreenSizeSettingsButton());
  }
}

// Verifies the main menu doesn't show the record game tile, when the feature is
// disabled.
TEST_P(GameTypeGameDashboardContextTest,
       MainMenuDialogWidget_RecordGameDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      {features::kFeatureManagementGameDashboardRecordGame});

  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();

  // Verify that the record game tile is unavailable in the main menu.
  EXPECT_FALSE(test_api_->GetMainMenuRecordGameTile());
  // Verify that the record game button is unavailable in the toolbar.
  EXPECT_FALSE(test_api_->GetToolbarRecordGameButton());
}

// Verifies the main menu screenshot tile will take a screenshot of the game
// window.
TEST_P(GameTypeGameDashboardContextTest, TakeScreenshotFromMainMenu) {
  test_api_->OpenTheMainMenu();

  // Retrieve the screenshot button and verify the initial state.
  const auto* screenshot_tile = test_api_->GetMainMenuScreenshotTile();
  ASSERT_TRUE(screenshot_tile);

  LeftClickOn(screenshot_tile);

  // Verify that a screenshot is taken of the game window.
  const auto file_path = WaitForCaptureFileToBeSaved();
  const auto image = ReadAndDecodeImageFile(file_path);
  EXPECT_EQ(image.Size(), game_window_->bounds().size());
}

// Verifies the record game buttons in the main menu and toolbar are disabled,
// if a recording session was started outside of the Game Dashboard.
TEST_P(GameTypeGameDashboardContextTest,
       CaptureSessionStartedOutsideOfTheGameDashboard) {
  test_api_->OpenTheMainMenu();

  // Verify the game dashboard button is initially not in the recording state.
  VerifyGameDashboardButtonState(/*is_recording=*/false);

  // Retrieve the record game tile from the main menu, and verify it's
  // enabled and toggled off.
  const auto* main_menu_record_game_button =
      test_api_->GetMainMenuRecordGameTile();
  EXPECT_TRUE(main_menu_record_game_button);
  EXPECT_TRUE(main_menu_record_game_button->GetEnabled());
  EXPECT_FALSE(main_menu_record_game_button->IsToggled());

  test_api_->OpenTheToolbar();
  // Retrieve the record game button from the toolbar, and verify it's
  // enabled and toggled off.
  const auto* toolbar_record_game_button =
      test_api_->GetToolbarRecordGameButton();
  EXPECT_TRUE(toolbar_record_game_button);
  EXPECT_TRUE(toolbar_record_game_button->GetEnabled());
  EXPECT_FALSE(toolbar_record_game_button->toggled());

  const auto* capture_mode_controller = CaptureModeController::Get();
  // Start video recording from `CaptureModeController`.
  EXPECT_FALSE(capture_mode_controller->is_recording_in_progress());
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(capture_mode_controller->is_recording_in_progress());

  // Verify the record game buttons are disabled and toggled off.
  EXPECT_FALSE(main_menu_record_game_button->GetEnabled());
  EXPECT_FALSE(main_menu_record_game_button->IsToggled());
  EXPECT_FALSE(toolbar_record_game_button->GetEnabled());
  EXPECT_FALSE(toolbar_record_game_button->toggled());

  // Verify the game dashboard button is not in the recording state.
  VerifyGameDashboardButtonState(/*is_recording=*/false);

  // Stop video recording.
  CaptureModeTestApi().StopVideoRecording();
  EXPECT_FALSE(capture_mode_controller->is_recording_in_progress());

  // Verify the record game buttons are not enabled until the video file is
  // finalized.
  EXPECT_FALSE(capture_mode_controller->can_start_new_recording());
  EXPECT_FALSE(main_menu_record_game_button->GetEnabled());
  EXPECT_FALSE(main_menu_record_game_button->IsToggled());
  EXPECT_FALSE(toolbar_record_game_button->GetEnabled());
  EXPECT_FALSE(toolbar_record_game_button->toggled());
  WaitForCaptureFileToBeSaved();
  EXPECT_TRUE(capture_mode_controller->can_start_new_recording());
  EXPECT_TRUE(main_menu_record_game_button->GetEnabled());
  EXPECT_FALSE(main_menu_record_game_button->IsToggled());
  EXPECT_TRUE(toolbar_record_game_button->GetEnabled());
  EXPECT_FALSE(toolbar_record_game_button->toggled());

  // Verify the game dashboard button is still in not in the recording state.
  VerifyGameDashboardButtonState(/*is_recording=*/false);
}

// Verifies the toolbar opens and closes when the toolbar button in the main
// menu is clicked.
TEST_P(GameTypeGameDashboardContextTest, OpenAndCloseToolbarWidget) {
  if (IsArcGame()) {
    game_window_->SetProperty(
        kArcGameControlsFlagsKey,
        static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                         ArcGameControlsFlag::kAvailable));
  }

  test_api_->OpenTheMainMenu();

  // Retrieve the toolbar button and verify the toolbar widget is not enabled.
  auto* toolbar_tile = test_api_->GetMainMenuToolbarTile();
  ASSERT_TRUE(toolbar_tile);
  EXPECT_FALSE(toolbar_tile->IsToggled());
  EXPECT_EQ(toolbar_tile->sub_label()->GetText(), hidden_label);

  // Open the toolbar, verify the main menu toolbar tile's sub-label is updated,
  // and verify available feature buttons.
  test_api_->OpenTheToolbar();
  EXPECT_EQ(toolbar_tile->sub_label()->GetText(), visible_label);
  EXPECT_TRUE(test_api_->GetToolbarGamepadButton());
  EXPECT_TRUE(test_api_->GetToolbarRecordGameButton());
  EXPECT_TRUE(test_api_->GetToolbarScreenshotButton());
  if (IsArcGame()) {
    EXPECT_TRUE(test_api_->GetToolbarGameControlsButton());
  } else {
    EXPECT_FALSE(test_api_->GetToolbarGameControlsButton());
  }

  // Verify toggling the main menu visibility doesn't affect the toolbar.
  test_api_->CloseTheMainMenu();
  EXPECT_TRUE(test_api_->GetToolbarWidget());
  test_api_->OpenTheMainMenu();
  toolbar_tile = test_api_->GetMainMenuToolbarTile();
  EXPECT_EQ(toolbar_tile->sub_label()->GetText(), visible_label);
  EXPECT_TRUE(test_api_->GetToolbarWidget());

  test_api_->CloseTheToolbar();

  // Verify that the toolbar widget is no longer available and is toggled off.
  EXPECT_FALSE(test_api_->GetToolbarWidget());
  EXPECT_FALSE(toolbar_tile->IsToggled());
  EXPECT_EQ(toolbar_tile->sub_label()->GetText(), hidden_label);
}

// Verifies the toolbar screenshot button will take a screenshot of the game
// window.
TEST_P(GameTypeGameDashboardContextTest, TakeScreenshotFromToolbar) {
  // Open the toolbar via the main menu.
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();

  // Click on the screenshot button within the toolbar.
  const auto* screenshot_button = test_api_->GetToolbarScreenshotButton();
  ASSERT_TRUE(screenshot_button);
  LeftClickOn(screenshot_button);

  // Verify that a screenshot is taken of the game window.
  const auto file_path = WaitForCaptureFileToBeSaved();
  const auto image = ReadAndDecodeImageFile(file_path);
  EXPECT_EQ(image.Size(), game_window_->GetBoundsInScreen().size());
}

// Verifies clicking the toolbar's gamepad button will expand and collapse the
// toolbar.
TEST_P(GameTypeGameDashboardContextTest, CollapseAndExpandToolbarWidget) {
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  const int initial_height = GetToolbarHeight();
  EXPECT_NE(initial_height, 0);

  // Click on the gamepad button within the toolbar.
  auto* gamepad_button = test_api_->GetToolbarGamepadButton();
  ASSERT_TRUE(gamepad_button);
  LeftClickOn(gamepad_button);
  int updated_height = GetToolbarHeight();

  // Verify that the initial y coordinate of the toolbar was larger than the
  // updated y value.
  EXPECT_GT(initial_height, updated_height);

  // Click on the gamepad button within the toolbar again.
  LeftClickOn(gamepad_button);
  updated_height = GetToolbarHeight();

  // Verify that the toolbar is back to its initially expanded height.
  EXPECT_EQ(initial_height, updated_height);
}

// Verifies the toolbar won't follow the mouse cursor outside of the game window
// bounds.
TEST_P(GameTypeGameDashboardContextTest, MoveToolbarOutOfBounds) {
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  ASSERT_TRUE(test_api_->GetToolbarWidget());
  ASSERT_EQ(test_api_->GetToolbarSnapLocation(),
            ToolbarSnapLocation::kTopRight);

  auto window_bounds = game_window_->GetBoundsInScreen();
  const int screen_point_x = kScreenBounds.x();
  const int screen_point_right = screen_point_x + kScreenBounds.width();
  const int screen_point_y = kScreenBounds.y();
  const int screen_point_bottom = screen_point_y + kScreenBounds.height();

  // Verify the screen bounds are larger than the game bounds.
  auto game_bounds = app_bounds();
  ASSERT_LT(screen_point_x, game_bounds.x());
  ASSERT_LT(screen_point_y, game_bounds.y());
  ASSERT_GT(screen_point_right, game_bounds.x() + game_bounds.width());
  ASSERT_GT(screen_point_bottom, game_bounds.y() + game_bounds.height());

  // Drag toolbar, moving the mouse past the game window to the top right corner
  // of the screen bounds, and verify the toolbar doesn't go past the game
  // window.
  DragToolbarToPoint(Movement::kMouse, {screen_point_right, screen_point_y},
                     false);
  const auto* native_window = test_api_->GetToolbarWidget()->GetNativeWindow();
  auto toolbar_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.right(), window_bounds.right());
  EXPECT_EQ(toolbar_bounds.y(), window_bounds.y());

  // Drag toolbar, moving the mouse past the game window to the top left corner
  // of the screen bounds.
  DragToolbarToPoint(Movement::kMouse, {screen_point_x, screen_point_y}, false);
  toolbar_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.x(), window_bounds.x());
  EXPECT_EQ(toolbar_bounds.y(), window_bounds.y());

  // Drag toolbar, moving the mouse past the game window to the bottom left
  // corner of the screen bounds.
  DragToolbarToPoint(Movement::kMouse, {screen_point_x, screen_point_bottom},
                     false);
  toolbar_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.x(), window_bounds.x());
  EXPECT_EQ(toolbar_bounds.bottom(), window_bounds.bottom());

  // Drag toolbar, moving the mouse past the game window to the bottom right
  // corner of the screen bounds.
  DragToolbarToPoint(Movement::kMouse,
                     {screen_point_right, screen_point_bottom}, false);
  toolbar_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.right(), window_bounds.right());
  EXPECT_EQ(toolbar_bounds.bottom(), window_bounds.bottom());

  GetEventGenerator()->ReleaseLeftButton();
}

// Verifies the toolbar can be moved around via the mouse.
TEST_P(GameTypeGameDashboardContextTest, MoveToolbarWidgetViaMouse) {
  VerifyToolbarDrag(Movement::kMouse);
}

// Verifies the toolbar can be moved around via touch.
TEST_P(GameTypeGameDashboardContextTest, MoveToolbarWidgetViaTouch) {
  VerifyToolbarDrag(Movement::kTouch);
}

// Verifies the toolbar can be moved around via keyboard arrows.
TEST_P(GameTypeGameDashboardContextTest, MoveToolbarWidgetViaArrowKeys) {
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  test_api_->SetFocusOnToolbar();

  // Verify that be default the snap position should be `kTopRight` and
  // toolbar is placed in the top right quadrant.
  EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
            ToolbarSnapLocation::kTopRight);

  // Press tab so the toolbar gains focus
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_TAB);

  // Press right arrow key and verify toolbar does not leave top right quadrant.
  PressKeyAndVerify(ui::VKEY_RIGHT, ToolbarSnapLocation::kTopRight);

  // Press left arrow key and verify toolbar moved to top left quadrant.
  PressKeyAndVerify(ui::VKEY_LEFT, ToolbarSnapLocation::kTopLeft);

  // Press down arrow key and verify toolbar moved to bottom left quadrant.
  PressKeyAndVerify(ui::VKEY_DOWN, ToolbarSnapLocation::kBottomLeft);

  // Press right arrow key and verify toolbar moved to bottom right quadrant.
  PressKeyAndVerify(ui::VKEY_RIGHT, ToolbarSnapLocation::kBottomRight);

  // Press up arrow key and verify toolbar moved to top right quadrant.
  PressKeyAndVerify(ui::VKEY_UP, ToolbarSnapLocation::kTopRight);

  // Press up arrow key again and verify toolbar does not leave top right
  // quadrant.
  PressKeyAndVerify(ui::VKEY_UP, ToolbarSnapLocation::kTopRight);

  // Press down arrow key and verify toolbar moved to bottom right quadrant.
  PressKeyAndVerify(ui::VKEY_DOWN, ToolbarSnapLocation::kBottomRight);

  // Press down arrow key again and verify toolbar does not leave bottom right
  // quadrant.
  PressKeyAndVerify(ui::VKEY_DOWN, ToolbarSnapLocation::kBottomRight);

  // Press left arrow key and verify toolbar moved to bottom left quadrant.
  PressKeyAndVerify(ui::VKEY_LEFT, ToolbarSnapLocation::kBottomLeft);

  // Press up arrow key and verify toolbar moved to top left quadrant.
  PressKeyAndVerify(ui::VKEY_UP, ToolbarSnapLocation::kTopLeft);

  // Press right arrow key and verify toolbar moved to top right quadrant.
  PressKeyAndVerify(ui::VKEY_RIGHT, ToolbarSnapLocation::kTopRight);
}

// Verifies the toolbar's physical placement on screen in each quadrant.
TEST_P(GameTypeGameDashboardContextTest, VerifyToolbarPlacementInQuadrants) {
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  const auto window_bounds = game_window_->GetBoundsInScreen();
  const auto window_center_point = window_bounds.CenterPoint();
  const int x_offset = window_bounds.width() / 4;
  const int y_offset = window_bounds.height() / 4;

  // Verify initial placement in top right quadrant.
  auto game_bounds = app_bounds();
  const auto* native_window = test_api_->GetToolbarWidget()->GetNativeWindow();
  auto toolbar_bounds = native_window->GetBoundsInScreen();
  const auto toolbar_size =
      test_api_->GetToolbarWidget()->GetContentsView()->GetPreferredSize();
  EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
            ToolbarSnapLocation::kTopRight);
  EXPECT_EQ(toolbar_bounds.x(), game_bounds.right() -
                                    game_dashboard::kToolbarEdgePadding -
                                    toolbar_size.width());
  EXPECT_EQ(toolbar_bounds.y(), game_bounds.y() +
                                    game_dashboard::kToolbarEdgePadding +
                                    frame_header_height_);

  // Move toolbar to top left quadrant and verify toolbar placement.
  DragToolbarToPoint(Movement::kMouse, {window_center_point.x() - x_offset,
                                        window_center_point.y() - y_offset});
  EXPECT_EQ(test_api_->GetToolbarSnapLocation(), ToolbarSnapLocation::kTopLeft);
  toolbar_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.x(),
            game_bounds.x() + game_dashboard::kToolbarEdgePadding);
  EXPECT_EQ(toolbar_bounds.y(), game_bounds.y() +
                                    game_dashboard::kToolbarEdgePadding +
                                    frame_header_height_);

  // Move toolbar to bottom right quadrant and verify toolbar placement.
  DragToolbarToPoint(Movement::kMouse, {window_center_point.x() + x_offset,
                                        window_center_point.y() + y_offset});
  toolbar_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.x(), game_bounds.right() -
                                    game_dashboard::kToolbarEdgePadding -
                                    toolbar_size.width());
  EXPECT_EQ(toolbar_bounds.y(), game_bounds.bottom() -
                                    game_dashboard::kToolbarEdgePadding -
                                    toolbar_size.height());

  // Move toolbar to bottom left quadrant and verify toolbar placement.
  DragToolbarToPoint(Movement::kMouse, {window_center_point.x() - x_offset,
                                        window_center_point.y() + y_offset});
  toolbar_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.x(),
            game_bounds.x() + game_dashboard::kToolbarEdgePadding);
  EXPECT_EQ(toolbar_bounds.y(), game_bounds.bottom() -
                                    game_dashboard::kToolbarEdgePadding -
                                    toolbar_size.height());
}

// Verifies the toolbar's snap location is preserved even after the visibility
// is hidden via the main menu view.
TEST_P(GameTypeGameDashboardContextTest, MoveAndHideToolbarWidget) {
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();

  // Move toolbar to bottom left quadrant and verify snap location is updated.
  const auto window_bounds = game_window_->GetBoundsInScreen();
  const auto window_center_point = window_bounds.CenterPoint();
  DragToolbarToPoint(Movement::kMouse,
                     {window_center_point.x() - (window_bounds.width() / 4),
                      window_center_point.y() + (window_bounds.height() / 4)});
  EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
            ToolbarSnapLocation::kBottomLeft);

  // Hide then show the toolbar and verify the toolbar was placed back into the
  // bottom left quadrant.
  test_api_->OpenTheMainMenu();
  test_api_->CloseTheToolbar();
  test_api_->OpenTheToolbar();
  EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
            ToolbarSnapLocation::kBottomLeft);
}

// Verifies the settings view can be closed via the back arrow and the Game
// Dashboard button.
TEST_P(GameTypeGameDashboardContextTest, OpenAndCloseSettingsView) {
  test_api_->OpenTheMainMenu();
  test_api_->OpenMainMenuSettings();

  // Close the settings page via the back button and verify the main menu is now
  // displayed.
  test_api_->CloseTheSettings();
  auto* main_menu_container = test_api_->GetMainMenuContainer();
  EXPECT_TRUE(test_api_->GetMainMenuView());
  EXPECT_TRUE(main_menu_container && main_menu_container->GetVisible());

  // Re-open the settings view and close it via the Game Dashboard button.
  test_api_->OpenMainMenuSettings();
  test_api_->CloseTheMainMenu();
}

// Verifies the Welcome Dialog switch can be toggled off in the settings and its
// state preserved.
TEST_P(GameTypeGameDashboardContextTest, ToggleWelcomeDialogSettings) {
  // Open the settings with the welcome dialog flag disabled.
  test_api_->OpenTheMainMenu();
  test_api_->OpenMainMenuSettings();

  // Verify the initial welcome dialog switch state is disabled.
  EXPECT_FALSE(test_api_->GetSettingsViewWelcomeDialogSwitch()->GetIsOn());

  // Toggle the switch on, close the main menu, then reopen settings and verify
  // the switch is still on.
  test_api_->ToggleWelcomeDialogSettingsSwitch();
  EXPECT_TRUE(test_api_->GetSettingsViewWelcomeDialogSwitch()->GetIsOn());
  test_api_->CloseTheMainMenu();
  test_api_->OpenTheMainMenu();
  test_api_->OpenMainMenuSettings();
  EXPECT_TRUE(test_api_->GetSettingsViewWelcomeDialogSwitch()->GetIsOn());
}

TEST_P(GameTypeGameDashboardContextTest, TabletMode) {
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();

  // App is launched in desktop mode in Setup and switch to the tablet mode.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());
  VerifyFeaturesEnabled(/*expect_enabled=*/false);
  EXPECT_TRUE(
      ToastManager::Get()->IsToastShown(game_dashboard::kTabletToastId));
  // Switch back to the desktop mode and this feature is resumed.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());
  VerifyFeaturesEnabled(/*expect_enabled=*/true, /*toolbar_visible=*/true);
  EXPECT_FALSE(
      ToastManager::Get()->IsToastShown(game_dashboard::kTabletToastId));
  CloseGameWindow();

  // No toast shown when there is no game window.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(
      ToastManager::Get()->IsToastShown(game_dashboard::kTabletToastId));

  // Launch app in the tablet mode and switch to the desktop mode.
  CreateGameWindow(IsArcGame());
  VerifyFeaturesEnabled(/*expect_enabled=*/false);
  EXPECT_FALSE(
      ToastManager::Get()->IsToastShown(game_dashboard::kTabletToastId));
  // Switch back to the desktop mode and this feature is resumed.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());
  VerifyFeaturesEnabled(/*expect_enabled=*/true);
  EXPECT_FALSE(
      ToastManager::Get()->IsToastShown(game_dashboard::kTabletToastId));

  // Start recording in the desktop mode and switch to the tablet mode.
  test_api_->OpenTheMainMenu();
  LeftClickOn(test_api_->GetMainMenuRecordGameTile());
  // Clicking on the record game tile closes the main menu, and asynchronously
  // starts the capture session. Run until idle to ensure that the posted task
  // runs synchronously and completes before proceeding.
  base::RunLoop().RunUntilIdle();
  ClickOnStartRecordingButtonInCaptureModeBarView();
  EXPECT_TRUE(CaptureModeController::Get()->is_recording_in_progress());
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_FALSE(CaptureModeController::Get()->is_recording_in_progress());
  EXPECT_TRUE(
      ToastManager::Get()->IsToastShown(game_dashboard::kTabletToastId));
}

// -----------------------------------------------------------------------------
// OnOverviewModeEndedWaiter:
class OnOverviewModeEndedWaiter : public OverviewObserver {
 public:
  OnOverviewModeEndedWaiter()
      : overview_controller_(OverviewController::Get()) {
    CHECK(overview_controller_);
    overview_controller_->AddObserver(this);
  }
  OnOverviewModeEndedWaiter(const OnOverviewModeEndedWaiter&) = delete;
  OnOverviewModeEndedWaiter& operator=(const OnOverviewModeEndedWaiter&) =
      delete;
  ~OnOverviewModeEndedWaiter() override {
    overview_controller_->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  // OverviewObserver:
  void OnOverviewModeEnded() override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
  // Owned by Shell.
  const raw_ptr<OverviewController> overview_controller_;
};

// Verifies that in overview mode, the Game Dashboard button is not visible, the
// main menu is closed, and the toolbar visibility is unchanged.
TEST_P(GameTypeGameDashboardContextTest, OverviewMode) {
  auto* game_dashboard_button_widget =
      test_api_->GetGameDashboardButtonWidget();
  ASSERT_TRUE(game_dashboard_button_widget);

  // Open the main menu view and toolbar.
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();

  // Verify the initial state.
  // Game Dashboard button is visible.
  EXPECT_TRUE(game_dashboard_button_widget->IsVisible());
  // Toolbar is visible.
  const auto* toolbar_widget = test_api_->GetToolbarWidget();
  ASSERT_TRUE(toolbar_widget);
  EXPECT_TRUE(toolbar_widget->IsVisible());
  // Main menu is visible.
  const auto* main_menu_widget = test_api_->GetMainMenuWidget();
  ASSERT_TRUE(main_menu_widget);
  EXPECT_TRUE(main_menu_widget->IsVisible());

  EnterOverview();
  const auto* overview_controller = OverviewController::Get();
  ASSERT_TRUE(overview_controller->InOverviewSession());

  // Verify states in overview mode.
  EXPECT_FALSE(game_dashboard_button_widget->IsVisible());
  ASSERT_EQ(toolbar_widget, test_api_->GetToolbarWidget());
  EXPECT_TRUE(toolbar_widget->IsVisible());
  EXPECT_FALSE(test_api_->GetMainMenuWidget());

  OnOverviewModeEndedWaiter waiter;
  ExitOverview();
  waiter.Wait();
  ASSERT_FALSE(overview_controller->InOverviewSession());

  // Verify states after exiting overview mode.
  EXPECT_TRUE(game_dashboard_button_widget->IsVisible());
  ASSERT_EQ(toolbar_widget, test_api_->GetToolbarWidget());
  EXPECT_TRUE(toolbar_widget->IsVisible());
  EXPECT_FALSE(test_api_->GetMainMenuWidget());
}

TEST_P(GameTypeGameDashboardContextTest, OverviewModeWithTabletMode) {
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  const auto* overview_controller = OverviewController::Get();

  // 1. Clamshell -> overrview -> tablet-> exit overview.
  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EnterOverview();
  ASSERT_TRUE(overview_controller->InOverviewSession());
  VerifyFeaturesEnabled(/*expect_enabled=*/false, /*toolbar_visible=*/true);
  ash::TabletModeControllerTestApi().EnterTabletMode();
  VerifyFeaturesEnabled(/*expect_enabled=*/false);
  ExitOverview();
  ASSERT_FALSE(overview_controller->InOverviewSession());
  VerifyFeaturesEnabled(/*expect_enabled=*/false);

  // 2. Tablet -> overview -> exit overview -> clamshell.
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EnterOverview();
  ASSERT_TRUE(overview_controller->InOverviewSession());
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());
  VerifyFeaturesEnabled(/*expect_enabled=*/false);
  ExitOverview();
  ASSERT_FALSE(overview_controller->InOverviewSession());
  VerifyFeaturesEnabled(/*expect_enabled=*/false);
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());
  VerifyFeaturesEnabled(/*expect_enabled=*/true, /*toolbar_visible=*/true);

  // 3. Tablet -> overview -> clamshell -> exit overview.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EnterOverview();
  ASSERT_TRUE(overview_controller->InOverviewSession());
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());
  ASSERT_TRUE(overview_controller->InOverviewSession());
  VerifyFeaturesEnabled(/*expect_enabled=*/false, /*toolbar_visible=*/true);
  ExitOverview();
  ASSERT_FALSE(overview_controller->InOverviewSession());
  VerifyFeaturesEnabled(/*expect_enabled=*/true, /*toolbar_visible=*/true);
}

TEST_P(GameTypeGameDashboardContextTest, RecordToggleMainMenuHistogramTest) {
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const std::string histogram_name_on =
      BuildGameDashboardHistogramName(kGameDashboardToggleMainMenuHistogram)
          .append(kGameDashboardHistogramSeparator)
          .append(kGameDashboardHistogramOn);
  const std::string histogram_name_off =
      BuildGameDashboardHistogramName(kGameDashboardToggleMainMenuHistogram)
          .append(kGameDashboardHistogramSeparator)
          .append(kGameDashboardHistogramOff);

  // Toggle on/off main menu by pressing GD button.
  test_api_->OpenTheMainMenu();
  VerifyToggleMainMenuHistogram(
      histograms, histogram_name_on,
      std::vector<int>{/*kGameDashboardButton=*/1, 0, 0, 0, 0, 0, 0});
  const int64_t gd_button_toggle_method = static_cast<int64_t>(
      GameDashboardMainMenuToggleMethod::kGameDashboardButton);
  VerifyToggleMainMenuLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/1u,
      std::vector<int64_t>{/*toggle_on=*/1,
                           /*toggle_method=*/gd_button_toggle_method});

  test_api_->CloseTheMainMenu();
  VerifyToggleMainMenuHistogram(
      histograms, histogram_name_off,
      std::vector<int>{/*kGameDashboardButton=*/1, 0, 0, 0, 0, 0, 0});
  VerifyToggleMainMenuLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/2u,
      std::vector<int64_t>{/*toggle_on=*/0,
                           /*toggle_method=*/gd_button_toggle_method});

  // Toggle on/off main menu by Search+G.
  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_G, ui::EF_COMMAND_DOWN);
  VerifyToggleMainMenuHistogram(
      histograms, histogram_name_on,
      std::vector<int>{1, /*kSearchPlusG=*/1, 0, 0, 0, 0, 0});
  VerifyToggleMainMenuLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/3u,
      std::vector<int64_t>{
          /*toggle_on=*/1,
          /*toggle_method=*/static_cast<int64_t>(
              GameDashboardMainMenuToggleMethod::kSearchPlusG)});

  event_generator->PressAndReleaseKey(ui::VKEY_G, ui::EF_COMMAND_DOWN);
  VerifyToggleMainMenuHistogram(
      histograms, histogram_name_off,
      std::vector<int>{1, /*kSearchPlusG=*/1, 0, 0, 0, 0, 0});
  VerifyToggleMainMenuLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/4u,
      std::vector<int64_t>{
          /*toggle_on=*/0,
          /*toggle_method=*/static_cast<int64_t>(
              GameDashboardMainMenuToggleMethod::kSearchPlusG)});

  // Toggle off main menu by key Esc.
  test_api_->OpenTheMainMenu();
  VerifyToggleMainMenuHistogram(
      histograms, histogram_name_on,
      std::vector<int>{/*kGameDashboardButton=*/2, 1, 0, 0, 0, 0, 0});
  VerifyToggleMainMenuLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/5u,
      std::vector<int64_t>{/*toggle_on=*/1,
                           /*toggle_method=*/gd_button_toggle_method});
  event_generator->PressAndReleaseKey(ui::VKEY_ESCAPE);
  // Main menu is closed asynchronously. Run until idle to ensure that this
  // posted task runs synchronously and completes before proceeding.
  base::RunLoop().RunUntilIdle();
  VerifyToggleMainMenuHistogram(histograms, histogram_name_off,
                                std::vector<int>{1, 1, /*kEsc=*/1, 0, 0, 0, 0});
  VerifyToggleMainMenuLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/6u,
      std::vector<int64_t>{/*toggle_on=*/0,
                           /*toggle_method=*/static_cast<int64_t>(
                               GameDashboardMainMenuToggleMethod::kEsc)});

  // Toggle off main menu by activating a new feature.
  test_api_->OpenTheMainMenu();
  VerifyToggleMainMenuHistogram(
      histograms, histogram_name_on,
      std::vector<int>{/*kGameDashboardButton=*/3, 1, 0, 0, 0, 0, 0});
  VerifyToggleMainMenuLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/7u,
      std::vector<int64_t>{/*toggle_on=*/1,
                           /*toggle_method=*/gd_button_toggle_method});
  LeftClickOn(test_api_->GetMainMenuScreenshotTile());
  VerifyToggleMainMenuHistogram(
      histograms, histogram_name_off,
      std::vector<int>{1, 1, 1, /*kActivateNewFeature=*/1, 0, 0, 0});
  VerifyToggleMainMenuLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/8u,
      std::vector<int64_t>{
          /*toggle_on=*/0,
          /*toggle_method=*/static_cast<int64_t>(
              GameDashboardMainMenuToggleMethod::kActivateNewFeature)});

  // Toggle off main menu by entering overview mode.
  test_api_->OpenTheMainMenu();
  VerifyToggleMainMenuHistogram(
      histograms, histogram_name_on,
      std::vector<int>{/*kGameDashboardButton=*/4, 1, 0, 0, 0, 0, 0});
  VerifyToggleMainMenuLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/9u,
      std::vector<int64_t>{/*toggle_on=*/1,
                           /*toggle_method=*/gd_button_toggle_method});
  EnterOverview();
  VerifyToggleMainMenuHistogram(
      histograms, histogram_name_off,
      std::vector<int>{1, 1, 1, 1, /*kOverview=*/1, 0, 0});
  VerifyToggleMainMenuLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/10u,
      std::vector<int64_t>{/*toggle_on=*/0,
                           /*toggle_method=*/static_cast<int64_t>(
                               GameDashboardMainMenuToggleMethod::kOverview)});
  OnOverviewModeEndedWaiter waiter;
  ExitOverview();
  waiter.Wait();

  // Toggle off main menu by entering the tablet mode.
  test_api_->OpenTheMainMenu();
  VerifyToggleMainMenuHistogram(
      histograms, histogram_name_on,
      std::vector<int>{/*kGameDashboardButton=*/5, 1, 0, 0, 0, 0, 0});
  VerifyToggleMainMenuLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/11u,
      std::vector<int64_t>{/*toggle_on=*/1,
                           /*toggle_method=*/gd_button_toggle_method});
  ash::TabletModeControllerTestApi().EnterTabletMode();
  VerifyToggleMainMenuHistogram(
      histograms, histogram_name_off,
      std::vector<int>{1, 1, 1, 1, 1, 0, /*kTabletMode=*/1});
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  VerifyToggleMainMenuLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/12u,
      std::vector<int64_t>{
          /*toggle_on=*/0,
          /*toggle_method=*/static_cast<int64_t>(
              GameDashboardMainMenuToggleMethod::kTabletMode)});

  // Toggle off main menu by clicking outside of the main menu.
  test_api_->OpenTheMainMenu();
  VerifyToggleMainMenuHistogram(
      histograms, histogram_name_on,
      std::vector<int>{/*kGameDashboardButton=*/6, 1, 0, 0, 0, 0, 0});
  VerifyToggleMainMenuLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/13u,
      std::vector<int64_t>{/*toggle_on=*/1,
                           /*toggle_method=*/gd_button_toggle_method});
  const gfx::Point bottom_center =
      test_api_->GetMainMenuView()->GetBoundsInScreen().bottom_center();
  event_generator->MoveMouseTo(
      gfx::Point(bottom_center.x(), bottom_center.y() + 10));
  event_generator->ClickLeftButton();
  // Main menu is closed asynchronously. Run until idle to ensure that this
  // posted task runs synchronously and completes before proceeding.
  base::RunLoop().RunUntilIdle();
  VerifyToggleMainMenuHistogram(
      histograms, histogram_name_off,
      std::vector<int>{1, 1, 1, 1, 1, /*kOthers=*/1, 1});
  VerifyToggleMainMenuLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/14u,
      std::vector<int64_t>{/*toggle_on=*/0,
                           /*toggle_method=*/static_cast<int64_t>(
                               GameDashboardMainMenuToggleMethod::kOthers)});

  test_api_->OpenTheMainMenu();
  VerifyToggleMainMenuHistogram(
      histograms, histogram_name_on,
      std::vector<int>{/*kGameDashboardButton=*/7, 1, 0, 0, 0, 0, 0});
  VerifyToggleMainMenuLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/15u,
      std::vector<int64_t>{/*toggle_on=*/1,
                           /*toggle_method=*/gd_button_toggle_method});
  CloseGameWindow();
  // Main menu is closed asynchronously. Run until idle to ensure that this
  // posted task runs synchronously and completes before proceeding.
  base::RunLoop().RunUntilIdle();
  VerifyToggleMainMenuHistogram(
      histograms, histogram_name_off,
      std::vector<int>{1, 1, 1, 1, 1, /*kOthers=*/2, 1});
  VerifyToggleMainMenuLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/16u,
      std::vector<int64_t>{/*toggle_on=*/0,
                           /*toggle_method=*/static_cast<int64_t>(
                               GameDashboardMainMenuToggleMethod::kOthers)});
}

TEST_P(GameTypeGameDashboardContextTest,
       RecordToolbarToggleStateHistogramTest) {
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();

  VerifyToggleToolbarHistogram(histograms,
                               std::vector<int>{0, /*toggle_on=*/1});
  VerifyToolbarToggleStateLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/1u, /*expect_histograms_value=*/1);

  test_api_->CloseTheToolbar();
  VerifyToggleToolbarHistogram(histograms,
                               std::vector<int>{/*toggle_off=*/1, 1});
  VerifyToolbarToggleStateLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/2u, /*expect_histograms_value=*/0);
}

TEST_P(GameTypeGameDashboardContextTest,
       RecordRecordingStartSourceHistogramTest) {
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Start recording from the main menu.
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  LeftClickOn(test_api_->GetMainMenuRecordGameTile());
  // Clicking on the record game tile closes the main menu, and asynchronously
  // starts the capture session. Run until idle to ensure that the posted task
  // runs synchronously and completes before proceeding.
  base::RunLoop().RunUntilIdle();
  ClickOnStartRecordingButtonInCaptureModeBarView();
  VerifyStartRecordingHistogram(histograms,
                                std::vector<int>{/*kMainMenu=*/1, 0});
  VerifyRecordingStartSourceLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/1u, /*expect_histograms_value=*/
      static_cast<int64_t>(GameDashboardMenu::kMainMenu));

  // Stop recording.
  LeftClickOn(test_api_->GetToolbarRecordGameButton());
  WaitForCaptureFileToBeSaved();

  // Start recording from the toolbar.
  LeftClickOn(test_api_->GetToolbarRecordGameButton());
  ClickOnStartRecordingButtonInCaptureModeBarView();
  VerifyStartRecordingHistogram(histograms,
                                std::vector<int>{1, /*kToolbar=*/1});
  VerifyRecordingStartSourceLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/2u, /*expect_histograms_value=*/
      static_cast<int64_t>(GameDashboardMenu::kToolbar));
}

TEST_P(GameTypeGameDashboardContextTest,
       RecordScreenshotTakeSourceHistogramTest) {
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  test_api_->OpenTheMainMenu();
  LeftClickOn(test_api_->GetMainMenuScreenshotTile());
  VerifyTakeScreenshotHistogram(histograms,
                                std::vector<int>{/*kMainMenu=*/1, 0});
  VerifyScreenshotTakeSourceLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/1u, /*expect_histograms_value=*/
      static_cast<int64_t>(GameDashboardMenu::kMainMenu));

  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  LeftClickOn(test_api_->GetToolbarScreenshotButton());
  VerifyTakeScreenshotHistogram(histograms,
                                std::vector<int>{1, /*kToolbar=*/1});
  VerifyScreenshotTakeSourceLastUkmHistogram(
      ukm_recorder, /*expect_entry_size=*/2u, /*expect_histograms_value=*/
      static_cast<int64_t>(GameDashboardMenu::kToolbar));
}

INSTANTIATE_TEST_SUITE_P(All,
                         GameTypeGameDashboardContextTest,
                         testing::Bool());

// -----------------------------------------------------------------------------
// GameDashboardStartAndStopCaptureSessionTest:
// Test fixture to verify the game window can be started and stopped from the
// main menu and toolbar, for both ARC and GeForceNow game windows.
class GameDashboardStartAndStopCaptureSessionTest
    : public GameDashboardContextTest,
      public testing::WithParamInterface<
          std::tuple</*is_arc_game_=*/bool,
                     /*should_start_from_main_menu_=*/bool,
                     /*should_stop_from_main_menu_=*/bool>> {
 public:
  GameDashboardStartAndStopCaptureSessionTest()
      : is_arc_game_(std::get<0>(GetParam())),
        should_start_from_main_menu_(std::get<1>(GetParam())),
        should_stop_from_main_menu_(std::get<2>(GetParam())) {}
  ~GameDashboardStartAndStopCaptureSessionTest() override = default;

  void SetUp() override {
    GameDashboardContextTest::SetUp();
    CreateGameWindow(is_arc_game_);
  }

 protected:
  const bool is_arc_game_;
  const bool should_start_from_main_menu_;
  const bool should_stop_from_main_menu_;
};

// GameDashboardStartAndStopCaptureSessionTest Tests
// -----------------------------------------------------------------------
// Verifies the game window recording starts and stops for the given set of test
// parameters.
TEST_P(GameDashboardStartAndStopCaptureSessionTest, RecordGameFromMainMenu) {
  const auto* capture_mode_controller = CaptureModeController::Get();
  const auto& timer = test_api_->GetRecordingTimer();

  test_api_->OpenTheMainMenu();
  EXPECT_FALSE(capture_mode_controller->is_recording_in_progress());
  EXPECT_FALSE(timer.IsRunning());
  VerifyGameDashboardButtonState(/*is_recording=*/false);

  if (should_start_from_main_menu_) {
    // Retrieve the record game tile from the main menu.
    const auto* record_game_tile = test_api_->GetMainMenuRecordGameTile();
    ASSERT_TRUE(record_game_tile);

    // Start the video recording from the main menu.
    LeftClickOn(record_game_tile);
    // Clicking on the record game tile closes the main menu, and asynchronously
    // starts the capture session. Run until idle to ensure that the posted task
    // runs synchronously and completes before proceeding.
    base::RunLoop().RunUntilIdle();
  } else {
    // Retrieve the record game button from the toolbar.
    CHECK(!test_api_->GetToolbarView());
    test_api_->OpenTheToolbar();
    test_api_->CloseTheMainMenu();
    const auto* record_game_button = test_api_->GetToolbarRecordGameButton();
    ASSERT_TRUE(record_game_button);

    // Start the video recording from the toolbar.
    LeftClickOn(record_game_button);
  }
  ClickOnStartRecordingButtonInCaptureModeBarView();

  EXPECT_TRUE(capture_mode_controller->is_recording_in_progress());
  EXPECT_TRUE(timer.IsRunning());
  VerifyGameDashboardButtonState(/*is_recording=*/true);

  if (should_stop_from_main_menu_) {
    // Stop the video recording from the main menu.
    test_api_->OpenTheMainMenu();
    LeftClickOn(test_api_->GetMainMenuRecordGameTile());
  } else {
    // Open the toolbar, if the video recording started from the main menu.
    if (should_start_from_main_menu_) {
      test_api_->OpenTheMainMenu();
      test_api_->OpenTheToolbar();
      test_api_->CloseTheMainMenu();
    }
    // Verify the toolbar is open.
    CHECK(test_api_->GetToolbarView());
    // Stop the video recording from the toolbar.
    LeftClickOn(test_api_->GetToolbarRecordGameButton());
  }
  EXPECT_FALSE(capture_mode_controller->is_recording_in_progress());
  EXPECT_FALSE(timer.IsRunning());
  VerifyGameDashboardButtonState(/*is_recording=*/false);
  WaitForCaptureFileToBeSaved();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GameDashboardStartAndStopCaptureSessionTest,
    testing::Combine(/*is_arc_game_=*/testing::Bool(),
                     /*should_start_from_main_menu_=*/testing::Bool(),
                     /*should_stop_from_main_menu_=*/testing::Bool()));

// -----------------------------------------------------------------------------
// GameDashboardUIStartupSequenceTest:
// Test fixture to verify the toolbar and welcome dialog startup sequence when
// opening a game window. This fixture runs through all combinations of whether
// the toolbar and welcome dialog should be shown or not.
class GameDashboardUIStartupSequenceTest
    : public GameDashboardContextTest,
      public testing::WithParamInterface<
          std::tuple</*show_toolbar=*/bool,
                     /*show_welcome_dialog=*/bool>> {
 public:
  GameDashboardUIStartupSequenceTest()
      : should_show_toolbar_(std::get<0>(GetParam())),
        should_show_welcome_dialog_(std::get<1>(GetParam())) {}
  ~GameDashboardUIStartupSequenceTest() override = default;

  void SetUp() override {
    GameDashboardContextTest::SetUp();
    SetShowWelcomeDialog(should_show_welcome_dialog_);
    SetShowToolbar(should_show_toolbar_);
    CreateGameWindow(/*is_arc_window=*/true,
                     /*set_arc_game_controls_flags_prop=*/true);
  }

  void VerifyToolbarVisibility(bool visible) {
    if (visible) {
      ASSERT_TRUE(test_api_->GetToolbarWidget());
    } else {
      ASSERT_FALSE(test_api_->GetToolbarWidget());
    }
  }

  void VerifyWelcomeDialogVisibility(bool visible) {
    if (visible) {
      ASSERT_TRUE(test_api_->GetWelcomeDialogWidget());
    } else {
      ASSERT_FALSE(test_api_->GetWelcomeDialogWidget());
    }
  }

 protected:
  const bool should_show_toolbar_;
  const bool should_show_welcome_dialog_;
};

// GameDashboardUIStartupSequenceTest Tests
// -----------------------------------------------------------------------
// Verifies the toolbar is visible after the welcome dialog is dismissed.
TEST_P(GameDashboardUIStartupSequenceTest, ToolbarAndShowWelcomeDialogStartup) {
  if (should_show_welcome_dialog_) {
    // Verify the welcome dialog is visible and the toolbar is not visible.
    VerifyWelcomeDialogVisibility(/*visible=*/true);
    VerifyToolbarVisibility(/*visible=*/false);

    // Advance by 4 seconds to dismiss the welcome dialog.
    task_environment()->FastForwardBy(base::Seconds(4));
  }

  VerifyWelcomeDialogVisibility(/*visible=*/false);
  VerifyToolbarVisibility(/*visible=*/should_show_toolbar_);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GameDashboardUIStartupSequenceTest,
    testing::Combine(/*should_show_toolbar_=*/testing::Bool(),
                     /*should_show_welcome_dialog_=*/testing::Bool()));

}  // namespace ash
