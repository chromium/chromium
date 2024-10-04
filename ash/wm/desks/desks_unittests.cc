// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <string>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/sticky_keys/sticky_keys_controller.h"
#include "ash/accessibility/ui/accessibility_confirmation_dialog.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/multi_user_window_manager_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/test/test_desk_profiles_delegate.h"
#include "ash/public/cpp/test/test_shelf_item_delegate.h"
#include "ash/public/cpp/window_finder.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/scroll_arrow_view.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/close_button.h"
#include "ash/style/color_util.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/test_widget_builder.h"
#include "ash/wm/desks/default_desk_button.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_action_button.h"
#include "ash/wm/desks/desk_action_context_menu.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_animation_base.h"
#include "ash/wm/desks/desk_bar_controller.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_button/desk_button.h"
#include "ash/wm/desks/desk_button/desk_button_container.h"
#include "ash/wm/desks/desk_button/desk_switch_button.h"
#include "ash/wm/desks/desk_button_base.h"
#include "ash/wm/desks/desk_icon_button.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desk_profiles_button.h"
#include "ash/wm/desks/desk_textfield.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/desks_test_api.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/desks/root_window_desk_switch_animator_test_api.h"
#include "ash/wm/desks/scroll_arrow_button.h"
#include "ash/wm/desks/templates/saved_desk_test_helper.h"
#include "ash/wm/desks/templates/saved_desk_test_util.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/containers/contains.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/desks/move_to_desks_menu_delegate.h"
#include "chromeos/ui/frame/desks/move_to_desks_menu_model.h"
#include "chromeos/ui/wm/desks/chromeos_desks_histogram_enums.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/ash/event_rewriter_ash.h"
#include "ui/events/ash/fake_event_rewriter_ash_delegate.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/client_view.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/window_modality_controller.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::ValuesIn;

std::unique_ptr<aura::Window> CreateTransientWindow(
    aura::Window* transient_parent,
    const gfx::Rect& bounds) {
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr, aura::client::WINDOW_TYPE_POPUP);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetBounds(bounds);
  ::wm::AddTransientChild(transient_parent, window.get());
  aura::client::ParentWindowWithContext(window.get(),
                                        transient_parent->GetRootWindow(),
                                        bounds, display::kInvalidDisplayId);
  window->Show();
  return window;
}

std::unique_ptr<aura::Window> CreateTransientModalChildWindow(
    aura::Window* transient_parent) {
  auto child =
      CreateTransientWindow(transient_parent, gfx::Rect(20, 30, 200, 150));
  child->SetProperty(aura::client::kModalKey, ui::mojom::ModalType::kWindow);
  ::wm::SetModalParent(child.get(), transient_parent);
  return child;
}

bool DoesActiveDeskContainWindow(aura::Window* window) {
  return base::Contains(DesksController::Get()->active_desk()->windows(),
                        window);
}

// Combines or closes a desk.
void CloseDeskFromMiniView(DeskMiniView* desk_mini_view,
                           ui::test::EventGenerator* event_generator) {
  DCHECK(desk_mini_view);
  DeskBarViewBase* desk_bar_view = desk_mini_view->owner_bar();
  // Move to the center of the mini view so that the close button shows up.
  const gfx::Point mini_view_center =
      desk_mini_view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(mini_view_center);

  // This is the combine button if available, close button otherwise.
  const CloseButton* close_button =
      GetCloseDeskButtonForMiniView(desk_mini_view);
  EXPECT_TRUE(close_button->GetVisible());

  // Move to the center of the close button and click.
  event_generator->MoveMouseTo(close_button->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
  views::test::RunScheduledLayout(desk_bar_view);
}

void DoubleClickOnView(const views::View* view,
                       ui::test::EventGenerator* event_generator) {
  DCHECK(view);

  const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(view_center);
  event_generator->DoubleClickLeftButton();
}

void SendAccessibleActionToView(views::View* view, ax::mojom::Action action) {
  ui::AXActionData action_data;
  action_data.action = action;
  view->HandleAccessibleAction(action_data);
}

BackdropController* GetDeskBackdropController(const Desk* desk,
                                              aura::Window* root) {
  auto* workspace_controller =
      GetWorkspaceController(desk->GetDeskContainerForRoot(root));
  WorkspaceLayoutManager* layout_manager =
      workspace_controller->layout_manager();
  return layout_manager->backdrop_controller();
}

// Simulate pressing on a desk preview.
void LongTapOnDeskPreview(const DeskMiniView* desk_mini_view,
                          ui::test::EventGenerator* event_generator) {
  DCHECK(desk_mini_view);

  gfx::Point desk_preview_center =
      desk_mini_view->GetPreviewBoundsInScreen().CenterPoint();

  LongGestureTap(desk_preview_center, event_generator, /*release_touch=*/false);
}

// Simulate drag on a desk preview.
void StartDragDeskPreview(const DeskMiniView* desk_mini_view,
                          ui::test::EventGenerator* event_generator) {
  DCHECK(desk_mini_view);

  gfx::Point desk_preview_center =
      desk_mini_view->GetPreviewBoundsInScreen().CenterPoint();
  event_generator->set_current_screen_location(desk_preview_center);

  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(0, 50);
}

TestDeskProfilesDelegate& GetDeskProfilesTestDelegate() {
  CHECK(chromeos::features::IsDeskProfilesEnabled());
  return *static_cast<TestDeskProfilesDelegate*>(
      Shell::Get()->GetDeskProfilesDelegate());
}

uint64_t GetDummyLacrosDeskProfileId(size_t index) {
  static constexpr uint64_t kLacrosProfileIdBase = 1001;
  return kLacrosProfileIdBase + index * 2;
}

// Adds `count` dummy lacros profiles to the test delegate.
void AddDummyLacrosDeskProfiles(size_t count) {
  for (size_t i = 0; i != count; ++i) {
    LacrosProfileSummary summary;
    summary.profile_id = GetDummyLacrosDeskProfileId(i);
    summary.name =
        base::UTF8ToUTF16(base::StringPrintf("Lacros user %lu", i + 1));
    summary.email =
        base::UTF8ToUTF16(base::StringPrintf("email%lu@gmail.com", i + 1));
    summary.icon = gfx::test::CreateImageSkia(32, 32);

    GetDeskProfilesTestDelegate().UpdateTestProfile(std::move(summary));
  }
}

// Sends tab keys (or shift+tab) until the passed predicate becomes true, or a
// fixed max is hit.
template <class Predicate>
bool TabUntil(bool reverse,
              Predicate&& predicate,
              ui::test::EventGenerator* generator) {
  for (int i = 0; i != 100; ++i) {
    if (predicate()) {
      return true;
    }

    SendKey(ui::VKEY_TAB, generator, reverse ? ui::EF_SHIFT_DOWN : ui::EF_NONE);
  }
  return false;
}

// Returns a predicate that will return true if the given `view` is focused.
auto ViewFocused(views::View* view) {
  return [view] { return view->HasFocus(); };
}

// Defines an observer to test DesksController notifications.
class TestObserver : public DesksController::Observer {
 public:
  TestObserver() = default;

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override = default;

  const std::vector<raw_ptr<const Desk, VectorExperimental>>& desks() const {
    return desks_;
  }
  int desk_name_changed_notify_counts() const {
    return desk_name_changed_notify_counts_;
  }

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk, bool from_undo) override {
    const size_t new_desk_index = DesksController::Get()->GetDeskIndex(desk);
    if (new_desk_index > desks_.size()) {
      desks_.emplace_back(desk);
    } else {
      desks_.insert(desks_.begin() + new_desk_index, desk);
    }
    EXPECT_TRUE(DesksController::Get()->AreDesksBeingModified());
  }
  void OnDeskRemoved(const Desk* desk) override {
    std::erase(desks_, desk);
    EXPECT_TRUE(DesksController::Get()->AreDesksBeingModified());
  }
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override {
    EXPECT_TRUE(DesksController::Get()->AreDesksBeingModified());
  }
  void OnDeskSwitchAnimationFinished() override {
    EXPECT_FALSE(DesksController::Get()->AreDesksBeingModified());
  }
  void OnDeskNameChanged(const Desk* desk,
                         const std::u16string& new_name) override {
    ++desk_name_changed_notify_counts_;
  }

 private:
  std::vector<raw_ptr<const Desk, VectorExperimental>> desks_;

  int desk_name_changed_notify_counts_ = 0;
};

class TestDeskObserver : public Desk::Observer {
 public:
  TestDeskObserver() = default;

  TestDeskObserver(const TestDeskObserver&) = delete;
  TestDeskObserver& operator=(const TestDeskObserver&) = delete;

  ~TestDeskObserver() override = default;

  int notify_counts() const { return notify_counts_; }

  const std::vector<uint64_t>& lacros_profile_id_updates() const {
    return lacros_profile_id_updates_;
  }

  // Desk::Observer:
  void OnContentChanged() override { ++notify_counts_; }
  void OnDeskDestroyed(const Desk* desk) override {}
  void OnDeskNameChanged(const std::u16string& new_name) override {}
  void OnDeskProfileChanged(uint64_t lacros_profile_id) override {
    lacros_profile_id_updates_.push_back(lacros_profile_id);
  }

 private:
  int notify_counts_ = 0;
  std::vector<uint64_t> lacros_profile_id_updates_;
};

class FullScreenStateObserver : public ShellObserver {
 public:
  FullScreenStateObserver() { Shell::Get()->AddShellObserver(this); }

  FullScreenStateObserver(const FullScreenStateObserver&) = delete;
  FullScreenStateObserver& operator=(const FullScreenStateObserver&) = delete;

  ~FullScreenStateObserver() override {
    Shell::Get()->RemoveShellObserver(this);
  }

  // ShellObserver:
  void OnFullscreenStateChanged(bool is_fullscreen,
                                aura::Window* container) override {
    is_fullscreen_ = is_fullscreen;
  }

  bool is_fullscreen() const { return is_fullscreen_; }

 private:
  bool is_fullscreen_ = false;
};

// A widget delegate that refuses to close.
class StuckWidgetDelegate : public views::WidgetDelegate {
 public:
  StuckWidgetDelegate() {
    SetCanMaximize(true);
    SetCanMinimize(true);
    SetCanResize(true);
    SetOwnedByWidget(true);
  }
  StuckWidgetDelegate(const StuckWidgetDelegate& other) = delete;
  StuckWidgetDelegate& operator=(const StuckWidgetDelegate& other) = delete;
  ~StuckWidgetDelegate() override = default;

  // Overridden from WidgetDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override {
    return Shell::Get()->CreateDefaultNonClientFrameView(widget);
  }

  bool OnCloseRequested(views::Widget::ClosedReason close_reason) override {
    return false;
  }
};

struct DesksTestParams {
  bool use_touch_gestures = false;
  bool use_16_desks = false;
  bool per_desk_shelf = false;
};

// Defines a parameterized test fixture to test Virtual Desks behavior.
class DesksTest : public AshTestBase,
                  public ::testing::WithParamInterface<DesksTestParams> {
 public:
  enum class DeskBarScrollDirection { kLeft, kRight };

  DesksTest() = default;
  explicit DesksTest(base::test::TaskEnvironment::TimeSource time)
      : AshTestBase(time) {}

  DesksTest(const DesksTest&) = delete;
  DesksTest& operator=(const DesksTest&) = delete;

  ~DesksTest() override = default;

  void SetUp() override {
    // This allows us to snap to the bottom in portrait mode.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kUseFirstDisplayAsInternal);

    scoped_feature_list_.InitWithFeatureStates(
        {{features::kFeatureManagement16Desks, GetParam().use_16_desks},
         {features::kPerDeskShelf, GetParam().per_desk_shelf},
         {features::kDeskBarWindowOcclusionOptimization, true},
         {chromeos::features::kOverviewSessionInitOptimizations, true}});

    AshTestBase::SetUp();
    SetVirtualKeyboardEnabled(true);

    // Wait for the desk model to have completed its initialization. Not doing
    // this would lead to flaky tests.
    ash_test_helper()->saved_desk_test_helper()->WaitForDeskModels();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    scoped_feature_list_.Reset();
  }

  void VerifyZeroStateNewDeskButtonVisibility(const DeskBarViewBase* bar_view,
                                              bool expected_visibility) {
    const auto state = bar_view->new_desk_button()->state();
    EXPECT_TRUE(expected_visibility ? state == DeskIconButton::State::kZero
                                    : state != DeskIconButton::State::kZero);
  }

  void VerifyExpandedStateNewDeskButtonVisibility(
      const DeskBarViewBase* bar_view,
      bool expected_visibility) {
    const auto state = bar_view->new_desk_button()->state();
    EXPECT_TRUE(expected_visibility
                    ? state == DeskIconButton::State::kExpanded
                    : state != DeskIconButton::State::kExpanded);
  }

  SkColor GetNewDeskButtonBackgroundColor(const DeskBarViewBase* bar_view) {
    return bar_view->new_desk_button()->background()->get_color();
  }

  void TryScrollOverviewDeskBar(DeskBarScrollDirection scroll_direction,
                                bool do_left_click = true) {
    ScrollArrowButton* scroll_button = nullptr;
    switch (scroll_direction) {
      case DeskBarScrollDirection::kLeft:
        scroll_button = DesksTestApi::GetDeskBarLeftScrollButton(
            DeskBarViewBase::Type::kOverview);
        break;
      case DeskBarScrollDirection::kRight:
        scroll_button = DesksTestApi::GetDeskBarRightScrollButton(
            DeskBarViewBase::Type::kOverview);
        break;
    }
    if (!scroll_button || !scroll_button->GetVisible()) {
      return;
    }
    if (do_left_click) {
      LeftClickOn(scroll_button);
    } else {
      GestureTapOn(scroll_button);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(DesksTest, DesksCreationAndRemoval) {
  TestObserver observer;
  auto* controller = DesksController::Get();
  controller->AddObserver(&observer);

  // There's always a default pre-existing desk that cannot be removed.
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_FALSE(controller->CanRemoveDesks());
  EXPECT_TRUE(controller->CanCreateDesks());

  // Add desks until no longer possible.
  while (controller->CanCreateDesks()) {
    NewDesk();
  }

  // Expect we've reached the max number of desks, and we've been notified only
  // with the newly created desks.
  EXPECT_EQ(desks_util::GetMaxNumberOfDesks(), controller->desks().size());
  EXPECT_EQ(desks_util::GetMaxNumberOfDesks() - 1, observer.desks().size());
  EXPECT_TRUE(controller->CanRemoveDesks());

  // Remove all desks until no longer possible, and expect that there's always
  // one default desk remaining.
  while (controller->CanRemoveDesks()) {
    RemoveDesk(observer.desks().back());
  }

  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_FALSE(controller->CanRemoveDesks());
  EXPECT_TRUE(controller->CanCreateDesks());
  EXPECT_TRUE(observer.desks().empty());

  controller->RemoveObserver(&observer);
}

TEST_P(DesksTest, DeskRemovalLifetimeHistogram) {
  base::HistogramTester histogram_tester;

  constexpr std::string_view kHistogramNames[] = {
      "Ash.Desks.DeskLifetime_2", "Ash.Desks.DeskLifetime_Profile_2"};

  auto* controller = DesksController::Get();

  for (bool set_lacros_profile : {false, true}) {
    std::string_view histogram = kHistogramNames[set_lacros_profile];
    SCOPED_TRACE(histogram);

    // Create a new desk and give it a creation time in the past.
    NewDesk();
    auto* desk = controller->desks().back().get();
    desk->set_creation_time(base::Time::Now() - base::Hours(8));

    if (set_lacros_profile) {
      desk->SetLacrosProfileId(GetDummyLacrosDeskProfileId(0),
                               /*source=*/std::nullopt);
    }

    RemoveDesk(desk);
    histogram_tester.ExpectBucketCount(histogram, 8, 1);
  }
}

// Regression test for a crash reported at https://crbug.com/1267069. If a
// window was created while the MRU tracker is paused (so it's not tracked), and
// later the desk on which this window resides is removed, that window will be
// moved to an adjacent desk, and its order in the MRU tracker is updated. But
// the MRU tracker was not tracking it to begin with, so this case has to be
// handled.
TEST_P(DesksTest, DeskRemovalWithPausedMruTracker) {
  NewDesk();
  auto* controller = DesksController::Get();
  EXPECT_EQ(2u, controller->desks().size());

  auto* desk_2 = controller->GetDeskAtIndex(1);
  ActivateDesk(desk_2);
  const auto win_bounds = gfx::Rect{10, 20, 250, 100};
  auto win1 = CreateAppWindow(win_bounds);
  auto* mru_tracker = Shell::Get()->mru_window_tracker();
  // Pause the MRU tracking and create a new window.
  mru_tracker->SetIgnoreActivations(true);
  auto win2 = CreateAppWindow(win_bounds);

  // Enter overview and remove `desk_2`. A crash should not be observed.
  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  RemoveDesk(desk_2);
}

// Verifies that desk's name change notifies |DesksController::Observer|.
TEST_P(DesksTest, OnDeskNameChanged) {
  TestObserver observer;
  auto* controller = DesksController::Get();
  controller->AddObserver(&observer);

  NewDesk();
  controller->GetDeskAtIndex(0)->SetName(u"test1", /*set_by_user=*/true);
  controller->GetDeskAtIndex(1)->SetName(u"test2", /*set_by_user=*/true);

  // Verify that desk name change will trigger
  // |TestObserver::OnDeskNameChanged()|. Notice when creating a new desk
  // and setting its name, it also triggers the call. Thus
  // |desk_name_changed_notify_counts_| is 3.
  ASSERT_EQ(3, observer.desk_name_changed_notify_counts());
  controller->RemoveObserver(&observer);
}

TEST_P(DesksTest, DesksTextfieldAddTooltipText) {
  NewDesk();

  auto* controller = DesksController::Get();

  // Set the first desk with a name which is short enough to be fit in the desk
  // name view.
  controller->GetDeskAtIndex(0)->SetName(u"test1", /*set_by_user=*/true);

  // Set the second desk with a name which is long enough to be truncated in the
  // desk name view.
  std::u16string desk_name2(
      u"test2 a very long desk name to test tooltip text");
  controller->GetDeskAtIndex(1)->SetName(desk_name2, /*set_by_user=*/true);

  // Start overview.
  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  // Expect there to be no tooltip when the desk name is short enough.
  auto* desks_bar_view =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();
  auto* desk_name_view1 = desks_bar_view->mini_views()[0]->desk_name_view();
  EXPECT_TRUE(desk_name_view1->GetTooltipText(gfx::Point()).empty());

  auto* desk_name_view2 = desks_bar_view->mini_views()[1]->desk_name_view();
  EXPECT_EQ(desk_name2, desk_name_view2->GetTooltipText(gfx::Point()));
}

TEST_P(DesksTest, DesksBarViewDeskCreation) {
  auto* controller = DesksController::Get();

  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());

  const auto* desks_bar_view = overview_grid->desks_bar_view();
  EXPECT_FALSE(overview_grid->IsDesksBarViewActive());

  DCHECK(desks_bar_view);
  EXPECT_TRUE(desks_bar_view->mini_views().empty());

  auto* event_generator = GetEventGenerator();
  LeftClickOn(desks_bar_view->default_desk_button());
  RunScheduledLayoutForAllOverviewDeskBars();
  EXPECT_FALSE(desks_bar_view->IsZeroState());

  auto* new_desk_button = desks_bar_view->new_desk_button();
  EXPECT_TRUE(new_desk_button->GetEnabled());

  // Click many times on the expanded new desk button and expect only the max
  // number of desks will be created, and the button is no longer enabled.
  for (size_t i = 0; i < desks_util::GetMaxNumberOfDesks() + 2; ++i) {
    LeftClickOn(new_desk_button);
    RunScheduledLayoutForAllOverviewDeskBars();
    TryScrollOverviewDeskBar(DeskBarScrollDirection::kRight);
    RunScheduledLayoutForAllOverviewDeskBars();
  }

  EXPECT_TRUE(overview_grid->IsDesksBarViewActive());
  EXPECT_EQ(desks_util::GetMaxNumberOfDesks(), controller->desks().size());
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_FALSE(controller->CanCreateDesks());
  EXPECT_TRUE(controller->CanRemoveDesks());
  EXPECT_FALSE(new_desk_button->GetEnabled());
  EXPECT_EQ(views::Button::STATE_DISABLED, new_desk_button->GetState());

  // Hover over one of the mini_views, and expect that the desk action interface
  // becomes visible.
  auto* mini_view = desks_bar_view->mini_views().back().get();
  EXPECT_FALSE(GetDeskActionVisibilityForMiniView(mini_view));
  const gfx::Point mini_view_center =
      mini_view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(mini_view_center);
  RunScheduledLayoutForAllOverviewDeskBars();
  EXPECT_TRUE(GetDeskActionVisibilityForMiniView(mini_view));

  // Use the close button to close the desk.
  CloseDeskFromMiniView(mini_view, event_generator);
  RunScheduledLayoutForAllOverviewDeskBars();

  // The new desk button is now enabled again.
  EXPECT_EQ(desks_util::GetMaxNumberOfDesks() - 1, controller->desks().size());
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_TRUE(controller->CanCreateDesks());
  EXPECT_TRUE(new_desk_button->GetEnabled());
  EXPECT_EQ(views::Button::STATE_NORMAL, new_desk_button->GetState());

  // Exit overview mode and re-enter. Since we have more than one pre-existing
  // desks, their mini_views should be created upon construction of the desks
  // bar.
  ExitOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Get the new grid and the new desk_bar_view.
  overview_grid =
      overview_controller->overview_session()->GetGridWithRootWindow(
          Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(overview_grid->IsDesksBarViewActive());
  desks_bar_view = overview_grid->desks_bar_view();

  DCHECK(desks_bar_view);
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_TRUE(desks_bar_view->new_desk_button()->GetEnabled());
}

TEST_P(DesksTest, RemoveDeskWithEmptyName) {
  auto* controller = DesksController::Get();

  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());

  const auto* desks_bar_view = overview_grid->desks_bar_view();

  auto* event_generator = GetEventGenerator();

  // Create a new desk by using button with empty name.
  controller->NewDesk(DesksCreationRemovalSource::kButton);
  EXPECT_EQ(2u, controller->desks().size());

  // Close the newly created desk with the close button.
  auto* mini_view = desks_bar_view->mini_views().back().get();
  CloseDeskFromMiniView(mini_view, event_generator);
  EXPECT_EQ(1u, controller->desks().size());
}

// Tests that removing a non-active desk updates the window workspaces for
// desks restore correctly.
TEST_P(DesksTest, RemovingNonActiveDeskUpdatesWindowWorkspaces) {
  auto* controller = DesksController::Get();

  // Create two new desks.
  NewDesk();
  NewDesk();
  EXPECT_EQ(3u, controller->desks().size());

  // Create one window in each desk.
  std::vector<std::unique_ptr<aura::Window>> windows;
  for (int i = 0; i < 3; i++) {
    windows.push_back(CreateAppWindow());
    controller->SendToDeskAtIndex(windows[i].get(), i);
    EXPECT_EQ(i, windows[i]->GetProperty(aura::client::kWindowWorkspaceKey));
  }

  // Switch to the third desk.
  ActivateDesk(controller->GetDeskAtIndex(2));

  // Close the second desk.
  RemoveDesk(controller->GetDeskAtIndex(1));
  EXPECT_EQ(2u, controller->desks().size());

  // The window in the first desk remain unaffected by the second desk removal.
  EXPECT_EQ(0, windows[0]->GetProperty(aura::client::kWindowWorkspaceKey));
  // The window in the removed second desk should move to the active desk, the
  // third desk, which just becomes the second desk after the removal.
  EXPECT_EQ(1, windows[1]->GetProperty(aura::client::kWindowWorkspaceKey));
  // The window in the third desk update its workspace to index - 1.
  EXPECT_EQ(1, windows[2]->GetProperty(aura::client::kWindowWorkspaceKey));
}

// Tests that removing an active desk updates the window workspaces for desks
// restore correctly.
TEST_P(DesksTest, RemovingActiveDeskUpdatesWindowWorkspaces) {
  auto* controller = DesksController::Get();

  // Create three new desks.
  NewDesk();
  NewDesk();
  NewDesk();
  EXPECT_EQ(4u, controller->desks().size());

  // Create one window in each desk.
  std::vector<std::unique_ptr<aura::Window>> windows;
  for (int i = 0; i < 4; i++) {
    windows.push_back(CreateAppWindow());
    controller->SendToDeskAtIndex(windows[i].get(), i);
    EXPECT_EQ(i, windows[i]->GetProperty(aura::client::kWindowWorkspaceKey));
  }

  // Switch to the second desk.
  const Desk* desk_2 = controller->GetDeskAtIndex(1);
  ActivateDesk(desk_2);

  // Close the second desk.
  RemoveDesk(desk_2);
  EXPECT_EQ(3u, controller->desks().size());

  // The previous desk (first) becomes active after closing the second desk.
  EXPECT_EQ(controller->GetDeskAtIndex(0), controller->active_desk());

  // The window in the first desk remain unaffected by the second desk removal.
  EXPECT_EQ(0, windows[0]->GetProperty(aura::client::kWindowWorkspaceKey));
  // The window from the removed second desk moves to the active, first desk.
  EXPECT_EQ(0, windows[1]->GetProperty(aura::client::kWindowWorkspaceKey));
  // The desk indices of the third and forth desks are decreased by one, so
  // the workspace values of windows in those desks are reduced by one.
  EXPECT_EQ(1, windows[2]->GetProperty(aura::client::kWindowWorkspaceKey));
  EXPECT_EQ(2, windows[3]->GetProperty(aura::client::kWindowWorkspaceKey));
}

// Test that gesture taps do not reset the button state to normal when the
// button is disabled. https://crbug.com/1084241.
TEST_P(DesksTest, GestureTapOnNewDeskButton) {
  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());

  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  LeftClickOn(desks_bar_view->default_desk_button());
  auto* new_desk_button = desks_bar_view->new_desk_button();
  EXPECT_TRUE(new_desk_button->GetEnabled());

  // Gesture tap multiple times on the new desk button until it's disabled,
  // and verify the button state.
  for (size_t i = 0; i < desks_util::GetMaxNumberOfDesks() + 2; ++i) {
    GestureTapOn(new_desk_button);
    RunScheduledLayoutForAllOverviewDeskBars();
    TryScrollOverviewDeskBar(DeskBarScrollDirection::kRight,
                             /*do_left_click=*/false);
  }

  EXPECT_FALSE(new_desk_button->GetEnabled());
  EXPECT_EQ(views::Button::STATE_DISABLED, new_desk_button->GetState());
}

TEST_P(DesksTest, DeskActivation) {
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  const Desk* desk_1 = controller->GetDeskAtIndex(0);
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  auto* root = Shell::GetPrimaryRootWindow();
  EXPECT_TRUE(desk_1->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_EQ(desks_util::GetActiveDeskContainerForRoot(root),
            desk_1->GetDeskContainerForRoot(root));

  // Create three new desks, and activate one of the middle ones.
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, controller->desks().size());
  const Desk* desk_2 = controller->GetDeskAtIndex(1);
  const Desk* desk_3 = controller->GetDeskAtIndex(2);
  const Desk* desk_4 = controller->GetDeskAtIndex(3);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  ActivateDesk(desk_2);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_FALSE(desk_1->is_active());
  EXPECT_TRUE(desk_2->is_active());
  EXPECT_FALSE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());
  EXPECT_FALSE(desk_1->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_TRUE(desk_2->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(root)->IsVisible());

  // Remove the active desk, which is in the middle, activation should move to
  // the left, so desk 1 should be activated.
  EXPECT_FALSE(controller->AreDesksBeingModified());
  RemoveDesk(desk_2);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  ASSERT_EQ(3u, controller->desks().size());
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_FALSE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());
  EXPECT_TRUE(desk_1->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(root)->IsVisible());

  // Remove the active desk, it's the first one on the left, so desk_3 (on the
  // right) will be activated.
  EXPECT_FALSE(controller->AreDesksBeingModified());
  RemoveDesk(desk_1);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  ASSERT_EQ(2u, controller->desks().size());
  EXPECT_EQ(desk_3, controller->active_desk());
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());
  EXPECT_TRUE(desk_3->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(root)->IsVisible());
}

TEST_P(DesksTest, TestWindowPositioningPaused) {
  auto* controller = DesksController::Get();
  NewDesk();

  // Create two windows whose window positioning is managed.
  const auto win0_bounds = gfx::Rect{10, 20, 250, 100};
  const auto win1_bounds = gfx::Rect{50, 50, 200, 200};
  auto win0 = CreateAppWindow(win0_bounds);
  auto win1 = CreateAppWindow(win1_bounds);
  WindowState* window_state = WindowState::Get(win0.get());
  window_state->SetWindowPositionManaged(true);
  window_state = WindowState::Get(win1.get());
  window_state->SetWindowPositionManaged(true);
  EXPECT_EQ(win0_bounds, win0->GetBoundsInScreen());
  EXPECT_EQ(win1_bounds, win1->GetBoundsInScreen());

  // Moving one window to the second desk should not affect the bounds of either
  // windows.
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  controller->MoveWindowFromActiveDeskTo(
      win1.get(), desk_2, win1->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kDragAndDrop);
  EXPECT_EQ(win0_bounds, win0->GetBoundsInScreen());
  EXPECT_EQ(win1_bounds, win1->GetBoundsInScreen());

  // Removing a desk, which results in moving its windows to another desk should
  // not affect the positions of those managed windows.
  RemoveDesk(desk_2);
  EXPECT_EQ(win0_bounds, win0->GetBoundsInScreen());
  EXPECT_EQ(win1_bounds, win1->GetBoundsInScreen());
}

// This test makes sure we have coverage for that desk switch animation when run
// with multiple displays.
TEST_P(DesksTest, DeskActivationDualDisplay) {
  UpdateDisplay("700x600,400x500");

  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  const Desk* desk_1 = controller->GetDeskAtIndex(0);
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  // Create three new desks, and activate one of the middle ones.
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, controller->desks().size());
  const Desk* desk_2 = controller->GetDeskAtIndex(1);
  const Desk* desk_3 = controller->GetDeskAtIndex(2);
  const Desk* desk_4 = controller->GetDeskAtIndex(3);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  ActivateDesk(desk_2);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_FALSE(desk_1->is_active());
  EXPECT_TRUE(desk_2->is_active());
  EXPECT_FALSE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());

  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  EXPECT_FALSE(desk_1->GetDeskContainerForRoot(roots[0])->IsVisible());
  EXPECT_FALSE(desk_1->GetDeskContainerForRoot(roots[1])->IsVisible());
  EXPECT_TRUE(desk_2->GetDeskContainerForRoot(roots[0])->IsVisible());
  EXPECT_TRUE(desk_2->GetDeskContainerForRoot(roots[1])->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(roots[0])->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(roots[1])->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(roots[0])->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(roots[1])->IsVisible());
}

TEST_P(DesksTest, TransientWindows) {
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  const Desk* desk_1 = controller->GetDeskAtIndex(0);
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  // Create two windows, one is a transient child of the other.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTransientWindow(win0.get(), gfx::Rect(100, 100, 100, 100));

  EXPECT_EQ(2u, desk_1->windows().size());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win1.get()));

  auto* root = Shell::GetPrimaryRootWindow();
  EXPECT_EQ(desks_util::GetActiveDeskContainerForRoot(root),
            desks_util::GetDeskContainerForContext(win0.get()));
  EXPECT_EQ(desks_util::GetActiveDeskContainerForRoot(root),
            desks_util::GetDeskContainerForContext(win1.get()));

  // Create a new desk and activate it.
  NewDesk();
  const Desk* desk_2 = controller->GetDeskAtIndex(1);
  EXPECT_TRUE(desk_2->windows().empty());
  ActivateDesk(desk_2);
  EXPECT_FALSE(desk_1->is_active());
  EXPECT_TRUE(desk_2->is_active());

  // Create another transient child of the earlier transient child, and confirm
  // it's tracked in desk_1 (even though desk_2 is the currently active one).
  // This is because the transient parent exists in desk_1.
  auto win2 = CreateTransientWindow(win1.get(), gfx::Rect(100, 100, 50, 50));
  EXPECT_FALSE(controller->AreDesksBeingModified());
  EXPECT_EQ(3u, desk_1->windows().size());
  EXPECT_TRUE(desk_2->windows().empty());
  EXPECT_FALSE(DoesActiveDeskContainWindow(win2.get()));
  auto* desk_1_container = desk_1->GetDeskContainerForRoot(root);
  EXPECT_EQ(win0.get(), desk_1_container->children()[0]);
  EXPECT_EQ(win1.get(), desk_1_container->children()[1]);
  EXPECT_EQ(win2.get(), desk_1_container->children()[2]);

  // Remove the inactive desk 1, and expect that its windows, including
  // transient will move to desk 2.
  RemoveDesk(desk_1);
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_EQ(3u, desk_2->windows().size());
  auto* desk_2_container = desk_2->GetDeskContainerForRoot(root);
  EXPECT_EQ(win0.get(), desk_2_container->children()[0]);
  EXPECT_EQ(win1.get(), desk_2_container->children()[1]);
  EXPECT_EQ(win2.get(), desk_2_container->children()[2]);
}

TEST_P(DesksTest, WindowStackingAfterWindowMoveToAnotherDesk) {
  auto* controller = DesksController::Get();
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));

  NewDesk();
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  ActivateDesk(desk_2);

  auto win1 = CreateAppWindow(gfx::Rect(10, 10, 250, 100));
  auto win2 = CreateTransientWindow(win1.get(), gfx::Rect(100, 100, 100, 100));
  wm::ActivateWindow(win2.get());
  auto win3 = CreateAppWindow(gfx::Rect(20, 20, 250, 100));

  // Move back to desk_1, so |win0| becomes the most recent.
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  ActivateDesk(desk_1);
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  // The global MRU order should be {win0, win3, win2, win1}.
  auto* mru_tracker = Shell::Get()->mru_window_tracker();
  EXPECT_EQ(mru_tracker->BuildMruWindowList(DesksMruType::kAllDesks),
            aura::WindowTracker::WindowList({
                win0.get(),
                win3.get(),
                win2.get(),
                win1.get(),
            }));

  // Now move back to desk_2, and move its windows to desk_1. Their window
  // stacking should match their order in the MRU.
  ActivateDesk(desk_2);
  // The global MRU order is updated to be {win3, win0, win2, win1}.
  EXPECT_EQ(mru_tracker->BuildMruWindowList(DesksMruType::kAllDesks),
            aura::WindowTracker::WindowList({
                win3.get(),
                win0.get(),
                win2.get(),
                win1.get(),
            }));

  // Moving |win2| should be enough to get its transient parent |win1| moved as
  // well.
  desk_2->MoveWindowToDesk(win2.get(), desk_1, win1->GetRootWindow(),
                           /*unminimize=*/true);
  desk_2->MoveWindowToDesk(win3.get(), desk_1, win1->GetRootWindow(),
                           /*unminimize=*/true);
  EXPECT_TRUE(window_util::IsStackedBelow(win1.get(), win2.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(win2.get(), win0.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(win0.get(), win3.get()));
}

TEST_P(DesksTest, TransientModalChildren) {
  auto* controller = DesksController::Get();
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  Desk* desk_3 = controller->GetDeskAtIndex(2);
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  // Create three windows, one of them is a modal transient child.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTransientModalChildWindow(win0.get());
  EXPECT_EQ(win1.get(), ::wm::GetModalTransient(win0.get()));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 200, 100));
  ASSERT_EQ(3u, desk_1->windows().size());
  auto* root = Shell::GetPrimaryRootWindow();
  auto* desk_1_container = desk_1->GetDeskContainerForRoot(root);
  EXPECT_EQ(win0.get(), desk_1_container->children()[0]);
  EXPECT_EQ(win1.get(), desk_1_container->children()[1]);
  EXPECT_EQ(win2.get(), desk_1_container->children()[2]);

  // Remove desk_1, and expect that all its windows (including the transient
  // modal child and its parent) are moved to desk_2, and that their z-order
  // within the container is preserved.
  RemoveDesk(desk_1);
  EXPECT_EQ(desk_2, controller->active_desk());
  ASSERT_EQ(3u, desk_2->windows().size());
  auto* desk_2_container = desk_2->GetDeskContainerForRoot(root);
  EXPECT_EQ(win0.get(), desk_2_container->children()[0]);
  EXPECT_EQ(win1.get(), desk_2_container->children()[1]);
  EXPECT_EQ(win2.get(), desk_2_container->children()[2]);
  EXPECT_EQ(win1.get(), ::wm::GetModalTransient(win0.get()));

  // Move only the modal child window to desk_3, and expect that its parent will
  // move along with it, and their z-order is preserved.
  controller->MoveWindowFromActiveDeskTo(
      win1.get(), desk_3, win1->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kDragAndDrop);
  ASSERT_EQ(1u, desk_2->windows().size());
  ASSERT_EQ(2u, desk_3->windows().size());
  EXPECT_EQ(win2.get(), desk_2_container->children()[0]);
  auto* desk_3_container = desk_3->GetDeskContainerForRoot(root);
  EXPECT_EQ(win0.get(), desk_3_container->children()[0]);
  EXPECT_EQ(win1.get(), desk_3_container->children()[1]);

  // The modality remains the same.
  ActivateDesk(desk_3);
  EXPECT_EQ(win1.get(), ::wm::GetModalTransient(win0.get()));
}

TEST_P(DesksTest, WindowActivation) {
  // Create three windows.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto win2 = CreateAppWindow(gfx::Rect(100, 100, 100, 100));

  EXPECT_TRUE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win2.get()));

  // Activate win0 and expects that it remains activated until we switch desks.
  wm::ActivateWindow(win0.get());

  // Create a new desk and activate it. Expect it's not tracking any windows
  // yet.
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  const Desk* desk_2 = controller->GetDeskAtIndex(1);
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_EQ(3u, desk_1->windows().size());
  EXPECT_TRUE(desk_2->windows().empty());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  // Activate the newly-added desk. Expect that the tracked windows per each
  // desk will remain the same.
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_EQ(3u, desk_1->windows().size());
  EXPECT_TRUE(desk_2->windows().empty());

  // `desk_2` has no windows, so now no window should be active. However,
  // windows on `desk_1` are activateable.
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());
  EXPECT_TRUE(wm::CanActivateWindow(win0.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win1.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win2.get()));

  // Create two new windows, they should now go to desk_2.
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  auto win4 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));
  wm::ActivateWindow(win3.get());
  EXPECT_EQ(2u, desk_2->windows().size());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win3.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win4.get()));
  EXPECT_FALSE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_FALSE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_FALSE(DoesActiveDeskContainWindow(win2.get()));
  EXPECT_EQ(win3.get(), window_util::GetActiveWindow());

  // Delete `win0` and expect that `desk_1`'s windows will be updated.
  win0.reset();
  EXPECT_EQ(2u, desk_1->windows().size());
  EXPECT_EQ(2u, desk_2->windows().size());
  // No change in the activation.
  EXPECT_EQ(win3.get(), window_util::GetActiveWindow());

  // Switch back to `desk_1`. Now we can activate its windows.
  ActivateDesk(desk_1);
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(wm::CanActivateWindow(win1.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win2.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win3.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win4.get()));

  // After `win0` has been deleted, `win2` is next on the MRU list.
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());

  // Remove `desk_2` and expect that its windows will be moved to the active
  // desk.
  TestDeskObserver observer;
  desk_1->AddObserver(&observer);
  RemoveDesk(desk_2);
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_EQ(4u, desk_1->windows().size());
  // Even though two new windows have been added to desk_1, observers should be
  // notified only once.
  EXPECT_EQ(1, observer.notify_counts());
  desk_1->RemoveObserver(&observer);
  EXPECT_TRUE(DoesActiveDeskContainWindow(win3.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win4.get()));

  // `desk_2`'s windows moved to `desk_1`, but that should not change the
  // already active window.
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());

  // Moved windows can still be activated.
  EXPECT_TRUE(wm::CanActivateWindow(win3.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win4.get()));
}

TEST_P(DesksTest, ActivateDeskFromOverview) {
  auto* controller = DesksController::Get();
  auto* overview_controller = OverviewController::Get();
  auto* event_generator = GetEventGenerator();

  // Create three desks other than the default initial desk.
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, controller->desks().size());

  // Create two windows on desk_1.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win1.get());
  EXPECT_EQ(win1.get(), window_util::GetActiveWindow());

  // Test that left-click on desk mini view activates the specified desk.
  {
    // Enter overview mode, and expect the desk bar is shown with exactly four
    // desks mini views, and there are exactly two windows in the overview mode
    // grid.
    EnterOverview();
    EXPECT_TRUE(overview_controller->InOverviewSession());
    const auto* overview_grid =
        GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
    const auto* desks_bar_view = overview_grid->desks_bar_view();
    ASSERT_TRUE(desks_bar_view);
    ASSERT_EQ(4u, desks_bar_view->mini_views().size());
    EXPECT_EQ(2u, overview_grid->item_list().size());

    // Activate desk_4 (last one on the right) by clicking on its mini view.
    const Desk* desk_4 = controller->GetDeskAtIndex(3);
    EXPECT_EQ(0, controller->GetActiveDeskIndex());
    auto* mini_view = desks_bar_view->mini_views().back().get();
    EXPECT_EQ(desk_4, mini_view->desk());
    EXPECT_FALSE(GetDeskActionVisibilityForMiniView(mini_view));
    DeskSwitchAnimationWaiter waiter;
    LeftClickOn(mini_view);
    waiter.Wait();

    // Expect that desk_4 is now active, and overview mode exited.
    EXPECT_EQ(3, controller->GetActiveDeskIndex());
    EXPECT_FALSE(overview_controller->InOverviewSession());
    // Exiting overview mode should not restore focus to a window on a
    // now-inactive desk. Run a loop since the overview session is destroyed
    // async and until that happens, focus will be on the dummy
    // "OverviewModeFocusedWidget".
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(nullptr, window_util::GetActiveWindow());

    // Create one window in desk_4 and enter overview mode. Expect the grid is
    // showing exactly one window.
    auto win2 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
    wm::ActivateWindow(win2.get());
    EnterOverview();
    EXPECT_TRUE(overview_controller->InOverviewSession());
    overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
    EXPECT_EQ(1u, overview_grid->item_list().size());

    // When exiting overview mode without changing desks, the focus should be
    // restored to the same window.
    ExitOverview();
    EXPECT_FALSE(overview_controller->InOverviewSession());
    // Run a loop since the overview session is destroyed async and until that
    // happens, focus will be on the dummy "OverviewModeFocusedWidget".
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(win2.get(), window_util::GetActiveWindow());
  }

  // Test double click on desk mini view does not crash.
  {
    EnterOverview();
    const auto* overview_grid =
        GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
    DeskSwitchAnimationWaiter waiter;
    DoubleClickOnView(overview_grid->desks_bar_view()->mini_views().front(),
                      event_generator);
    waiter.Wait();
    EXPECT_EQ(0, controller->GetActiveDeskIndex());
    EXPECT_FALSE(overview_controller->InOverviewSession());
  }

  // Test that using ChromeVox on a desk mini view does not crash.
  {
    EnterOverview();
    const auto* overview_grid =
        GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
    DeskSwitchAnimationWaiter waiter;
    SendAccessibleActionToView(
        overview_grid->desks_bar_view()->mini_views()[1]->desk_preview(),
        ax::mojom::Action::kDoDefault);
    waiter.Wait();
    EXPECT_EQ(1, controller->GetActiveDeskIndex());
    EXPECT_FALSE(overview_controller->InOverviewSession());
  }
}

// This test makes sure we have coverage for that desk switch animation when run
// with multiple displays while overview mode is active.
TEST_P(DesksTest, ActivateDeskFromOverviewDualDisplay) {
  UpdateDisplay("700x600,400x500");

  auto* controller = DesksController::Get();

  // Create three desks other than the default initial desk.
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, controller->desks().size());

  // Enter overview mode.
  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  // Use secondary display grid.
  const auto* overview_grid = GetOverviewGridForRoot(roots[1]);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(4u, desks_bar_view->mini_views().size());

  // Activate desk_4 (last one on the right) by clicking on its mini view.
  const Desk* desk_4 = controller->GetDeskAtIndex(3);
  EXPECT_FALSE(desk_4->is_active());
  const auto* mini_view = desks_bar_view->mini_views().back().get();
  const gfx::Point mini_view_center =
      mini_view->GetBoundsInScreen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(mini_view_center);
  DeskSwitchAnimationWaiter waiter;
  event_generator->ClickLeftButton();
  waiter.Wait();

  // Expect that desk_4 is now active, and overview mode exited.
  EXPECT_TRUE(desk_4->is_active());
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

TEST_P(DesksTest, RemoveInactiveDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create three desks other than the default initial desk.
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, controller->desks().size());

  // Create 3 windows on desk_1.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto win2 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  auto* mru_tracker = Shell::Get()->mru_window_tracker();
  EXPECT_THAT(mru_tracker->BuildMruWindowList(DesksMruType::kActiveDesk),
              ElementsAre(win0.get(), win2.get(), win1.get()));

  // Active desk_4 and enter overview mode, and add a single window.
  Desk* desk_4 = controller->GetDeskAtIndex(3);
  ActivateDesk(desk_4);
  auto* overview_controller = OverviewController::Get();
  auto win3 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->item_list().size());

  // Remove desk_1 using the close button on its mini view. desk_1 is currently
  // inactive. Its windows should be moved to desk_4 and added to the overview
  // grid in the MRU order (win0, win2, and win1) at the end after desk_4's
  // existing window (win3).
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(4u, desks_bar_view->mini_views().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  auto* mini_view = desks_bar_view->mini_views().front().get();
  EXPECT_EQ(desk_1, mini_view->desk());

  // Setup observers of both the active and inactive desks to make sure
  // refreshing the mini_view is requested only *once* for the active desk, and
  // never for the to-be-removed inactive desk.
  TestDeskObserver desk_4_observer;
  desk_4->AddObserver(&desk_4_observer);
  TestDeskObserver desk_1_observer;
  desk_1->AddObserver(&desk_1_observer);

  if (features::IsSavedDeskUiRevampEnabled()) {
    views::MenuItemView* menu_item =
        DesksTestApi::OpenDeskContextMenuAndGetMenuItem(
            Shell::GetPrimaryRootWindow(), DeskBarViewBase::Type::kOverview,
            /*index=*/0u, DeskActionContextMenu::CommandId::kCombineDesks);
    LeftClickOn(menu_item);
  } else {
    // This will combine `desk_1` into `desk_4`.
    CloseDeskFromMiniView(mini_view, GetEventGenerator());
  }

  EXPECT_EQ(0, desk_1_observer.notify_counts());
  EXPECT_EQ(1, desk_4_observer.notify_counts());

  ASSERT_EQ(3u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ASSERT_EQ(4u, overview_grid->item_list().size());
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win0.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win1.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win2.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win3.get()));
  // Expected order of items: win3, win0, win2, win1.
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win3.get()),
            overview_grid->item_list()[0].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win0.get()),
            overview_grid->item_list()[1].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win2.get()),
            overview_grid->item_list()[2].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win1.get()),
            overview_grid->item_list()[3].get());

  // Make sure overview mode remains active.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Removing a desk with no windows should not result in any new mini_views
  // updates.
  mini_view = desks_bar_view->mini_views().front();
  EXPECT_TRUE(mini_view->desk()->windows().empty());
  CloseDeskFromMiniView(mini_view, GetEventGenerator());
  EXPECT_EQ(1, desk_4_observer.notify_counts());

  // Exiting overview mode should not cause any mini_views refreshes, since the
  // destroyed overview-specific windows do not show up in the mini_view.
  ExitOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(1, desk_4_observer.notify_counts());
  desk_4->RemoveObserver(&desk_4_observer);

  // Verify that the stacking order is correct (top-most comes last, and
  // top-most is the same as MRU).
  EXPECT_THAT(mru_tracker->BuildMruWindowList(DesksMruType::kActiveDesk),
              ElementsAre(win3.get(), win0.get(), win2.get(), win1.get()));
  EXPECT_THAT(desk_4->GetDeskContainerForRoot(Shell::GetPrimaryRootWindow())
                  ->children(),
              ElementsAre(win1.get(), win2.get(), win0.get(), win3.get()));
}

TEST_P(DesksTest, RemoveActiveDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create one desk other than the default initial desk.
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  // Create two windows on desk_1.
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  // Activate desk_2 and create one more window.
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  ActivateDesk(desk_2);
  auto win2 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto win3 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win2.get());
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());

  // The MRU across all desks is now {win2, win3, win0, win1}.
  auto* mru_tracker = Shell::Get()->mru_window_tracker();
  EXPECT_THAT(mru_tracker->BuildMruWindowList(DesksMruType::kAllDesks),
              ElementsAre(win2.get(), win3.get(), win0.get(), win1.get()));

  // Enter overview mode, and remove desk_2 from its mini-view close button.
  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(2u, overview_grid->item_list().size());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  auto* mini_view = desks_bar_view->mini_views().back().get();
  EXPECT_EQ(desk_2, mini_view->desk());

  // Setup observers of both the active and inactive desks to make sure
  // refreshing the mini_view is requested only *once* for the soon-to-be active
  // desk_1, and never for the to-be-removed currently active desk_2.
  TestDeskObserver desk_1_observer;
  desk_1->AddObserver(&desk_1_observer);
  TestDeskObserver desk_2_observer;
  desk_2->AddObserver(&desk_2_observer);

  if (features::IsSavedDeskUiRevampEnabled()) {
    views::MenuItemView* menu_item =
        DesksTestApi::OpenDeskContextMenuAndGetMenuItem(
            Shell::GetPrimaryRootWindow(), DeskBarViewBase::Type::kOverview,
            /*index=*/1u, DeskActionContextMenu::CommandId::kCombineDesks);
    LeftClickOn(menu_item);
  } else {
    // This will combine `desk_2` into `desk_1`.
    CloseDeskFromMiniView(mini_view, GetEventGenerator());
  }

  EXPECT_EQ(1, desk_1_observer.notify_counts());
  EXPECT_EQ(0, desk_2_observer.notify_counts());

  // Make sure that the desks_bar_view window is still visible, i.e. it moved to
  // the newly-activated desk's container.
  EXPECT_TRUE(desks_bar_view->GetWidget()->IsVisible());
  EXPECT_TRUE(DoesActiveDeskContainWindow(
      desks_bar_view->GetWidget()->GetNativeWindow()));

  // desk_1 will become active, and windows from desk_2 will move to desk_1 such
  // that they become last in MRU order, and therefore appended at the end of
  // the overview grid. Desks bar should go back to zero state since there is a
  // single desk after removing.
  ASSERT_EQ(1u, controller->desks().size());
  ASSERT_EQ(1u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(4u, overview_grid->item_list().size());
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win0.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win1.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win2.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win3.get()));

  // The new MRU order is {win0, win1, win2, win3}.
  EXPECT_THAT(mru_tracker->BuildMruWindowList(DesksMruType::kActiveDesk),
              ElementsAre(win0.get(), win1.get(), win2.get(), win3.get()));
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win0.get()),
            overview_grid->item_list()[0].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win1.get()),
            overview_grid->item_list()[1].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win2.get()),
            overview_grid->item_list()[2].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win3.get()),
            overview_grid->item_list()[3].get());

  // Make sure overview mode remains active.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Exiting overview mode should not cause any mini_views refreshes, since the
  // destroyed overview-specific windows do not show up in the mini_view.
  ExitOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(1, desk_1_observer.notify_counts());
  desk_1->RemoveObserver(&desk_1_observer);
}

// Test that there is no crash when closing active desk with a all desk window
// right after creating a desk with empty name.
// Regression test for b/266147233.
TEST_P(DesksTest,
       RemoveActiveDeskWithAllDeskWindowFromOverviewWithNewDeskOfEmptyName) {
  auto* controller = DesksController::Get();

  NewDesk();
  auto win1 = CreateAppWindow();
  views::Widget::GetWidgetForNativeWindow(win1.get())
      ->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(desks_util::IsWindowVisibleOnAllWorkspaces(win1.get()));

  EnterOverview();
  LeftClickOn(GetPrimaryRootDesksBarView()->new_desk_button());
  ASSERT_EQ(3u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  Desk* desk_3 = controller->GetDeskAtIndex(2);
  ASSERT_TRUE(desk_1->is_active());

  controller->RemoveDesk(desk_1, DesksCreationRemovalSource::kButton,
                         DeskCloseType::kCloseAllWindowsAndWait);
  ASSERT_EQ(desk_2, controller->GetTargetActiveDesk());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  controller->RemoveDesk(desk_2, DesksCreationRemovalSource::kButton,
                         DeskCloseType::kCloseAllWindowsAndWait);
  ASSERT_EQ(desk_3, controller->GetTargetActiveDesk());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  // The desk bar doesn't switch to zero state.
  EXPECT_EQ(1u, GetPrimaryRootDesksBarView()->mini_views().size());
}

TEST_P(DesksTest, ActivateActiveDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create one more desk other than the default initial desk, so the desks bar
  // shows up in overview mode.
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  // Enter overview mode, and click on `desk_1`'s mini_view, and expect that
  // overview mode exits since this is the already active desk.
  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  const Desk* desk_1 = controller->GetDeskAtIndex(0);
  const auto* mini_view = desks_bar_view->mini_views().front().get();
  LeftClickOn(mini_view);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_EQ(desk_1, controller->active_desk());
}

TEST_P(DesksTest, MinimizedWindow) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_1 = controller->GetDeskAtIndex(0);
  const Desk* desk_2 = controller->GetDeskAtIndex(1);

  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  auto* window_state = WindowState::Get(win0.get());
  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  // Minimized windows on the active desk show up in the MRU list...
  EXPECT_EQ(1u, Shell::Get()
                    ->mru_window_tracker()
                    ->BuildMruWindowList(kActiveDesk)
                    .size());

  ActivateDesk(desk_2);

  // ... But they don't once their desk is inactive.
  EXPECT_TRUE(Shell::Get()
                  ->mru_window_tracker()
                  ->BuildMruWindowList(kActiveDesk)
                  .empty());

  // Switching back to their desk should neither activate them nor unminimize
  // them.
  ActivateDesk(desk_1);
  EXPECT_TRUE(window_state->IsMinimized());
  EXPECT_NE(win0.get(), window_util::GetActiveWindow());
}

// Tests that the app list stays open when switching desks. Regression test for
// http://crbug.com/1138982.
TEST_P(DesksTest, AppListStaysOpenInClamshell) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  // Create one app window on each desk.
  auto win0 = CreateAppWindow(gfx::Rect(400, 400));
  ActivateDesk(controller->GetDeskAtIndex(1));
  auto win1 = CreateAppWindow(gfx::Rect(400, 400));

  // Open the app list.
  auto* app_list_controller = Shell::Get()->app_list_controller();
  app_list_controller->ShowAppList(AppListShowSource::kSearchKey);
  ASSERT_TRUE(app_list_controller->IsVisible());

  // Switch back to desk 1. Test that the app list is still open.
  ActivateDesk(controller->GetDeskAtIndex(0));
  EXPECT_TRUE(app_list_controller->IsVisible());
}

// Tests that the app list correctly loses focus in tablet mode when switching
// desks. Regression test for https://crbug.com/1206030.
TEST_P(DesksTest, AppListActivationInTablet) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  // Create one app window on desk 1.
  auto window = CreateAppWindow(gfx::Rect(400, 400));
  ASSERT_EQ(window.get(), window_util::GetActiveWindow());

  // Enter tablet mode and switch to desk 2. Verify the app list has activation
  // as there are no app windows.
  TabletModeControllerTestApi().EnterTabletMode();
  ActivateDesk(controller->GetDeskAtIndex(1));
  auto* app_list_controller = Shell::Get()->app_list_controller();
  ASSERT_EQ(app_list_controller->GetWindow(), window_util::GetActiveWindow());

  // Switch back to desk 1. `window` should have activation now.
  ActivateDesk(controller->GetDeskAtIndex(0));
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
}

TEST_P(DesksTest, DragWindowToDesk) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_1 = controller->GetDeskAtIndex(0);
  const Desk* desk_2 = controller->GetDeskAtIndex(1);

  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 200, 150));
  wm::ActivateWindow(win1.get());
  EXPECT_EQ(win1.get(), window_util::GetActiveWindow());

  ui::Shadow* shadow = ::wm::ShadowController::GetShadowForWindow(win1.get());
  ASSERT_TRUE(shadow);
  ASSERT_TRUE(shadow->layer());
  EXPECT_TRUE(shadow->layer()->GetTargetVisibility());

  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(2u, overview_grid->GetNumWindows());

  // While in overview mode, the window's shadow is hidden.
  EXPECT_FALSE(shadow->layer()->GetTargetVisibility());

  auto* overview_session = overview_controller->overview_session();
  auto* overview_item = overview_session->GetOverviewItemForWindow(win1.get());
  ASSERT_TRUE(overview_item);
  const gfx::RectF target_bounds_before_drag = overview_item->target_bounds();

  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  auto* desk_1_mini_view = desks_bar_view->mini_views()[0].get();
  EXPECT_EQ(desk_1, desk_1_mini_view->desk());
  // Drag it and drop it on its same desk's mini_view. Nothing happens, it
  // should be returned back to its original target bounds.
  auto* event_generator = GetEventGenerator();
  DragItemToPoint(overview_item,
                  desk_1_mini_view->GetBoundsInScreen().CenterPoint(),
                  event_generator, GetParam().use_touch_gestures);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(2u, overview_grid->GetNumWindows());
  EXPECT_EQ(target_bounds_before_drag, overview_item->target_bounds());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win1.get()));

  // Now drag it to desk_2's mini_view. The overview grid should now have only
  // `win2`, and `win1` should move to desk_2.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  EXPECT_EQ(desk_2, desk_2_mini_view->desk());
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  event_generator, GetParam().use_touch_gestures);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(1u, overview_grid->GetNumWindows());
  EXPECT_FALSE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_TRUE(base::Contains(desk_2->windows(), win1.get()));
  EXPECT_FALSE(overview_grid->drop_target());

  // After dragging an item outside of overview to another desk, the focus
  // should not be given to another window in overview, and should remain on the
  // OverviewModeFocusedWidget.
  EXPECT_EQ(overview_session->GetOverviewFocusWindow(),
            window_util::GetActiveWindow());

  // It is possible to select the remaining window which will activate it and
  // exit overview.
  overview_item = overview_session->GetOverviewItemForWindow(win2.get());
  ASSERT_TRUE(overview_item);
  const auto window_center =
      gfx::ToFlooredPoint(overview_item->target_bounds().CenterPoint());
  if (GetParam().use_touch_gestures) {
    event_generator->GestureTapAt(window_center);
  } else {
    event_generator->MoveMouseTo(window_center);
    event_generator->ClickLeftButton();
  }
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());

  // After the window is dropped onto another desk, its shadow should be
  // restored properly.
  EXPECT_TRUE(shadow->layer()->GetTargetVisibility());
}

// Tests that theme change during drag to close does not trigger any crashes.
// Regression test for b/270171802.
TEST_P(DesksTest, DragWindowToCloseWithThemeChange) {
  // Create two windows.
  auto win1 = CreateAppWindow();
  auto win2 = CreateAppWindow();

  // Enter overview.
  EnterOverview();
  auto* overview_item = Shell::Get()
                            ->overview_controller()
                            ->overview_session()
                            ->GetOverviewItemForWindow(win1.get());
  ASSERT_TRUE(overview_item);

  // Drag `win1` to close and simulate multiple theme changes at the same time.
  // There should not be any crashes.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(
      gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
  event_generator->PressTouch();
  overview_item->item_widget()->ThemeChanged();
  event_generator->MoveTouchBy(0, -50);
  overview_item->item_widget()->ThemeChanged();
  event_generator->MoveTouchBy(0, -200);
  overview_item->item_widget()->ThemeChanged();
  event_generator->ReleaseTouch();
  overview_item->item_widget()->ThemeChanged();
}

TEST_P(DesksTest, DragMinimizedWindowToDesk) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_2 = controller->GetDeskAtIndex(1);

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Minimize the window before entering Overview Mode.
  auto* window_state = WindowState::Get(window.get());
  window_state->Minimize();
  ASSERT_TRUE(window_state->IsMinimized());

  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());

  // Drag the window to desk_2's mini_view and activate desk_2. Expect that the
  // window will be in an unminimized state and all its visibility and layer
  // opacity attributes are correct.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  EXPECT_EQ(desk_2, desk_2_mini_view->desk());
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator(), GetParam().use_touch_gestures);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_grid->empty());
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));
  EXPECT_FALSE(overview_grid->drop_target());
  DeskSwitchAnimationWaiter waiter;
  LeftClickOn(desk_2_mini_view);
  waiter.Wait();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(desk_2->is_active());

  EXPECT_FALSE(window_state->IsMinimized());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  EXPECT_EQ(1.f, window->layer()->GetTargetOpacity());
}

TEST_P(DesksTest, DragAllOverviewWindowsToOtherDesksNotEndOverview) {
  NewDesk();
  ASSERT_EQ(2u, DesksController::Get()->desks().size());
  auto win = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto* overview_controller = OverviewController::Get();
  ASSERT_TRUE(EnterOverview());
  auto* overview_session = overview_controller->overview_session();
  DragItemToPoint(overview_session->GetOverviewItemForWindow(win.get()),
                  GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
                      ->desks_bar_view()
                      ->mini_views()[1]
                      ->GetBoundsInScreen()
                      .CenterPoint(),
                  GetEventGenerator(), GetParam().use_touch_gestures);
  EXPECT_FALSE(DoesActiveDeskContainWindow(win.get()));
  ASSERT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_session->IsEmpty());
}

TEST_P(DesksTest, DragWindowToNonMiniViewPoints) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->GetNumWindows());

  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const gfx::RectF target_bounds_before_drag = overview_item->target_bounds();

  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  // Drag it and drop it on the center of the bottom of the display. Also,
  // nothing should happen.
  DragItemToPoint(overview_item,
                  window->GetRootWindow()->GetBoundsInScreen().bottom_center(),
                  GetEventGenerator(), GetParam().use_touch_gestures);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(1u, overview_grid->GetNumWindows());
  EXPECT_EQ(target_bounds_before_drag, overview_item->target_bounds());
  EXPECT_TRUE(DoesActiveDeskContainWindow(window.get()));
}

// Tests that dragging and dropping window to new desk while desks bar view is
// at zero state.
TEST_P(DesksTest, DragWindowAtZeroState) {
  auto* controller = DesksController::Get();
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));

  ASSERT_EQ(1u, controller->desks().size());
  auto* overview_controller = OverviewController::Get();
  EnterOverview();

  ASSERT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  // Since we only have one desk, there should be 0 desk mini view and the zero
  // state default desk button and new desk button should be visible.
  ASSERT_EQ(0u, desks_bar_view->mini_views().size());
  auto* default_desk_button = desks_bar_view->default_desk_button();
  auto* new_desk_button = desks_bar_view->new_desk_button();
  EXPECT_TRUE(default_desk_button->GetVisible());
  VerifyZeroStateNewDeskButtonVisibility(desks_bar_view, true);
  VerifyExpandedStateNewDeskButtonVisibility(desks_bar_view, false);

  auto* overview_item1 = overview_grid->GetOverviewItemContaining(win1.get());
  auto* event_generator = GetEventGenerator();

  // Start dragging `overview_item1` without dropping it. Make sure the square
  // length between `overview_item1` and `zero_state_new_desk_button` is longer
  // than `kExpandDesksBarThreshold`. Verify that the desks bar is still at zero
  // state in this use case.
  DragItemToPoint(
      overview_item1,
      gfx::ToRoundedPoint(overview_item1->target_bounds().CenterPoint() +
                          gfx::Vector2d(100, -100)),
      event_generator,
      /*by_touch_gestures=*/false, /*drop=*/false);
  // The desk bar will expand immediately at the beginning of the drag.
  EXPECT_FALSE(default_desk_button->GetVisible());
  VerifyZeroStateNewDeskButtonVisibility(desks_bar_view, false);
  VerifyExpandedStateNewDeskButtonVisibility(desks_bar_view, true);

  const gfx::Point new_desk_button_center_point =
      new_desk_button->GetBoundsInScreen().CenterPoint();
  // The new desk button will be scaled up as a drop target for `overview_item1`
  // when `overview_item1` it's being hovered on top it over 500 milliseconds.
  // Drag `overview_item1` to hover on the new desk button, and wait for 200
  // milliseconds, verify that new desk button is still at the expanded state
  // and the new desk label is not shown.
  const gfx::Point new_desk_button_bottom_left =
      new_desk_button->GetBoundsInScreen().bottom_left();
  DragItemToPoint(overview_item1,
                  new_desk_button_bottom_left + gfx::Vector2d(10, -10),
                  event_generator, /*by_touch_gestures=*/false,
                  /*drop=*/false);
  WaitForMilliseconds(200);
  EXPECT_EQ(DeskIconButton::State::kExpanded, new_desk_button->state());
  EXPECT_FALSE(IsLazyInitViewVisible(desks_bar_view->new_desk_button_label()));
  // Now fire the timer directly to skip the wait time. Verify that the new desk
  // button is scaled up and at the active state and the new desk label is shown
  // now.
  overview_controller->overview_session()
      ->window_drag_controller()
      ->new_desk_button_scale_up_timer_for_test()
      ->FireNow();
  RunScheduledLayoutForAllOverviewDeskBars();
  EXPECT_EQ(DeskIconButton::State::kActive, new_desk_button->state());
  EXPECT_TRUE(IsLazyInitViewVisible(desks_bar_view->new_desk_button_label()));

  // Keep dragging `overview_item1` to the center of the new desk button to make
  // it a drop target.
  DragItemToPoint(overview_item1,
                  new_desk_button_center_point + gfx::Vector2d(10, 10),
                  event_generator, /*by_touch_gestures=*/false,
                  /*drop=*/false);

  // Now drop |overview_item1|, a new desk which contains |win1| will be
  // created.
  event_generator->ReleaseLeftButton();
  RunScheduledLayoutForAllOverviewDeskBars();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(2u, desks_bar_view->mini_views().size());
  EXPECT_EQ(2u, controller->desks().size());
  EXPECT_TRUE(
      base::Contains(controller->GetDeskAtIndex(1)->windows(), win1.get()));
  // The active desk should still be the first desk, even though a new desk
  // is created.
  EXPECT_EQ(DesksController::Get()->active_desk(),
            controller->GetDeskAtIndex(0));
  // |overview_grid| should have size equals to 0 now, since |overview_item1|
  // havs been moved to a new desk.
  EXPECT_EQ(0u, overview_grid->GetNumWindows());
}

// Tests that dragging a window at zero state but without dropping it on the new
// desk button, the window being dragged will fall back to the existing desk and
// no new desk should be created.
TEST_P(DesksTest, DragWindowAtZeroStateWithoutDroppingItOnTheNewDesk) {
  auto* controller = DesksController::Get();
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));

  ASSERT_EQ(1u, controller->desks().size());
  auto* overview_controller = OverviewController::Get();
  EnterOverview();

  ASSERT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  // Make sure the desks bar is at zero state in the beginning.
  auto* default_desk_button = desks_bar_view->default_desk_button();
  auto* new_desk_button = desks_bar_view->new_desk_button();
  EXPECT_TRUE(default_desk_button->GetVisible());
  VerifyZeroStateNewDeskButtonVisibility(desks_bar_view, true);
  VerifyExpandedStateNewDeskButtonVisibility(desks_bar_view, false);

  auto* overview_item1 = overview_grid->GetOverviewItemContaining(win1.get());
  auto* event_generator = GetEventGenerator();

  // Start dragging `overview_item1`, verify desks bar expands immediately and
  // the new desk button is transformed to the expanded state;
  DragItemToPoint(
      overview_item1,
      gfx::ToRoundedPoint(overview_item1->target_bounds().CenterPoint() +
                          gfx::Vector2d(100, -100)),
      event_generator,
      /*by_touch_gestures=*/false, /*drop=*/false);
  EXPECT_FALSE(desks_bar_view->IsZeroState());
  EXPECT_EQ(DeskIconButton::State::kExpanded, new_desk_button->state());
  EXPECT_FALSE(IsLazyInitViewVisible(desks_bar_view->new_desk_button_label()));

  // Keep dragging `overview_item1` to hover on the new desk button, immediately
  // fire the time to skip to wait time. Verify that new desk button is
  // transformed to the active state and the new desk label is shown now.
  const gfx::Point new_desk_button_center_point =
      new_desk_button->GetBoundsInScreen().CenterPoint();
  DragItemToPoint(overview_item1,
                  gfx::Point(new_desk_button_center_point.x() + 10,
                             new_desk_button_center_point.y() + 10),
                  event_generator, /*by_touch_gestures=*/false,
                  /*drop=*/false);
  overview_controller->overview_session()
      ->window_drag_controller()
      ->new_desk_button_scale_up_timer_for_test()
      ->FireNow();
  RunScheduledLayoutForAllOverviewDeskBars();
  EXPECT_EQ(DeskIconButton::State::kActive, new_desk_button->state());
  EXPECT_TRUE(IsLazyInitViewVisible(desks_bar_view->new_desk_button_label()));

  // Now keep dragging `overview_item1` and make it not able to be dropped on
  // the new desk, then drop it. Check that `overview_item1` is dropped back to
  // the existing desk, there's no new desk created, and desks bar is
  // transformed back to the zero state.
  const gfx::Point expanded_new_desk_button_center_point =
      new_desk_button->GetBoundsInScreen().CenterPoint();
  DragItemToPoint(overview_item1,
                  gfx::Point(expanded_new_desk_button_center_point.x() + 200,
                             expanded_new_desk_button_center_point.y() + 200),
                  event_generator, /*by_touch_gestures=*/false, /*drop=*/true);
  RunScheduledLayoutForAllOverviewDeskBars();
  // The desk bar never goes back to the zero state from the expanded state even
  // these's only one desk. Verify that the new desk label is invisible now.
  EXPECT_FALSE(desks_bar_view->IsZeroState());
  EXPECT_FALSE(IsLazyInitViewVisible(desks_bar_view->new_desk_button_label()));
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_TRUE(
      base::Contains(controller->GetDeskAtIndex(0)->windows(), win1.get()));
}

// Tests that dragging and dropping window to new desk while desks bar view is
// at expanded state.
TEST_P(DesksTest, DragWindowAtExpandedState) {
  auto* controller = DesksController::Get();
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  NewDesk();

  ASSERT_EQ(2u, controller->desks().size());
  auto* overview_controller = OverviewController::Get();
  EnterOverview();

  ASSERT_TRUE(overview_controller->InOverviewSession());
  const auto* desks_bar_view =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  auto* new_desk_button = desks_bar_view->new_desk_button();
  VerifyExpandedStateNewDeskButtonVisibility(desks_bar_view, true);

  auto* event_generator = GetEventGenerator();
  // Drag `overview_item1` on that new desk button, and fire the timer
  // immediately and then drops it. A new desk which contains `win1` will be
  // created.
  DragItemToPoint(
      overview_controller->overview_session()->GetOverviewItemForWindow(
          win1.get()),
      new_desk_button->GetBoundsInScreen().CenterPoint(), GetEventGenerator(),
      /*by_touch_gestures=*/false,
      /*drop=*/false);
  overview_controller->overview_session()
      ->window_drag_controller()
      ->new_desk_button_scale_up_timer_for_test()
      ->FireNow();
  event_generator->ReleaseLeftButton();

  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(3u, desks_bar_view->mini_views().size());
  EXPECT_EQ(3u, controller->desks().size());
  EXPECT_TRUE(
      base::Contains(controller->GetDeskAtIndex(2)->windows(), win1.get()));
}

// Tests that dragging and dropping a window on the new desk button does not
// create a new desk if we are already at the maximum number of desks.
TEST_P(DesksTest, DragWindowAtMaximumDesksState) {
  // Set a display mode that forces vertical layout of split view drag
  // indicators. This is so that we are able to drop an overview item on the
  // "new desk" button even if it's right up to the edge.
  UpdateDisplay("800x801");

  auto* controller = DesksController::Get();
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  while (controller->desks().size() < desks_util::GetMaxNumberOfDesks()) {
    NewDesk();
  }

  ASSERT_EQ(desks_util::GetMaxNumberOfDesks(), controller->desks().size());
  auto* overview_controller = OverviewController::Get();
  EnterOverview();

  ASSERT_TRUE(overview_controller->InOverviewSession());
  const auto* desks_bar_view =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  auto* event_generator = GetEventGenerator();
  for (int i = 0; i != 3; ++i) {
    TryScrollOverviewDeskBar(DeskBarScrollDirection::kRight);
  }

  // Drag and drop the overview to the new desk button. Since we already have
  // maximum number of desks, this won't create a new desk, and the dragged
  // window will fall back to the desk where it's from. Dragging here is not
  // done using `DragItemToPoint`. This is because once a drag has been
  // initiated, the split view drag indicators will show and shift the position
  // of the desks bar, which includes the new desk button. In other words, we
  // have to initiate the drag before we can know where to drop.
  const gfx::Point overview_item_center =
      gfx::ToRoundedPoint(overview_controller->overview_session()
                              ->GetOverviewItemForWindow(win1.get())
                              ->target_bounds()
                              .CenterPoint());

  // Pick up the item and move it a little bit to initiate a drag.
  event_generator->set_current_screen_location(overview_item_center);
  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(20, 0);

  // Move the item to the new desk button and drop it.
  event_generator->MoveMouseTo(
      desks_bar_view->new_desk_button()->GetBoundsInScreen().CenterPoint());
  event_generator->ReleaseLeftButton();

  // We should still be in overview mode. We should still have the max number of
  // desks. And `win1` should still belong to the first desk.
  ASSERT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(desks_util::GetMaxNumberOfDesks(),
            desks_bar_view->mini_views().size());
  EXPECT_EQ(desks_util::GetMaxNumberOfDesks(), controller->desks().size());
  EXPECT_TRUE(
      base::Contains(controller->GetDeskAtIndex(0)->windows(), win1.get()));
}

TEST_P(DesksTest, MruWindowTracker) {
  // Create two desks with two windows in each.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_2 = controller->GetDeskAtIndex(1);
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, controller->active_desk());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  auto win3 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));

  // Build active desk's MRU window list.
  auto* mru_window_tracker = Shell::Get()->mru_window_tracker();
  auto window_list = mru_window_tracker->BuildWindowForCycleList(kActiveDesk);
  ASSERT_EQ(2u, window_list.size());
  EXPECT_EQ(win3.get(), window_list[0]);
  EXPECT_EQ(win2.get(), window_list[1]);

  // Build the global MRU window list.
  window_list = mru_window_tracker->BuildWindowForCycleList(kAllDesks);
  ASSERT_EQ(4u, window_list.size());
  EXPECT_EQ(win3.get(), window_list[0]);
  EXPECT_EQ(win2.get(), window_list[1]);
  EXPECT_EQ(win1.get(), window_list[2]);
  EXPECT_EQ(win0.get(), window_list[3]);

  // Switch back to desk_1 and test both MRU list types.
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  ActivateDesk(desk_1);
  window_list = mru_window_tracker->BuildWindowForCycleList(kActiveDesk);
  ASSERT_EQ(2u, window_list.size());
  EXPECT_EQ(win1.get(), window_list[0]);
  EXPECT_EQ(win0.get(), window_list[1]);
  // TODO(afakhry): Check with UX if we should favor active desk's windows in
  // the global MRU list (i.e. put them first in the list).
  window_list = mru_window_tracker->BuildWindowForCycleList(kAllDesks);
  ASSERT_EQ(4u, window_list.size());
  EXPECT_EQ(win1.get(), window_list[0]);
  EXPECT_EQ(win3.get(), window_list[1]);
  EXPECT_EQ(win2.get(), window_list[2]);
  EXPECT_EQ(win0.get(), window_list[3]);
}

TEST_P(DesksTest, NextActivatable) {
  // Create two desks with two windows in each.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_2 = controller->GetDeskAtIndex(1);
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, controller->active_desk());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  auto win3 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));
  EXPECT_EQ(win3.get(), window_util::GetActiveWindow());

  // When deactivating a window, the next activatable window should be on the
  // same desk.
  wm::DeactivateWindow(win3.get());
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());
  wm::DeactivateWindow(win2.get());
  EXPECT_EQ(win3.get(), window_util::GetActiveWindow());

  // Similarly for desk_1.
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  ActivateDesk(desk_1);
  EXPECT_EQ(win1.get(), window_util::GetActiveWindow());
  win1.reset();
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());
  win0.reset();
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());
}

TEST_P(DesksTest, NoMiniViewsUpdateOnOverviewEnter) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  auto* desk_1 = controller->GetDeskAtIndex(0);
  auto* desk_2 = controller->GetDeskAtIndex(1);

  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win1.get());
  EXPECT_EQ(win1.get(), window_util::GetActiveWindow());

  TestDeskObserver desk_1_observer;
  TestDeskObserver desk_2_observer;
  desk_1->AddObserver(&desk_1_observer);
  desk_2->AddObserver(&desk_2_observer);
  EXPECT_EQ(0, desk_1_observer.notify_counts());
  EXPECT_EQ(0, desk_2_observer.notify_counts());

  // The widgets created by overview mode, whose windows are added to the active
  // desk's container, should never result in mini_views updates since they're
  // not mirrored there at all.
  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  EXPECT_EQ(0, desk_1_observer.notify_counts());
  EXPECT_EQ(0, desk_2_observer.notify_counts());

  desk_1->RemoveObserver(&desk_1_observer);
  desk_2->RemoveObserver(&desk_2_observer);
}

// Tests that the consecutive daily visits metric is properly recorded.
TEST_P(DesksTest, ConsecutiveDailyVisitsMetric) {
  constexpr char kConsecutiveDailyVisitsHistogram[] =
      "Ash.Desks.ConsecutiveDailyVisits";
  auto* desks_controller = DesksController::Get();
  base::HistogramTester histogram_tester;
  base::SimpleTestClock test_clock;

  // Set the time to 00:00:00 local time the next day, override the current
  // desk's clock and reset its visited metrics.
  test_clock.SetNow(base::Time::Now().LocalMidnight());
  test_clock.Advance(base::Hours(1));
  auto* active_desk = desks_controller->active_desk();
  desks_restore_util::OverrideClockForTesting(&test_clock);
  DesksTestApi::ResetDeskVisitedMetrics(const_cast<Desk*>(active_desk));
  EXPECT_EQ(
      0u,
      histogram_tester.GetAllSamples(kConsecutiveDailyVisitsHistogram).size());

  // Create a new desk and don't visit it.
  active_desk = desks_controller->active_desk();
  NewDesk();
  ASSERT_EQ(active_desk, desks_controller->active_desk());

  // Fast forward by two days then remove the active desk. This should record an
  // entry for two days since we stayed on the active desk the whole time.
  // Additionally, there shouldn't be a record for the desk we switch to since
  // we haven't visited it yet.
  test_clock.Advance(base::Days(2));
  RemoveDesk(active_desk);
  histogram_tester.ExpectBucketCount(kConsecutiveDailyVisitsHistogram, 3, 1);
  EXPECT_EQ(
      1u,
      histogram_tester.GetAllSamples(kConsecutiveDailyVisitsHistogram).size());

  // Create a new desk and remove the active desk. This should record an entry
  // for one day since we visited the active desk before removing it.
  active_desk = desks_controller->active_desk();
  NewDesk();
  RemoveDesk(active_desk);
  histogram_tester.ExpectBucketCount(kConsecutiveDailyVisitsHistogram, 1, 1);
  EXPECT_EQ(
      2u,
      histogram_tester.GetAllSamples(kConsecutiveDailyVisitsHistogram).size());

  // Create a new desk and switch to it. Then fast forward two days and revisit
  // the previous desk. Since it's been more than one day since the last visit,
  // a one day entry should be recorded for the previous desk.
  NewDesk();
  ActivateDesk(desks_controller->GetNextDesk());
  test_clock.Advance(base::Days(2));
  ActivateDesk(desks_controller->GetPreviousDesk());
  histogram_tester.ExpectBucketCount(kConsecutiveDailyVisitsHistogram, 1, 2);
  EXPECT_EQ(
      2u,
      histogram_tester.GetAllSamples(kConsecutiveDailyVisitsHistogram).size());

  // Go back in time to simulate a user switching timezones and then switch to
  // the next desk. Since the current time is before the |last_day_visited_|
  // field of the next desk, its visited fields should be reset.
  test_clock.Advance(base::Days(-2));
  ActivateDesk(desks_controller->GetNextDesk());
  active_desk = desks_controller->active_desk();
  const int current_date = desks_restore_util::GetDaysFromLocalEpoch();
  EXPECT_EQ(current_date, active_desk->first_day_visited());
  EXPECT_EQ(current_date, active_desk->last_day_visited());
  desks_restore_util::OverrideClockForTesting(nullptr);
}

// Tests that the new desk button's state and color are as expected.
TEST_P(DesksTest, NewDeskButtonStateAndColor) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  EnterOverview();
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  const auto* new_desk_button = desks_bar_view->new_desk_button();

  // Tests that with one or two desks, the new desk button has an enabled state
  // and color.
  auto* color_provider = new_desk_button->GetColorProvider();
  SkColor background_color =
      color_provider->GetColor(cros_tokens::kCrosSysPrimary);

  const SkColor disabled_background_color =
      ColorUtil::GetDisabledColor(background_color);
  EXPECT_TRUE(new_desk_button->GetEnabled());
  EXPECT_EQ(background_color, GetNewDeskButtonBackgroundColor(desks_bar_view));

  LeftClickOn(new_desk_button);
  EXPECT_TRUE(new_desk_button->GetEnabled());
  EXPECT_EQ(background_color, GetNewDeskButtonBackgroundColor(desks_bar_view));

  // Tests that adding desks until we reach the desks limit should change the
  // state and color of the new desk button.
  size_t prev_size = controller->desks().size();
  while (controller->CanCreateDesks()) {
    LeftClickOn(new_desk_button);
    EXPECT_EQ(prev_size + 1, controller->desks().size());
    prev_size = controller->desks().size();
    RunScheduledLayoutForAllOverviewDeskBars();
    TryScrollOverviewDeskBar(DeskBarScrollDirection::kRight);
    RunScheduledLayoutForAllOverviewDeskBars();
  }
  EXPECT_FALSE(new_desk_button->GetEnabled());
  EXPECT_EQ(disabled_background_color,
            GetNewDeskButtonBackgroundColor(desks_bar_view));
}

// Tests that the fullscreen state in shell is updated when switching between
// desks that have active windows in different fullscreen states.
TEST_P(DesksTest, FullscreenStateUpdatedAcrossDesks) {
  FullScreenStateObserver full_screen_state_observer;
  auto* controller = DesksController::Get();
  WMEvent event_toggle_fullscreen(WM_EVENT_TOGGLE_FULLSCREEN);

  // Create one new desks.
  NewDesk();
  EXPECT_EQ(2u, controller->desks().size());

  // Create one window in each desk.
  std::vector<std::unique_ptr<aura::Window>> windows;
  for (int i = 0; i < 2; i++) {
    windows.push_back(CreateAppWindow());
    controller->SendToDeskAtIndex(windows[i].get(), i);
    EXPECT_EQ(i, windows[i]->GetProperty(aura::client::kWindowWorkspaceKey));
  }

  WindowState* win0_state = WindowState::Get(windows[0].get());
  WindowState* win1_state = WindowState::Get(windows[1].get());

  EXPECT_FALSE(full_screen_state_observer.is_fullscreen());

  // Set window on desk 0 to fullscreen.
  win0_state->OnWMEvent(&event_toggle_fullscreen);
  EXPECT_EQ(windows[0].get(), window_util::GetActiveWindow());
  EXPECT_TRUE(win0_state->IsFullscreen());
  EXPECT_TRUE(full_screen_state_observer.is_fullscreen());

  // Switch to desk 1 and expect the fullscreen state to change.
  ActivateDesk(controller->GetDeskAtIndex(1));

  EXPECT_EQ(windows[1].get(), window_util::GetActiveWindow());
  EXPECT_FALSE(win1_state->IsFullscreen());
  EXPECT_FALSE(full_screen_state_observer.is_fullscreen());

  // Cycle back to desk 0 and expect the fullscreen state to change back.
  ActivateDesk(controller->GetDeskAtIndex(0));

  EXPECT_EQ(windows[0].get(), window_util::GetActiveWindow());
  EXPECT_TRUE(win0_state->IsFullscreen());
  EXPECT_TRUE(full_screen_state_observer.is_fullscreen());
}

// Tests the Ash.Desks.AnimationLatency.DeskActivation histogram.
TEST_P(DesksTest, AnimationLatencyDeskActivation) {
  NewDesk();
  auto* controller = DesksController::Get();
  ASSERT_EQ(2u, controller->desks().size());

  base::HistogramTester histogram_tester;
  ActivateDesk(controller->GetDeskAtIndex(1));
  histogram_tester.ExpectTotalCount("Ash.Desks.AnimationLatency.DeskActivation",
                                    1);
}

// Tests the Ash.Desks.AnimationLatency.DeskRemoval histogram.
TEST_P(DesksTest, AnimationLatencyDeskRemoval) {
  NewDesk();
  auto* controller = DesksController::Get();
  ASSERT_EQ(2u, controller->desks().size());

  base::HistogramTester histogram_tester;
  RemoveDesk(controller->GetDeskAtIndex(0));
  histogram_tester.ExpectTotalCount("Ash.Desks.AnimationLatency.DeskRemoval",
                                    1);
}

// Tests that the MRU window in a desk is focused when that desk is activated.
TEST_P(DesksTest, MruFocusedOnDeskSwitchDualDisplay) {
  UpdateDisplay("700x600,400x500");
  auto* controller = DesksController::Get();
  auto roots = Shell::GetAllRootWindows();

  // Create two desks, each with 2 windows
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto win2 = CreateAppWindow(gfx::Rect(750, 50, 200, 200));

  // Ensure there is one window on each display.
  ASSERT_EQ(win1->GetRootWindow(), roots[0]);
  ASSERT_EQ(win2->GetRootWindow(), roots[1]);

  // Focus window 2
  win2->Focus();

  // Create and switch to the second desk.
  NewDesk();
  const Desk* desk_1 = controller->GetDeskAtIndex(0);
  const Desk* desk_2 = controller->GetDeskAtIndex(1);
  ActivateDesk(desk_2);
  auto win3 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto win4 = CreateAppWindow(gfx::Rect(750, 50, 200, 200));
  ASSERT_EQ(win3->GetRootWindow(), roots[0]);
  ASSERT_EQ(win4->GetRootWindow(), roots[1]);

  // Focus window 3
  win3->Focus();

  // Switch back to desk 1 and ensure window 2 regains focus.
  ActivateDesk(desk_1);
  ASSERT_TRUE(win2->HasFocus());
  ASSERT_FALSE(win1->HasFocus());
  ASSERT_FALSE(win3->HasFocus());
  ASSERT_FALSE(win4->HasFocus());

  // Switch back to desk 2 and ensure window 3 regains focus.
  ActivateDesk(desk_2);
  ASSERT_TRUE(win3->HasFocus());
  ASSERT_FALSE(win1->HasFocus());
  ASSERT_FALSE(win2->HasFocus());
  ASSERT_FALSE(win4->HasFocus());
}

// Tests that we can set a lacros profile ID on a desk and that observers get
// notified.
TEST_P(DesksTest, LacrosProfileId) {
  auto* controller = DesksController::Get();
  Desk* desk = controller->GetDeskAtIndex(0);

  TestDeskObserver desk_observer;
  desk->AddObserver(&desk_observer);

  desk->SetLacrosProfileId(1001, /*source=*/std::nullopt);
  EXPECT_THAT(desk_observer.lacros_profile_id_updates(),
              testing::ElementsAre(1001));

  // Setting the same ID does not result in observer notifications.
  desk->SetLacrosProfileId(1001, /*source=*/std::nullopt);
  EXPECT_THAT(desk_observer.lacros_profile_id_updates(),
              testing::ElementsAre(1001));

  desk->SetLacrosProfileId(2001, /*source=*/std::nullopt);
  EXPECT_THAT(desk_observer.lacros_profile_id_updates(),
              testing::ElementsAre(1001, 2001));

  desk->RemoveObserver(&desk_observer);
}

// Tests that a display can be removed during a desk switch.
TEST_P(DesksTest, RemoveDisplayWhileSwitchingDesks) {
  auto* controller = DesksController::Get();

  // Set up two desks and two displays. We're then going to switch to the second
  // desk and during the switch, remove one of the displays.
  UpdateDisplay("800x600,800x600");
  NewDesk();

  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  DeskSwitchAnimationWaiter desk_switch_waiter;

  const Desk* desk_2 = controller->GetDeskAtIndex(1);
  controller->ActivateDesk(desk_2, DesksSwitchSource::kMiniViewButton);

  // Updating to one display. This will cause the root window for the second
  // display to be destroyed.
  UpdateDisplay("800x600");

  desk_switch_waiter.Wait();
  EXPECT_EQ(desk_2, controller->active_desk());
}

// Tests that creating a new desk while overview is exiting doesn't cause a
// crash. Regression test for http://b/365546169
TEST_P(DesksTest, AddDeskWhileExitingOverview) {
  UpdateDisplay("800x600");

  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Exit overview (with an animation) and immediately create a second desk.
  ExitOverview();
  NewDesk();
}

class DesksWithMultiDisplayOverview : public AshTestBase {
 public:
  DesksWithMultiDisplayOverview() = default;
  ~DesksWithMultiDisplayOverview() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Start the test with two displays and two desks.
    UpdateDisplay("700x600,400x500");
    NewDesk();
  }
};

TEST_F(DesksWithMultiDisplayOverview, DropOnSameDeskInOtherDisplay) {
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());

  auto win = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item = overview_session->GetOverviewItemForWindow(win.get());
  ASSERT_TRUE(overview_item);

  // The window should exist on the grid of the first display.
  auto* grid1 = GetOverviewGridForRoot(roots[0]);
  auto* grid2 = GetOverviewGridForRoot(roots[1]);
  EXPECT_EQ(1u, grid1->GetNumWindows());
  EXPECT_EQ(grid1, overview_item->overview_grid());
  EXPECT_EQ(0u, grid2->GetNumWindows());

  // Drag the item and drop it on the mini view of the same desk (i.e. desk 1)
  // on the second display. The window should not change desks, but it should
  // change displays (i.e. it should be removed from |grid1| and added to
  // |grid2|).
  const auto* desks_bar_view = grid2->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  auto* desk_1_mini_view = desks_bar_view->mini_views()[0].get();
  auto* event_generator = GetEventGenerator();
  DragItemToPoint(overview_item,
                  desk_1_mini_view->GetBoundsInScreen().CenterPoint(),
                  event_generator,
                  /*by_touch_gestures=*/false);
  // A new item should have been created for the window on the second grid.
  EXPECT_TRUE(overview_controller->InOverviewSession());
  overview_item = overview_session->GetOverviewItemForWindow(win.get());
  ASSERT_TRUE(overview_item);
  EXPECT_EQ(0u, grid1->GetNumWindows());
  EXPECT_EQ(grid2, overview_item->overview_grid());
  EXPECT_EQ(1u, grid2->GetNumWindows());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win.get()));
  EXPECT_EQ(roots[1], win->GetRootWindow());
}

TEST_F(DesksWithMultiDisplayOverview, DropOnOtherDeskInOtherDisplay) {
  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());

  auto win = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item = overview_session->GetOverviewItemForWindow(win.get());
  ASSERT_TRUE(overview_item);

  // The window should exist on the grid of the first display.
  auto* grid1 = GetOverviewGridForRoot(roots[0]);
  auto* grid2 = GetOverviewGridForRoot(roots[1]);
  EXPECT_EQ(1u, grid1->GetNumWindows());
  EXPECT_EQ(grid1, overview_item->overview_grid());
  EXPECT_EQ(0u, grid2->GetNumWindows());

  const auto* desks_bar_view = grid2->desks_bar_view();
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  gfx::Point desk_2_mini_view_center =
      desk_2_mini_view->GetBoundsInScreen().CenterPoint();
  // When |chromeos::wm::features::kVerticalSnap| is enabled, one of two drag
  // indicators show up on the top instead of the left side of the display. Such
  // top indicator pushes the desks bar down, so we need to drag the item to the
  // area that triggers drag indicators without dropping first to get the
  // updated position of the mini view before dropping the window on it.
  DragItemToPoint(overview_item, desk_2_mini_view_center, GetEventGenerator(),
                  /*by_touch_gestures=*/false,
                  /*drop=*/false);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  // Validate that before dropping, the SplitView indicators and the drop target
  // widget are created.
  EXPECT_EQ(
      SplitViewDragIndicators::WindowDraggingState::kFromOverview,
      grid2->split_view_drag_indicators()->current_window_dragging_state());
  desk_2_mini_view_center = desk_2_mini_view->GetBoundsInScreen().CenterPoint();
  // Now drop the window to desk 2 mini view in the second display.
  DragItemToPoint(overview_item, desk_2_mini_view_center, GetEventGenerator(),
                  /*by_touch_gestures=*/false,
                  /*drop=*/true);

  // The item should no longer be in any grid, since it moved to an inactive
  // desk.
  EXPECT_TRUE(overview_controller->InOverviewSession());
  overview_item = overview_session->GetOverviewItemForWindow(win.get());
  ASSERT_FALSE(overview_item);
  EXPECT_EQ(0u, grid1->GetNumWindows());
  EXPECT_EQ(0u, grid2->GetNumWindows());
  EXPECT_FALSE(DoesActiveDeskContainWindow(win.get()));
  EXPECT_EQ(roots[1], win->GetRootWindow());
  EXPECT_FALSE(win->IsVisible());
  auto* controller = DesksController::Get();
  const Desk* desk_2 = controller->GetDeskAtIndex(1);
  EXPECT_TRUE(base::Contains(desk_2->windows(), win.get()));
}

// Tests that closing a desk while in overview before the overview starting
// animation finishes on a second display does not cause a crash. Regression
// test for https://crbug.com/1346154.
TEST_F(DesksWithMultiDisplayOverview, CloseDeskBeforeAnimationFinishes) {
  // We need a non-zero duration to ensure we can close the desk before the
  // animation completes.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create a maximized window on Desk 1. This means that when entering
  // overview, we skip the wallpaper animation for Desk 1, so that the
  // desks_bar_view for RootWindow-0 will initialize before RootWindow-1.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  const WMEvent toggle_maximize_event(WM_EVENT_TOGGLE_MAXIMIZE);
  WindowState::Get(win0.get())->OnWMEvent(&toggle_maximize_event);
  ASSERT_TRUE(WindowState::Get(win0.get())->IsMaximized());

  EnterOverview();
  auto* overview_controller = OverviewController::Get();
  ASSERT_TRUE(overview_controller->InOverviewSession());

  // Check that the desk bar on our first root window has finished initializing,
  // while the second desk bar hasn't been initialized yet since the enter
  // animation hasn't finished for the second root window.
  auto root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const auto* desks_bar_view_1 =
      GetOverviewGridForRoot(root_windows[0])->desks_bar_view();
  ASSERT_EQ(2u, desks_bar_view_1->mini_views().size());
  ASSERT_EQ(nullptr, GetOverviewGridForRoot(root_windows[1])->desks_bar_view());

  // Close the first desk. Previously, this would result in a crash since the
  // desk is deleted before `desks_bar_view_2` is initialized, resulting in the
  // `desk_1_mini_view` not being created for `desks_bar_view_2`. When
  // `OnDeskRemoved` is then called for the second desk bar, we now return early
  // if that miniview can't be found.
  // To prevent flakiness in this test, we wait until `desks_bar_view_2` is
  // initialized and shown before we check the state.
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "OverviewDeskBarWidget");
  auto* desk_1_mini_view = desks_bar_view_1->mini_views()[0].get();
  CloseDeskFromMiniView(desk_1_mini_view, GetEventGenerator());
  waiter.WaitIfNeededAndGet();

  // Verify that we are still in overview mode and that both desks bars are in
  // the zero state.
  ASSERT_TRUE(overview_controller->InOverviewSession());
  // The desk bar never goes back to zero state after it's initialized.
  ASSERT_FALSE(desks_bar_view_1->IsZeroState());
  const auto* desks_bar_view_2 =
      GetOverviewGridForRoot(root_windows[1])->desks_bar_view();
  ASSERT_TRUE(desks_bar_view_2->IsZeroState());
}

PrefService* GetPrimaryUserPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

// Returns the desk names in the given `user_prefs`.
std::vector<std::string> GetDeskRestoreNames(PrefService* user_prefs) {
  const base::Value::List& desk_restore_names =
      user_prefs->GetList(prefs::kDesksNamesList);

  std::vector<std::string> names;
  for (const auto& value : desk_restore_names) {
    names.push_back(value.GetString());
  }
  return names;
}

// Returns the GUIDs in the given `user_prefs`.
std::vector<base::Uuid> GetDeskRestoreGuids(PrefService* user_prefs) {
  const base::Value::List& desks_restore_guids =
      user_prefs->GetList(prefs::kDesksGuidsList);

  std::vector<base::Uuid> guids;
  for (const base::Value& value : desks_restore_guids) {
    guids.emplace_back(base::Uuid::ParseLowercase(value.GetString()));
  }
  return guids;
}

// Returns the lacros profile IDs in the given `user_prefs`.
std::vector<uint64_t> GetDeskRestoreLacrosProfileIds(PrefService* user_prefs) {
  const base::Value::List& lacros_profile_ids_list =
      user_prefs->GetList(prefs::kDesksLacrosProfileIdList);

  std::vector<uint64_t> lacros_profile_ids;
  for (const base::Value& value : lacros_profile_ids_list) {
    uint64_t lacros_profile_id = 0;
    if (base::StringToUint64(value.GetString(), &lacros_profile_id)) {
      lacros_profile_ids.push_back(lacros_profile_id);
    } else {
      lacros_profile_ids.push_back(0);
    }
  }
  return lacros_profile_ids;
}

class DesksEditableNamesTest : public DesksTest {
 public:
  DesksEditableNamesTest() = default;
  DesksEditableNamesTest(const DesksEditableNamesTest&) = delete;
  DesksEditableNamesTest& operator=(const DesksEditableNamesTest&) = delete;
  ~DesksEditableNamesTest() override = default;

  DesksController* controller() { return controller_; }
  OverviewGrid* overview_grid() { return overview_grid_; }
  const DeskBarViewBase* desks_bar_view() { return desks_bar_view_; }

  // DesksTest:
  void SetUp() override {
    DesksTest::SetUp();

    // Begin all tests with two desks and start overview.
    NewDesk();
    controller_ = DesksController::Get();

    EnterOverview();
    overview_grid_ = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
    desks_bar_view_ = overview_grid_->desks_bar_view();
    ASSERT_TRUE(desks_bar_view_);
  }

  void ClickOnDeskNameViewAtIndex(size_t index) {
    ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
    ASSERT_LT(index, desks_bar_view_->mini_views().size());

    auto* desk_name_view =
        desks_bar_view_->mini_views()[index]->desk_name_view();
    auto* generator = GetEventGenerator();
    generator->MoveMouseTo(desk_name_view->GetBoundsInScreen().CenterPoint());
    generator->ClickLeftButton();
  }

 private:
  raw_ptr<DesksController, DanglingUntriaged> controller_ = nullptr;
  raw_ptr<OverviewGrid, DanglingUntriaged> overview_grid_ = nullptr;
  raw_ptr<const DeskBarViewBase, DanglingUntriaged> desks_bar_view_ = nullptr;
};

TEST_P(DesksEditableNamesTest, DefaultNameChangeAborted) {
  ASSERT_EQ(2u, controller()->desks().size());
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Click on the desk name view of the second mini view.
  auto* desk_name_view_2 = desks_bar_view()->mini_views()[1]->desk_name_view();
  EXPECT_FALSE(overview_grid()->IsDeskNameBeingModified());
  ClickOnDeskNameViewAtIndex(1);
  EXPECT_TRUE(overview_grid()->IsDeskNameBeingModified());
  EXPECT_TRUE(desk_name_view_2->HasFocus());

  // Pressing enter now without making any changes should not change the
  // `is_name_set_by_user_` bit.
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_FALSE(overview_grid()->IsDeskNameBeingModified());
  EXPECT_FALSE(desk_name_view_2->HasFocus());
  auto* desk_2 = controller()->GetDeskAtIndex(1);
  EXPECT_FALSE(desk_2->is_name_set_by_user());

  // Desks restore data should reflect two default-named desks.
  EXPECT_THAT(GetDeskRestoreNames(GetPrimaryUserPrefService()),
              ElementsAre("", ""));
}

TEST_P(DesksEditableNamesTest, NamesSetByUsersAreNotOverwritten) {
  ASSERT_EQ(2u, controller()->desks().size());
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Change the name of the first desk. Adding/removing desks or
  // exiting/reentering overview should not cause changes to the desk's name.
  // All other names should be the default.
  ClickOnDeskNameViewAtIndex(0);
  // Select all and delete.
  PressAndReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  PressAndReleaseKey(ui::VKEY_BACK);
  // Type " code  " (with one space at the beginning and two spaces at the end)
  // and hit enter.
  PressAndReleaseKey(ui::VKEY_SPACE);
  PressAndReleaseKey(ui::VKEY_C);
  PressAndReleaseKey(ui::VKEY_O);
  PressAndReleaseKey(ui::VKEY_D);
  PressAndReleaseKey(ui::VKEY_E);
  PressAndReleaseKey(ui::VKEY_SPACE);
  PressAndReleaseKey(ui::VKEY_SPACE);
  PressAndReleaseKey(ui::VKEY_RETURN);

  // Extra whitespace should be trimmed.
  auto* desk_1 = controller()->GetDeskAtIndex(0);
  auto* desk_2 = controller()->GetDeskAtIndex(1);
  EXPECT_EQ(u"code", desk_1->name());
  EXPECT_EQ(u"Desk 2", desk_2->name());
  EXPECT_TRUE(desk_1->is_name_set_by_user());
  EXPECT_FALSE(desk_2->is_name_set_by_user());

  // Renaming desks via the mini views trigger an update to the desks restore
  // prefs.
  EXPECT_THAT(GetDeskRestoreNames(GetPrimaryUserPrefService()),
              ElementsAre("code", ""));

  // Add a third desk and remove the second. Both operations should not affect
  // the user-modified desk names.
  NewDesk();
  auto* desk_3 = controller()->GetDeskAtIndex(2);
  EXPECT_EQ(u"Desk 3", desk_3->name());
  EXPECT_TRUE(desk_1->is_name_set_by_user());
  EXPECT_FALSE(desk_2->is_name_set_by_user());
  EXPECT_FALSE(desk_3->is_name_set_by_user());

  // Adding a desk triggers an update to the restore prefs.
  EXPECT_THAT(GetDeskRestoreNames(GetPrimaryUserPrefService()),
              ElementsAre("code", "", ""));

  RemoveDesk(desk_2);
  EXPECT_TRUE(desk_1->is_name_set_by_user());
  EXPECT_FALSE(desk_3->is_name_set_by_user());
  // Desk 3 will now be renamed to "Desk 2".
  EXPECT_EQ(u"Desk 2", desk_3->name());
  EXPECT_THAT(GetDeskRestoreNames(GetPrimaryUserPrefService()),
              ElementsAre("code", ""));

  ExitOverview();
  EnterOverview();
  EXPECT_TRUE(desk_1->is_name_set_by_user());
  EXPECT_FALSE(desk_3->is_name_set_by_user());
  EXPECT_EQ(u"code", desk_1->name());
  EXPECT_EQ(u"Desk 2", desk_3->name());
}

TEST_P(DesksEditableNamesTest, DontAllowEmptyNames) {
  ASSERT_EQ(2u, controller()->desks().size());
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Change the name of a desk to an empty string.
  ClickOnDeskNameViewAtIndex(0);
  // Select all and delete.
  PressAndReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  PressAndReleaseKey(ui::VKEY_BACK);
  // Commit with the enter key.
  PressAndReleaseKey(ui::VKEY_RETURN);
  // The name should now revert back to the default value.
  auto* desk_1 = controller()->GetDeskAtIndex(0);
  EXPECT_FALSE(desk_1->name().empty());
  EXPECT_FALSE(desk_1->is_name_set_by_user());
  EXPECT_EQ(u"Desk 1", desk_1->name());
  EXPECT_THAT(GetDeskRestoreNames(GetPrimaryUserPrefService()),
              ElementsAre("", ""));
}

TEST_P(DesksEditableNamesTest, RevertDeskNameOnEscape) {
  ASSERT_EQ(2u, controller()->desks().size());
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Select first desk name view.
  ClickOnDeskNameViewAtIndex(0);
  // Edit the name of the desk.
  PressAndReleaseKey(ui::VKEY_E);
  PressAndReleaseKey(ui::VKEY_S);
  PressAndReleaseKey(ui::VKEY_C);
  // Press escape key.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  // Name should be previous value.
  auto* desk_1 = controller()->GetDeskAtIndex(0);
  EXPECT_FALSE(desk_1->is_name_set_by_user());
  EXPECT_EQ(u"Desk 1", desk_1->name());
}

TEST_P(DesksEditableNamesTest, SelectAllOnFocus) {
  ASSERT_EQ(2u, controller()->desks().size());
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
  ClickOnDeskNameViewAtIndex(0);

  auto* desk_name_view = desks_bar_view()->mini_views()[0]->desk_name_view();
  EXPECT_TRUE(desk_name_view->HasFocus());
  EXPECT_TRUE(desk_name_view->HasSelection());
  auto* desk_1 = controller()->GetDeskAtIndex(0);
  EXPECT_EQ(desk_1->name(), desk_name_view->GetSelectedText());
}

TEST_P(DesksEditableNamesTest, EventsThatCommitChanges) {
  ASSERT_EQ(2u, controller()->desks().size());
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
  ClickOnDeskNameViewAtIndex(0);
  auto* desk_name_view = desks_bar_view()->mini_views()[0]->desk_name_view();
  EXPECT_TRUE(desk_name_view->HasFocus());

  // Creating a new desk commits the changes.
  auto* new_desk_button = desks_bar_view()->new_desk_button();
  LeftClickOn(new_desk_button);
  RunScheduledLayoutForAllOverviewDeskBars();
  ASSERT_EQ(3u, controller()->desks().size());
  EXPECT_FALSE(desk_name_view->HasFocus());

  // Deleting a desk commits the changes.
  ClickOnDeskNameViewAtIndex(0);
  RunScheduledLayoutForAllOverviewDeskBars();
  EXPECT_TRUE(desk_name_view->HasFocus());
  auto* event_generator = GetEventGenerator();
  CloseDeskFromMiniView(desks_bar_view()->mini_views()[2], event_generator);
  RunScheduledLayoutForAllOverviewDeskBars();
  ASSERT_EQ(2u, controller()->desks().size());
  EXPECT_FALSE(desk_name_view->HasFocus());

  // Clicking in the empty area of the desks bar also commits the changes.
  ClickOnDeskNameViewAtIndex(0);
  RunScheduledLayoutForAllOverviewDeskBars();
  EXPECT_TRUE(desk_name_view->HasFocus());
  event_generator->MoveMouseTo(gfx::Point(2, 2));
  event_generator->ClickLeftButton();
  EXPECT_FALSE(desk_name_view->HasFocus());
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
}

TEST_P(DesksEditableNamesTest, MaxLength) {
  ASSERT_EQ(2u, controller()->desks().size());
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  ClickOnDeskNameViewAtIndex(0);
  // Select all and delete.
  PressAndReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  PressAndReleaseKey(ui::VKEY_BACK);

  // Simulate user is typing text beyond the max length.
  std::u16string expected_desk_name(DeskTextfield::kMaxLength, L'a');
  for (size_t i = 0; i < DeskTextfield::kMaxLength + 10; ++i) {
    PressAndReleaseKey(ui::VKEY_A);
  }
  PressAndReleaseKey(ui::VKEY_RETURN);

  // Desk name has been trimmed.
  auto* desk_1 = controller()->GetDeskAtIndex(0);
  EXPECT_EQ(DeskTextfield::kMaxLength, desk_1->name().size());
  EXPECT_EQ(expected_desk_name, desk_1->name());
  EXPECT_TRUE(desk_1->is_name_set_by_user());

  // Test that pasting a large amount of text is trimmed at the max length.
  std::u16string clipboard_text(DeskTextfield::kMaxLength + 10, L'b');
  expected_desk_name = std::u16string(DeskTextfield::kMaxLength, L'b');
  EXPECT_GT(clipboard_text.size(), DeskTextfield::kMaxLength);
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(clipboard_text);

  ClickOnDeskNameViewAtIndex(0);
  // Select all and delete.
  PressAndReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  PressAndReleaseKey(ui::VKEY_BACK);

  // Paste text.
  PressAndReleaseKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_EQ(DeskTextfield::kMaxLength, desk_1->name().size());
  EXPECT_EQ(expected_desk_name, desk_1->name());
}

class TabletModeDesksTest : public DesksTest {
 public:
  TabletModeDesksTest() = default;

  TabletModeDesksTest(const TabletModeDesksTest&) = delete;
  TabletModeDesksTest& operator=(const TabletModeDesksTest&) = delete;

  ~TabletModeDesksTest() override = default;

  // DesksTest:
  void SetUp() override {
    DesksTest::SetUp();

    // Enter tablet mode after detaching all mouse devices, as this is needed
    // when running these tests on an actual workstation.
    TabletModeControllerTestApi tablet_mode_test_api;
    tablet_mode_test_api.DetachAllMice();
    tablet_mode_test_api.EnterTabletMode();
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }
};

// Triggers a callback when the visibility of an observed window changes for the
// first time since creation of this observer.
class WindowVisibilityObserver : public aura::WindowObserver {
 public:
  WindowVisibilityObserver(aura::Window* window, base::OnceClosure callback)
      : on_visibility_changed_callback_(std::move(callback)) {
    DCHECK(on_visibility_changed_callback_);
    observer_.Observe(window);
  }
  WindowVisibilityObserver(const WindowVisibilityObserver&) = delete;
  WindowVisibilityObserver& operator=(const WindowVisibilityObserver&) = delete;
  ~WindowVisibilityObserver() override = default;

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (observer_.IsObservingSource(window) &&
        on_visibility_changed_callback_) {
      std::move(on_visibility_changed_callback_).Run();
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    DCHECK(observer_.IsObservingSource(window));
    observer_.Reset();
  }

 private:
  base::ScopedObservation<aura::Window, aura::WindowObserver> observer_{this};

  base::OnceClosure on_visibility_changed_callback_;
};

// Regression test for https://crbug.com/1368587.
TEST_P(TabletModeDesksTest, CantDestroyBackdropWhileHiding) {
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  const Desk* desk_1 = controller->GetDeskAtIndex(0);

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Verify that there is a visible backdrop for the app window on the current
  // desk.
  auto* desk_1_backdrop_controller =
      GetDeskBackdropController(desk_1, Shell::GetPrimaryRootWindow());
  auto* backdrop_window = desk_1_backdrop_controller->backdrop_window();
  ASSERT_TRUE(backdrop_window);
  EXPECT_TRUE(backdrop_window->IsVisible());

  // Intercept the visibility change notification of the backdrop (which will
  // happen when we enter overview below), and hide the app window. This
  // triggers an update of the backdrop while still in the process of hiding
  // it. Now that the single app window available is hidden, the backdrop
  // controller will try to destroy the backdrop. This should be prevented as
  // we don't allow `BackdropController::Hide()` to be called recursively.
  auto* app_window = window.get();
  WindowVisibilityObserver observer{
      backdrop_window,
      base::BindLambdaForTesting([app_window, backdrop_window]() {
        app_window->Hide();
        EXPECT_FALSE(backdrop_window->IsVisible());
      })};

  // Enter overview and expect that the backdrop is still present for desk_1 but
  // hidden.
  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window()->IsVisible());
}

TEST_P(TabletModeDesksTest, Backdrops) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_1 = controller->GetDeskAtIndex(0);
  const Desk* desk_2 = controller->GetDeskAtIndex(1);

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Enter tablet mode and expect that the backdrop is created only for desk_1,
  // since it's the one that has a window in it.
  auto* desk_1_backdrop_controller =
      GetDeskBackdropController(desk_1, Shell::GetPrimaryRootWindow());
  auto* desk_2_backdrop_controller =
      GetDeskBackdropController(desk_2, Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_TRUE(desk_1_backdrop_controller->backdrop_window()->IsVisible());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());

  // Enter overview and expect that the backdrop is still present for desk_1 but
  // hidden.
  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window()->IsVisible());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->GetNumWindows());

  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  // Now drag it to desk_2's mini_view, so that it moves to desk_2. Expect that
  // desk_1's backdrop is destroyed, while created (but still hidden) for
  // desk_2.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  EXPECT_EQ(desk_2, desk_2_mini_view->desk());
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window()->IsVisible());

  // Exit overview, and expect that desk_2's backdrop remains hidden since the
  // desk is not activated yet.
  ExitOverview(OverviewEnterExitType::kImmediateExit);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window()->IsVisible());

  // Activate desk_2 and expect that its backdrop is now visible.
  ActivateDesk(desk_2);
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_TRUE(desk_2_backdrop_controller->backdrop_window()->IsVisible());

  // No backdrops after exiting tablet mode.
  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());
}

TEST_P(TabletModeDesksTest,
       BackdropStackingAndMiniviewsUpdatesWithOverviewDragDrop) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
  auto* desk_1_backdrop_controller =
      GetDeskBackdropController(desk_1, Shell::GetPrimaryRootWindow());
  auto* desk_2_backdrop_controller =
      GetDeskBackdropController(desk_2, Shell::GetPrimaryRootWindow());

  // Enter overview and expect that |desk_1| has a backdrop stacked under
  // |window| while desk_2 has none.
  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_EQ(window.get(), desk_1_backdrop_controller->window_having_backdrop());
  EXPECT_FALSE(desk_2_backdrop_controller->window_having_backdrop());
  EXPECT_TRUE(window_util::IsStackedBelow(
      desk_1_backdrop_controller->backdrop_window(), window.get()));

  // Prepare to drag and drop |window| on desk_2's mini view.
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->GetNumWindows());
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();

  // Observe how many times a drag and drop operation updates the mini views.
  TestDeskObserver observer1;
  TestDeskObserver observer2;
  desk_1->AddObserver(&observer1);
  desk_2->AddObserver(&observer2);
  {
    // For this test to fail the stacking test below, we need to drag and drop
    // while animations are enabled. https://crbug.com/1055732.
    ui::ScopedAnimationDurationScaleMode normal_anim(
        ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
    DragItemToPoint(overview_item,
                    desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                    GetEventGenerator());
  }
  // The backdrop should be destroyed for |desk_1|, and a new one should be
  // created for |window| in desk_2 and should be stacked below it.
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_1_backdrop_controller->window_having_backdrop());
  EXPECT_EQ(window.get(), desk_2_backdrop_controller->window_having_backdrop());
  EXPECT_TRUE(window_util::IsStackedBelow(
      desk_2_backdrop_controller->backdrop_window(), window.get()));

  // The mini views should only be updated once for both desks.
  EXPECT_EQ(1, observer1.notify_counts());
  EXPECT_EQ(1, observer2.notify_counts());
  desk_1->RemoveObserver(&observer1);
  desk_2->RemoveObserver(&observer2);
}

TEST_P(TabletModeDesksTest, NoDesksBarInTabletModeWithOneDesk) {
  // Initially there's only one desk.
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Enter overview and expect that the DesksBar widget won't be created.
  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_FALSE(desks_bar_view);

  // It's possible to drag the window without any crashes.
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  DragItemToPoint(overview_item, window->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator(), /*by_touch_gestures=*/true);

  // Exit overview and add a new desk, then re-enter overview. Expect that now
  // the desks bar is visible.
  ExitOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  NewDesk();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
}

TEST_P(TabletModeDesksTest, DesksCreationRemovalCycle) {
  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Create and remove desks in a cycle while in overview mode. Expect as the
  // containers are reused for new desks, their backdrop state are always
  // correct, and there are no crashes as desks are removed.
  auto* desks_controller = DesksController::Get();
  for (size_t i = 0; i < 2 * desks_util::GetMaxNumberOfDesks(); ++i) {
    NewDesk();
    ASSERT_EQ(2u, desks_controller->desks().size());
    const Desk* desk_1 = desks_controller->GetDeskAtIndex(0);
    const Desk* desk_2 = desks_controller->GetDeskAtIndex(1);
    auto* desk_1_backdrop_controller =
        GetDeskBackdropController(desk_1, Shell::GetPrimaryRootWindow());
    auto* desk_2_backdrop_controller =
        GetDeskBackdropController(desk_2, Shell::GetPrimaryRootWindow());
    {
      SCOPED_TRACE("Check backdrops after desk creation");
      ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
      EXPECT_TRUE(desk_1_backdrop_controller->backdrop_window()->IsVisible());
      EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());
    }
    // Remove the active desk, and expect that now desk_2 should have a hidden
    // backdrop, while the container of the removed desk_1 should have none.
    RemoveDesk(desk_1);
    {
      SCOPED_TRACE("Check backdrops after desk removal");
      EXPECT_TRUE(desk_2->is_active());
      EXPECT_TRUE(DoesActiveDeskContainWindow(window.get()));
      EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
      ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
      EXPECT_TRUE(desk_2_backdrop_controller->backdrop_window()->IsVisible());
    }
  }
}

TEST_P(TabletModeDesksTest, RestoreSplitViewOnDeskSwitch) {
  // Create two desks with two snapped windows in each.
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_1 = desks_controller->GetDeskAtIndex(0);
  Desk* desk_2 = desks_controller->GetDeskAtIndex(1);

  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(win2.get(), SnapPosition::kSecondary);
  EXPECT_EQ(win1.get(), split_view_controller()->primary_window());
  EXPECT_EQ(win2.get(), split_view_controller()->secondary_window());

  // Desk 2 has no windows, so the SplitViewController should be tracking no
  // windows.
  ActivateDesk(desk_2);
  EXPECT_EQ(nullptr, split_view_controller()->primary_window());
  EXPECT_EQ(nullptr, split_view_controller()->secondary_window());
  // However, the snapped windows on desk 1 should retain their snapped state.
  EXPECT_TRUE(WindowState::Get(win1.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(win2.get())->IsSnapped());

  // Snap two other windows in desk 2.
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win3.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(win4.get(), SnapPosition::kSecondary);
  EXPECT_EQ(win3.get(), split_view_controller()->primary_window());
  EXPECT_EQ(win4.get(), split_view_controller()->secondary_window());

  // Switch back to desk 1, and expect the snapped windows are restored.
  ActivateDesk(desk_1);
  EXPECT_EQ(win1.get(), split_view_controller()->primary_window());
  EXPECT_EQ(win2.get(), split_view_controller()->secondary_window());
  EXPECT_TRUE(WindowState::Get(win3.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(win4.get())->IsSnapped());
}

TEST_P(
    TabletModeDesksTest,
    SnappedStateRetainedOnSwitchingDesksWithOverviewFullOfUnsnappableWindows) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());

  // Create one window for snapping, and two windows whose minimum size is too
  // big for half snap.
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          Shell::GetPrimaryRootWindow());
  const gfx::Size big(work_area.width() * 2 / 3, work_area.height() * 2 / 3);
  const gfx::Size small(250, 100);
  std::unique_ptr<aura::Window> win1 = CreateTestWindow(gfx::Rect(small));
  aura::test::TestWindowDelegate win2_delegate;
  win2_delegate.set_minimum_size(big);
  std::unique_ptr<aura::Window> win2(CreateTestWindowInShellWithDelegate(
      &win2_delegate, /*id=*/-1, gfx::Rect(big)));
  aura::test::TestWindowDelegate win3_delegate;
  win3_delegate.set_minimum_size(big);
  std::unique_ptr<aura::Window> win3(CreateTestWindowInShellWithDelegate(
      &win3_delegate, /*id=*/-1, gfx::Rect(big)));

  ASSERT_TRUE(EnterOverview());
  split_view_controller()->SnapWindow(win1.get(), SnapPosition::kPrimary);
  EXPECT_EQ(win1.get(), split_view_controller()->primary_window());
  EXPECT_FALSE(split_view_controller()->CanSnapWindow(
      win2.get(), chromeos::kDefaultSnapRatio));
  EXPECT_FALSE(split_view_controller()->CanSnapWindow(
      win3.get(), chromeos::kDefaultSnapRatio));

  // Use `Search + ]` to switch to `desk2`. Split view should end, but `win1`
  // should retain its snapped state.
  PressAndReleaseKey(ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN);
  DeskSwitchAnimationWaiter().Wait();
  EXPECT_TRUE(WindowState::Get(win1.get())->IsSnapped());
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());

  // Use `Search + [` to switch back to `desk_1` and verify that split view is
  // arranged as before.
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
  PressAndReleaseKey(ui::VKEY_OEM_4, ui::EF_COMMAND_DOWN);
  DeskSwitchAnimationWaiter().Wait();

  EXPECT_TRUE(WindowState::Get(win1.get())->IsSnapped());
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(win1.get(), split_view_controller()->primary_window());
}

TEST_P(TabletModeDesksTest, OverviewStateOnSwitchToDeskWithSplitView) {
  // Setup two desks, one (desk_1) with two snapped windows, and the other
  // (desk_2) with only one snapped window.
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_1 = desks_controller->GetDeskAtIndex(0);
  Desk* desk_2 = desks_controller->GetDeskAtIndex(1);
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(win2.get(), SnapPosition::kSecondary);
  EXPECT_EQ(win1.get(), split_view_controller()->primary_window());
  EXPECT_EQ(win2.get(), split_view_controller()->secondary_window());
  auto* overview_controller = OverviewController::Get();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  ActivateDesk(desk_2);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win3.get(), SnapPosition::kPrimary);
  EXPECT_EQ(win3.get(), split_view_controller()->primary_window());
  EXPECT_EQ(nullptr, split_view_controller()->secondary_window());

  // Switching to the desk that has only one snapped window to be restored in
  // SplitView should enter overview mode, whereas switching to one that has two
  // snapped windows should exit overview.
  ActivateDesk(desk_1);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  ActivateDesk(desk_2);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ActivateDesk(desk_1);
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

TEST_P(TabletModeDesksTest, RemovingDesksWithSplitView) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_2 = desks_controller->GetDeskAtIndex(1);
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SnapPosition::kPrimary);
  EXPECT_EQ(win1.get(), split_view_controller()->primary_window());
  EXPECT_EQ(nullptr, split_view_controller()->secondary_window());
  ActivateDesk(desk_2);
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win2.get(), SnapPosition::kSecondary);
  EXPECT_EQ(nullptr, split_view_controller()->primary_window());
  EXPECT_EQ(win2.get(), split_view_controller()->secondary_window());

  // Removing desk_2 will cause both snapped windows to merge in SplitView.
  RemoveDesk(desk_2);
  EXPECT_EQ(win1.get(), split_view_controller()->primary_window());
  EXPECT_EQ(win2.get(), split_view_controller()->secondary_window());
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
}

TEST_P(TabletModeDesksTest, RemoveDeskWithMaximizedWindowAndMergeWithSnapped) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_2 = desks_controller->GetDeskAtIndex(1);
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SnapPosition::kPrimary);
  EXPECT_EQ(win1.get(), split_view_controller()->primary_window());
  EXPECT_EQ(nullptr, split_view_controller()->secondary_window());
  ActivateDesk(desk_2);
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  EXPECT_EQ(nullptr, split_view_controller()->primary_window());
  EXPECT_EQ(nullptr, split_view_controller()->secondary_window());
  EXPECT_TRUE(WindowState::Get(win2.get())->IsMaximized());

  // Removing desk_2 will cause us to enter overview mode without any crashes.
  // SplitView will remain left snapped.
  RemoveDesk(desk_2);
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(win1.get(), split_view_controller()->primary_window());
  EXPECT_EQ(nullptr, split_view_controller()->secondary_window());
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
}

// Tests that closing the active desk while in overview does not quit overview.
// Regression test for https://crbug.com/1309175.
TEST_P(TabletModeDesksTest, RemovingActiveDeskDoesNotExitOverview) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_2 = desks_controller->GetDeskAtIndex(1);
  ActivateDesk(desk_2);

  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  ASSERT_TRUE(overview_controller->InOverviewSession());

  // Remove `desk_2`, which is the active test. We should stay in overview.
  RemoveDesk(desk_2);
  EXPECT_TRUE(overview_controller->InOverviewSession());
}

TEST_P(TabletModeDesksTest, BackdropsStacking) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_1 = desks_controller->GetDeskAtIndex(0);
  Desk* desk_2 = desks_controller->GetDeskAtIndex(1);

  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(win2.get(), SnapPosition::kSecondary);
  auto* desk_1_backdrop_controller =
      GetDeskBackdropController(desk_1, Shell::GetPrimaryRootWindow());
  auto* desk_2_backdrop_controller =
      GetDeskBackdropController(desk_2, Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());

  // The backdrop window should be stacked below both snapped windows.
  auto* desk_1_backdrop = desk_1_backdrop_controller->backdrop_window();
  EXPECT_TRUE(window_util::IsStackedBelow(desk_1_backdrop, win1.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(desk_1_backdrop, win2.get()));

  // Switching to another desk doesn't change the backdrop state of the inactive
  // desk.
  ActivateDesk(desk_2);
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_TRUE(window_util::IsStackedBelow(desk_1_backdrop, win1.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(desk_1_backdrop, win2.get()));

  // Snapping new windows in desk_2 should update the backdrop state of desk_2,
  // but should not affect desk_1.
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win3.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(win4.get(), SnapPosition::kSecondary);
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  auto* desk_2_backdrop = desk_2_backdrop_controller->backdrop_window();
  EXPECT_TRUE(window_util::IsStackedBelow(desk_2_backdrop, win3.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(desk_2_backdrop, win4.get()));
}

TEST_P(TabletModeDesksTest, RestoringUnsnappableWindowsInSplitView) {
  UpdateDisplay("600x400");
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  // Setup an app window that cannot be snapped in landscape orientation, but
  // can be snapped in portrait orientation.
  auto window = CreateAppWindow(gfx::Rect(350, 350));
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window.get());
  widget->widget_delegate()->GetContentsView()->SetPreferredSize(
      gfx::Size(350, 100));
  EXPECT_FALSE(split_view_controller()->CanSnapWindow(
      window.get(), chromeos::kDefaultSnapRatio));

  // Change to a portrait orientation and expect it's possible to snap the
  // window.
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(
      window.get(), chromeos::kDefaultSnapRatio));

  // Snap the window in this orientation.
  split_view_controller()->SnapWindow(window.get(), SnapPosition::kPrimary);
  EXPECT_EQ(window.get(), split_view_controller()->primary_window());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // Create a second desk, switch to it, and change back the orientation to
  // landscape, in which the window is not snappable. The window still exists on
  // the first desk, so nothing should change.
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_2 = controller->GetDeskAtIndex(1);
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, controller->active_desk());
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);

  // Switch back to the first desk, and expect that SplitView is not restored,
  // since the only available window on that desk is not snappable.
  const Desk* desk_1 = controller->GetDeskAtIndex(0);
  ActivateDesk(desk_1);
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());
}

TEST_P(DesksTest, MiniViewsTouchGestures) {
  auto* controller = DesksController::Get();
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(3u, desks_bar_view->mini_views().size());
  auto* desk_1_mini_view = desks_bar_view->mini_views()[0].get();
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  auto* desk_3_mini_view = desks_bar_view->mini_views()[2].get();

  // Long gesture tapping on one desk preview shows its desk action interface,
  // and hides those of other mini views.
  auto* event_generator = GetEventGenerator();
  const gfx::Point desk_1_preview_center =
      desk_1_mini_view->desk_preview()->GetBoundsInScreen().CenterPoint();
  const gfx::Point desk_2_preview_center =
      desk_2_mini_view->desk_preview()->GetBoundsInScreen().CenterPoint();

  LongGestureTap(desk_1_preview_center, event_generator);

  // The context menu appears on the first long press and after the user taps
  // away the buttons will show. So we need to tap away and wait for the context
  // menu to disappear before checking whether the desk action interfaces are
  // visible.
  event_generator->GestureTapDownAndUp(desk_1_preview_center);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetDeskActionVisibilityForMiniView(desk_1_mini_view));
  EXPECT_FALSE(GetDeskActionVisibilityForMiniView(desk_2_mini_view));
  EXPECT_FALSE(GetDeskActionVisibilityForMiniView(desk_3_mini_view));

  LongGestureTap(desk_2_preview_center, event_generator);

  event_generator->GestureTapDownAndUp(desk_2_preview_center);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(GetDeskActionVisibilityForMiniView(desk_1_mini_view));
  EXPECT_TRUE(GetDeskActionVisibilityForMiniView(desk_2_mini_view));
  EXPECT_FALSE(GetDeskActionVisibilityForMiniView(desk_3_mini_view));

  // Tapping on the visible close button closes the desk rather than
  // switches to that desk.
  GestureTapOn(GetCloseDeskButtonForMiniView(desk_2_mini_view));
  ASSERT_EQ(2u, controller->desks().size());
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Tapping on the invisible close button should not result in closing
  // that desk; rather activating that desk.
  EXPECT_FALSE(GetDeskActionVisibilityForMiniView(desk_1_mini_view));
  GestureTapOn(GetCloseDeskButtonForMiniView(desk_1_mini_view));
  ASSERT_EQ(2u, controller->desks().size());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(controller->GetDeskAtIndex(0)->is_active());
}

TEST_P(DesksTest, AutohiddenShelfAnimatesAfterDeskSwitch) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfWidget* shelf_widget = shelf->shelf_widget();
  const gfx::Rect shown_shelf_bounds = shelf_widget->GetWindowBoundsInScreen();

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Enable animations so that we can make sure that they occur.
  ui::ScopedAnimationDurationScaleMode non_zero_animation(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  NewDesk();

  // Create a window on the first desk so that the shelf will auto-hide there.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->Maximize();
  // LayoutShelf() forces the animation to completion, at which point the
  // shelf should go off the screen.
  shelf->shelf_layout_manager()->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  const gfx::Rect hidden_shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  EXPECT_NE(shown_shelf_bounds, hidden_shelf_bounds);

  // Go to the second desk.
  ActivateDesk(DesksController::Get()->GetDeskAtIndex(1));
  // The shelf should now want to show itself.
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Since the layer transform animation is just starting, the transformed
  // bounds should still be hidden. If this fails, the change was not animated.
  gfx::Rect transformed_bounds = shelf_widget->GetLayer()->transform().MapRect(
      shelf_widget->GetWindowBoundsInScreen());
  EXPECT_EQ(transformed_bounds, hidden_shelf_bounds);
  EXPECT_EQ(shelf_widget->GetWindowBoundsInScreen(), shown_shelf_bounds);

  ui::LayerAnimationStoppedWaiter().Wait(shelf_widget->GetLayer());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

TEST_P(DesksTest, SwitchToDeskWithSnappedActiveWindow) {
  auto* desks_controller = DesksController::Get();
  auto* overview_controller = OverviewController::Get();

  // Two virtual desks: |desk_1| (active) and |desk_2|.
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_1 = desks_controller->GetDeskAtIndex(0);
  Desk* desk_2 = desks_controller->GetDeskAtIndex(1);

  // Two windows on |desk_1|: |win0| (snapped) and |win1|.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  WindowState* win0_state = WindowState::Get(win0.get());
  WindowSnapWMEvent snap_to_left(WM_EVENT_CYCLE_SNAP_PRIMARY);
  win0_state->OnWMEvent(&snap_to_left);
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            win0_state->GetStateType());

  // Switch to |desk_2| and then back to |desk_1|. Verify that neither split
  // view nor overview arises.
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  EXPECT_FALSE(split_view_controller->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  ActivateDesk(desk_2);
  EXPECT_FALSE(split_view_controller->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  ActivateDesk(desk_1);
  EXPECT_FALSE(split_view_controller->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

TEST_P(DesksTest, SuccessfulDragToDeskRemovesSplitViewIndicators) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());

  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());

  // Drag it to desk_2's mini_view. The overview grid should now show the
  // "no-windows" widget, and the window should move to desk_2.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator(),
                  /*by_touch_gestures=*/false,
                  /*drop=*/false);
  // Validate that before dropping, the SplitView indicators and the drop target
  // widget are created.
  EXPECT_TRUE(overview_grid->drop_target());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            overview_session->grid_list()[0]
                ->split_view_drag_indicators()
                ->current_window_dragging_state());
  // Now drop the window, and validate the indicators and the drop target were
  // removed.
  GetEventGenerator()->ReleaseLeftButton();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_grid->empty());
  EXPECT_FALSE(DoesActiveDeskContainWindow(window.get()));
  EXPECT_TRUE(overview_grid->no_windows_widget());
  EXPECT_FALSE(overview_grid->drop_target());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            overview_session->grid_list()[0]
                ->split_view_drag_indicators()
                ->current_window_dragging_state());
}

TEST_P(DesksTest, DragAllOverviewWindowsToOtherDesksNotEndClamshellSplitView) {
  // Two virtual desks.
  NewDesk();
  ASSERT_EQ(2u, DesksController::Get()->desks().size());

  // Two windows: |win0| (in clamshell split view) and |win1| (in overview).
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* overview_controller = OverviewController::Get();
  ASSERT_TRUE(EnterOverview());
  auto* overview_session = overview_controller->overview_session();
  auto* generator = GetEventGenerator();
  DragItemToPoint(overview_session->GetOverviewItemForWindow(win0.get()),
                  gfx::Point(0, 0), generator);
  ASSERT_TRUE(overview_controller->InOverviewSession());
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(split_view_controller->InSplitViewMode());
  EXPECT_TRUE(RootWindowController::ForWindow(win0.get())
                  ->split_view_overview_session());

  // Drag |win1| to the other desk.
  DragItemToPoint(overview_session->GetOverviewItemForWindow(win1.get()),
                  GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
                      ->desks_bar_view()
                      ->mini_views()[1]
                      ->GetBoundsInScreen()
                      .CenterPoint(),
                  generator);
  EXPECT_FALSE(DoesActiveDeskContainWindow(win1.get()));

  // Overview should now be empty, but split view should still be active.
  ASSERT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_session->IsEmpty());
  EXPECT_TRUE(split_view_controller->InSplitViewMode());
}

TEST_P(DesksTest, DeskTraversalNonTouchpadMetrics) {
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, DesksController::Get()->desks().size());

  constexpr char kDeskTraversalsHistogramName[] =
      "Ash.Desks.NumberOfDeskTraversals";

  base::HistogramTester histogram_tester;
  auto* controller = DesksController::Get();
  const auto& desks = controller->desks();
  ASSERT_EQ(controller->active_desk(), desks[0].get());

  // Move 5 desks. There is nothing recorded at the end since the timer is still
  // running.
  ActivateDesk(desks[1].get());
  ActivateDesk(desks[0].get());
  ActivateDesk(desks[1].get());
  ActivateDesk(desks[2].get());
  ActivateDesk(desks[3].get());
  histogram_tester.ExpectBucketCount(kDeskTraversalsHistogramName, 5, 0);

  // Simulate advancing the time to end the timer. There should be 5 desks
  // recorded.
  controller->FireMetricsTimerForTesting();
  histogram_tester.ExpectBucketCount(kDeskTraversalsHistogramName, 5, 1);
}

// Tests that clipping is unchanged when removing a desk in overview. Regression
// test for https://crbug.com/1166300.
TEST_P(DesksTest, RemoveDeskPreservesOverviewClipping) {
  // Three virtual desks.
  NewDesk();
  NewDesk();

  auto* controller = DesksController::Get();
  Desk* desk2 = controller->GetDeskAtIndex(1);
  Desk* desk3 = controller->GetDeskAtIndex(2);
  ActivateDesk(desk3);

  // Create a window on |desk3| with a header.
  const int header_height = 32;
  auto win0 = CreateAppWindow(gfx::Rect(200, 200));
  win0->SetProperty(aura::client::kTopViewInset, header_height);
  EXPECT_EQ(desk3->GetDeskContainerForRoot(Shell::GetPrimaryRootWindow()),
            win0->parent());

  ASSERT_TRUE(EnterOverview());
  const gfx::Rect expected_clip = win0->layer()->GetTargetClipRect();

  // Remove |desk3|. |win0| is now a child of |desk2|.
  RemoveDesk(desk3);
  ASSERT_EQ(desk2->GetDeskContainerForRoot(Shell::GetPrimaryRootWindow()),
            win0->parent());

  // Tests that the clip is the same after the desk removal.
  EXPECT_EQ(expected_clip, win0->layer()->GetTargetClipRect());
}

struct PerDeskZOrderTestCase {
  std::string test_name;

  // True when we want to run the test with multiple displays.
  bool multi_display;

  // Numerical identifiers of windows on desk 1 and 2, grouped by root window.
  // Windows will be stacked in the order they appear, with .back() at top.
  std::vector<std::vector<int>> desk_1_windows;
  std::vector<std::vector<int>> desk_2_windows;

  // Numerical identifiers of all-desk windows. Entries must match windows in
  // `desk_1_windows`.
  std::set<int> adw_windows;

  // Windows to activate in order.
  std::vector<int> activate_windows;

  // Once windows have been created and (some) promoted to all-desk, we start
  // the test by switching to desk 2. This is the list of windows that we expect
  // to find on desk 1/2, grouped by root window.
  std::vector<std::vector<int>> expected_desk_1_windows_before;
  std::vector<std::vector<int>> expected_desk_2_windows_before;

  // We can then move some set of windows from desk 2 to desk 1. This must be a
  // subset of `desk_2_windows` and cannot contain entries from `adw_windows`
  // (since those not logically part of a specific desk). Prior to moving, the
  // window will be activated (to simulate the user interacting with it), and
  // this will push it to the front of the MRU list.
  std::vector<int> move_windows;

  // Windows to move from the first display to the second. This can include both
  // normal and adw windows.
  std::vector<int> move_windows_to_other_display;

  // Windows to close while on the second desk. Can include any windows.
  std::vector<int> close_windows;

  // We then switch back to desk 1 and expect to find windows in this order,
  // grouped by root window.
  std::vector<std::vector<int>> expected_desk_1_windows_after;
  std::vector<std::vector<int>> expected_desk_2_windows_after;

  // This is the window we expect to be active after we switch back to desk 1.
  int expected_desk_1_final_active_window;
};

class DesksPerDeskZOrderTest : public AshTestBase {
 public:
  DesksPerDeskZOrderTest() = default;
  ~DesksPerDeskZOrderTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Start the test with two desks.
    NewDesk();
  }

  void RunTests(const std::vector<PerDeskZOrderTestCase>& tests) {
    auto* controller = DesksController::Get();
    auto* desk_1 = controller->GetDeskAtIndex(0);
    auto* desk_2 = controller->GetDeskAtIndex(1);

    for (const auto& test : tests) {
      SCOPED_TRACE(test.test_name);

      if (test.multi_display) {
        UpdateDisplay("700x600,400x500");
      } else {
        UpdateDisplay("700x600");
      }

      // This is only used for multi-displays tests and will in those cases
      // represent the secondary display.
      display::Display secondary_display =
          display::Screen::GetScreen()->GetAllDisplays().back();

      std::map<int, std::unique_ptr<aura::Window>> id_to_window;
      std::map<aura::Window*, int> window_to_id;

      auto populate_desk =
          [&](Desk* desk, const std::vector<std::vector<int>>& desk_windows) {
            SCOPED_TRACE("Create windows for " +
                         base::UTF16ToUTF8(desk->name()));

            // Verify we are on the correct desk.
            ASSERT_TRUE(desk->is_active());

            int offset = 0;
            for (const auto& window_ids : desk_windows) {
              for (int id : window_ids) {
                auto window =
                    CreateAppWindow(gfx::Rect(offset + id, 0 + id, 100, 100));
                window->SetTitle(u"TestWindow" + base::NumberToString16(id));
                window_to_id[window.get()] = id;
                id_to_window[id] = std::move(window);
              }
              // Increment the offset to ensure that the window will be created
              // on the next root window.
              offset += secondary_display.bounds().x();
            }
          };

      // Populate each of the desks with the desired windows.
      populate_desk(desk_1, test.desk_1_windows);
      ActivateDesk(desk_2);
      populate_desk(desk_2, test.desk_2_windows);
      ActivateDesk(desk_1);

      // Mark windows as awd.
      for (int id : test.adw_windows) {
        views::Widget::GetWidgetForNativeWindow(id_to_window.at(id).get())
            ->SetVisibleOnAllWorkspaces(true);
      }

      for (int id : test.activate_windows) {
        wm::ActivateWindow(id_to_window.at(id).get());
      }

      // Verifies that the desk preview for the given root window is consistent
      // with the expected per-desk Z-order. Since the layers are mirrored
      // instead of the same instances, we verify by layer bounds here.
      auto verify_desk_preview_mirrored_layer_tree =
          [&](Desk* desk, aura::Window* root,
              const std::vector<int>& expected_windows,
              const std::string& debug_info) {
            SCOPED_TRACE("Verify " + base::UTF16ToUTF8(desk->name()) + " " +
                         root->GetName() + " " + debug_info);
            ToggleOverview();

            // Retrieves the mirrored layers `mirrored_layers` of application
            // windows for `desk`. The root of `layer_tree_owner` is a layer
            // that has only one child, and the only child acts as the parent of
            // all the mirrored layers of application windows.
            const ui::LayerTreeOwner* layer_tree_owner =
                DesksTestApi::GetMirroredContentsLayerTreeForRootAndDesk(root,
                                                                         desk);
            const std::vector<raw_ptr<ui::Layer, VectorExperimental>>
                mirrored_layers =
                    layer_tree_owner->root()->children().front()->children();

            // Tests that `mirrored_layers` and `expected_windows` are sync'ed.
            ASSERT_EQ(expected_windows.size(), mirrored_layers.size());

            std::vector<gfx::Rect> actual_bounds, expected_bounds;
            for (size_t i = 0; i < expected_windows.size(); i++) {
              expected_bounds.push_back(
                  id_to_window[expected_windows[i]]->layer()->bounds());
              actual_bounds.push_back(mirrored_layers[i]->bounds());
            }
            EXPECT_THAT(actual_bounds, ElementsAreArray(expected_bounds));

            ToggleOverview();
          };

      // Verifies that windows on the given desk and root window are found in
      // the expected order. Any windows that have not been created by the test
      // will be ignored.
      auto verify_windows = [&](Desk* desk, aura::Window* root,
                                const std::vector<int>& expected_windows,
                                const std::string& debug_info) {
        aura::Window* container = desk->GetDeskContainerForRoot(root);

        // Collect any test windows present on the desk.
        std::vector<int> actual_windows;
        for (aura::Window* child : container->children()) {
          auto it = window_to_id.find(child);
          if (it != window_to_id.end()) {
            actual_windows.push_back(it->second);
          }
        }

        ASSERT_EQ(expected_windows, actual_windows)
            << "Window mismatch " << debug_info << " on " << root->GetName()
            << ":" << desk->name();
      };

      auto root_windows = Shell::GetAllRootWindows();
      ASSERT_EQ(test.multi_display ? 2u : 1u, root_windows.size());

      // Now we are ready to actually execute the test.
      ActivateDesk(desk_2);
      for (size_t i = 0; i < root_windows.size(); i++) {
        verify_windows(desk_2, root_windows[i],
                       test.expected_desk_2_windows_before[i], "before moving");
        verify_desk_preview_mirrored_layer_tree(
            desk_1, root_windows[i], test.expected_desk_1_windows_before[i],
            "before");
        verify_desk_preview_mirrored_layer_tree(
            desk_2, root_windows[i], test.expected_desk_2_windows_before[i],
            "before");
      }

      // Move specified windows to desk 1.
      for (int id : test.move_windows) {
        const auto& window = id_to_window.at(id);
        wm::ActivateWindow(window.get());
        ASSERT_TRUE(controller->MoveWindowFromActiveDeskTo(
            window.get(), desk_1, window.get()->GetRootWindow(),
            DesksMoveWindowFromActiveDeskSource::kShortcut));
      }

      // Move specified windows to display 2.
      for (int id : test.move_windows_to_other_display) {
        const auto& window = id_to_window.at(id);
        aura::Window* root_before_moving = window->GetRootWindow();
        auto bounds = window->GetBoundsInScreen();
        bounds.Offset(secondary_display.bounds().origin().x(), 0);
        window->SetBoundsInScreen(bounds, secondary_display);
        ASSERT_NE(root_before_moving, window->GetRootWindow());
      }

      // Close specified windows.
      for (int id : test.close_windows) {
        auto it = id_to_window.find(id);
        ASSERT_NE(it, id_to_window.end()) << "Test setup error";
        window_to_id.erase(it->second.get());
        id_to_window.erase(it);
      }

      ActivateDesk(desk_1);
      for (size_t i = 0; i < root_windows.size(); i++) {
        verify_windows(desk_1, root_windows[i],
                       test.expected_desk_1_windows_after[i], "after moving");
        verify_desk_preview_mirrored_layer_tree(
            desk_1, root_windows[i], test.expected_desk_1_windows_after[i],
            "after");
        verify_desk_preview_mirrored_layer_tree(
            desk_2, root_windows[i], test.expected_desk_2_windows_after[i],
            "after");
      }

      // Verify that the correct window is activated. This is particularly
      // important for when we have all desk windows and multiple displays.
      // Please refer to b/274110274.
      int actual_active_window_id = -1;
      auto active_window_it = window_to_id.find(window_util::GetActiveWindow());
      if (active_window_it != window_to_id.end()) {
        actual_active_window_id = active_window_it->second;
      }
      ASSERT_EQ(test.expected_desk_1_final_active_window,
                actual_active_window_id);
    }
  }
};

TEST_F(DesksPerDeskZOrderTest, SingleDisplay) {
  RunTests(std::vector<PerDeskZOrderTestCase>{
      {.test_name = "Single adw window 1",
       .desk_1_windows = {{1}},
       .desk_2_windows = {{}},
       .adw_windows = {1},
       .expected_desk_1_windows_before = {{1}},
       .expected_desk_2_windows_before = {{1}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1}},
       .expected_desk_2_windows_after = {{1}},
       .expected_desk_1_final_active_window = 1},
      {.test_name = "Single adw window 2",
       .desk_1_windows = {{1, 2, 3}},
       .desk_2_windows = {{5, 4}},
       .adw_windows = {1},
       .expected_desk_1_windows_before = {{1, 2, 3}},
       .expected_desk_2_windows_before = {{5, 4, 1}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1, 2, 3}},
       .expected_desk_2_windows_after = {{5, 4, 1}},
       .expected_desk_1_final_active_window = 3},
      {.test_name = "Single adw window 3",
       .desk_1_windows = {{1, 2, 3}},
       .desk_2_windows = {{5, 4}},
       .adw_windows = {1},
       .expected_desk_1_windows_before = {{1, 2, 3}},
       .expected_desk_2_windows_before = {{5, 4, 1}},
       .move_windows = {5},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1, 2, 3, 5}},
       .expected_desk_2_windows_after = {{4, 1}},
       .expected_desk_1_final_active_window = 5},
      {.test_name = "Single adw window 4",
       .desk_1_windows = {{1, 2, 3}},
       .desk_2_windows = {{5, 4}},
       .adw_windows = {2},
       .expected_desk_1_windows_before = {{1, 2, 3}},
       .expected_desk_2_windows_before = {{5, 4, 2}},
       .move_windows = {5},
       .close_windows = {1},
       .expected_desk_1_windows_after = {{2, 3, 5}},
       .expected_desk_2_windows_after = {{4, 2}},
       .expected_desk_1_final_active_window = 5},
      {.test_name = "Single adw window 5",
       .desk_1_windows = {{1, 2, 3, 4, 5}},
       .desk_2_windows = {{6}},
       .adw_windows = {3},
       .expected_desk_1_windows_before = {{1, 2, 3, 4, 5}},
       .expected_desk_2_windows_before = {{6, 3}},
       .move_windows = {6},
       .close_windows = {1, 2},
       .expected_desk_1_windows_after = {{3, 4, 5, 6}},
       .expected_desk_2_windows_after = {{3}},
       .expected_desk_1_final_active_window = 6},
      {.test_name = "Multiple adw windows 1",
       .desk_1_windows = {{1, 2, 3, 4, 5}},
       .desk_2_windows = {{6, 7}},
       .adw_windows = {2, 4},
       .expected_desk_1_windows_before = {{1, 2, 3, 4, 5}},
       .expected_desk_2_windows_before = {{6, 7, 2, 4}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1, 2, 3, 4, 5}},
       .expected_desk_2_windows_after = {{6, 7, 2, 4}},
       .expected_desk_1_final_active_window = 5},
      {.test_name = "Multiple adw windows 2",
       .desk_1_windows = {{1, 2, 3, 4, 5}},
       .desk_2_windows = {{6, 7}},
       .adw_windows = {1, 3, 5},
       .expected_desk_1_windows_before = {{1, 2, 3, 4, 5}},
       .expected_desk_2_windows_before = {{6, 7, 1, 3, 5}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1, 2, 3, 4, 5}},
       .expected_desk_2_windows_after = {{6, 7, 1, 3, 5}},
       .expected_desk_1_final_active_window = 5},
      {.test_name = "Multiple adw windows 3",
       .desk_1_windows = {{1, 2}},
       .desk_2_windows = {{}},
       .adw_windows = {1, 2},
       .expected_desk_1_windows_before = {{1, 2}},
       .expected_desk_2_windows_before = {{1, 2}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1, 2}},
       .expected_desk_2_windows_after = {{1, 2}},
       .expected_desk_1_final_active_window = 2},
      {.test_name = "Multiple adw windows 4",
       .desk_1_windows = {{1, 2, 3, 4}},
       .desk_2_windows = {{}},
       .adw_windows = {1, 4},
       .activate_windows = {4, 3, 2, 1},
       .expected_desk_1_windows_before = {{4, 3, 2, 1}},
       .expected_desk_2_windows_before = {{1, 4}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{4, 3, 2, 1}},
       .expected_desk_2_windows_after = {{1, 4}},
       .expected_desk_1_final_active_window = 1},
      {.test_name = "Multiple adw windows 5",
       .desk_1_windows = {{1, 2, 3, 4}},
       .desk_2_windows = {{5}},
       .adw_windows = {1, 3},
       .activate_windows = {1, 2, 3, 4},
       .expected_desk_1_windows_before = {{1, 2, 3, 4}},
       .expected_desk_2_windows_before = {{5, 1, 3}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1, 2, 3, 4}},
       .expected_desk_2_windows_after = {{5, 1, 3}},
       .expected_desk_1_final_active_window = 4},
  });
}

TEST_F(DesksPerDeskZOrderTest, MultiDisplaySingleADW) {
  RunTests(std::vector<PerDeskZOrderTestCase>{
      {.test_name = "Multiple displays single adw 1",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{}, {5}},
       .adw_windows = {1},
       .activate_windows = {1, 2, 3, 4},
       .expected_desk_1_windows_before = {{1}, {2, 3, 4}},
       .expected_desk_2_windows_before = {{1}, {5}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1}, {2, 3, 4}},
       .expected_desk_2_windows_after = {{1}, {5}},
       .expected_desk_1_final_active_window = 4},
      {.test_name = "Multiple displays single adw 2",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{}, {5}},
       .adw_windows = {1},
       .activate_windows = {1, 2, 3, 4},
       .expected_desk_1_windows_before = {{1}, {2, 3, 4}},
       .expected_desk_2_windows_before = {{1}, {5}},
       .move_windows = {},
       .close_windows = {1},
       .expected_desk_1_windows_after = {{}, {2, 3, 4}},
       .expected_desk_2_windows_after = {{}, {5}},
       .expected_desk_1_final_active_window = 4},
      {.test_name = "Multiple displays single adw 3",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{}, {5}},
       .adw_windows = {1},
       .activate_windows = {4, 3, 2, 1},
       .expected_desk_1_windows_before = {{1}, {4, 3, 2}},
       .expected_desk_2_windows_before = {{1}, {5}},
       .move_windows = {},
       .close_windows = {1},
       .expected_desk_1_windows_after = {{}, {4, 3, 2}},
       .expected_desk_2_windows_after = {{}, {5}},
       .expected_desk_1_final_active_window = 2},
      {.test_name = "Multiple displays single adw 4",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{}, {5}},
       .adw_windows = {1},
       .activate_windows = {4, 3, 2, 1},
       .expected_desk_1_windows_before = {{1}, {4, 3, 2}},
       .expected_desk_2_windows_before = {{1}, {5}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1}, {4, 3, 2}},
       .expected_desk_2_windows_after = {{1}, {5}},
       .expected_desk_1_final_active_window = 1},
      {.test_name = "Multiple displays single adw 5",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{}, {5}},
       .adw_windows = {4},
       .activate_windows = {1, 2, 4, 3},
       .expected_desk_1_windows_before = {{1}, {2, 4, 3}},
       .expected_desk_2_windows_before = {{}, {5, 4}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1}, {2, 4, 3}},
       .expected_desk_2_windows_after = {{}, {5, 4}},
       .expected_desk_1_final_active_window = 3},
      {.test_name = "Multiple displays single adw 6",
       .multi_display = true,
       .desk_1_windows = {{1, 2}, {3, 4}},
       .desk_2_windows = {{5}, {6}},
       .adw_windows = {3},
       .activate_windows = {1, 2, 3, 4},
       .expected_desk_1_windows_before = {{1, 2}, {3, 4}},
       .expected_desk_2_windows_before = {{5}, {6, 3}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1, 2}, {3, 4}},
       .expected_desk_2_windows_after = {{5}, {6, 3}},
       .expected_desk_1_final_active_window = 4},
      {.test_name = "Multiple displays single adw 7",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{}, {5}},
       .adw_windows = {1},
       .activate_windows = {4, 3, 2, 1},
       .expected_desk_1_windows_before = {{1}, {4, 3, 2}},
       .expected_desk_2_windows_before = {{1}, {5}},
       .move_windows = {5},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1}, {4, 3, 2, 5}},
       .expected_desk_2_windows_after = {{1}, {}},
       .expected_desk_1_final_active_window = 5},
      {.test_name = "Multiple displays single adw 8",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{5}, {}},
       .adw_windows = {1},
       .activate_windows = {4, 3, 2, 1},
       .expected_desk_1_windows_before = {{1}, {4, 3, 2}},
       .expected_desk_2_windows_before = {{5, 1}, {}},
       .move_windows = {5},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1, 5}, {4, 3, 2}},
       .expected_desk_2_windows_after = {{1}, {}},
       .expected_desk_1_final_active_window = 5},
      {.test_name = "Multiple displays single adw 9",
       .multi_display = true,
       .desk_1_windows = {{}, {1, 2, 3}},
       .desk_2_windows = {{}, {}},
       .adw_windows = {1},
       .expected_desk_1_windows_before = {{}, {1, 2, 3}},
       .expected_desk_2_windows_before = {{}, {1}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{}, {1, 2, 3}},
       .expected_desk_2_windows_after = {{}, {1}},
       .expected_desk_1_final_active_window = 3},
      {.test_name = "Multiple displays single adw 10",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{5}, {}},
       .adw_windows = {4},
       .activate_windows = {1, 4, 3, 2},
       .expected_desk_1_windows_before = {{1}, {4, 3, 2}},
       .expected_desk_2_windows_before = {{5}, {4}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1}, {4, 3, 2}},
       .expected_desk_2_windows_after = {{5}, {4}},
       .expected_desk_1_final_active_window = 2},
  });
}

TEST_F(DesksPerDeskZOrderTest, MultiDisplayMultipleADW) {
  RunTests(std::vector<PerDeskZOrderTestCase>{
      {.test_name = "Multiple displays multiple adw 1",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{}, {5}},
       .adw_windows = {1, 4},
       .activate_windows = {1, 2, 3, 4},
       .expected_desk_1_windows_before = {{1}, {2, 3, 4}},
       .expected_desk_2_windows_before = {{1}, {5, 4}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1}, {2, 3, 4}},
       .expected_desk_2_windows_after = {{1}, {5, 4}},
       .expected_desk_1_final_active_window = 4},
      {.test_name = "Multiple displays multiple adw 2",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{}, {5}},
       .adw_windows = {1, 4},
       .activate_windows = {2, 3, 4, 1},
       .expected_desk_1_windows_before = {{1}, {2, 3, 4}},
       .expected_desk_2_windows_before = {{1}, {5, 4}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1}, {2, 3, 4}},
       .expected_desk_2_windows_after = {{1}, {5, 4}},
       .expected_desk_1_final_active_window = 1},
      {.test_name = "Multiple displays multiple adw 3",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{}, {5}},
       .adw_windows = {1, 4},
       .activate_windows = {1, 2, 3, 4},
       .expected_desk_1_windows_before = {{1}, {2, 3, 4}},
       .expected_desk_2_windows_before = {{1}, {5, 4}},
       .move_windows = {},
       .close_windows = {1},
       .expected_desk_1_windows_after = {{}, {2, 3, 4}},
       .expected_desk_2_windows_after = {{}, {5, 4}},
       .expected_desk_1_final_active_window = 4},
      {.test_name = "Multiple displays multiple adw 4",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{}, {5}},
       .adw_windows = {1, 4},
       .activate_windows = {2, 3, 4, 1},
       .expected_desk_1_windows_before = {{1}, {2, 3, 4}},
       .expected_desk_2_windows_before = {{1}, {5, 4}},
       .move_windows = {},
       .close_windows = {1},
       .expected_desk_1_windows_after = {{}, {2, 3, 4}},
       .expected_desk_2_windows_after = {{}, {5, 4}},
       .expected_desk_1_final_active_window = 4},
      {.test_name = "Multiple displays multiple adw 5",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{}, {5}},
       .adw_windows = {1, 4},
       .activate_windows = {4, 3, 2, 1},
       .expected_desk_1_windows_before = {{1}, {4, 3, 2}},
       .expected_desk_2_windows_before = {{1}, {5, 4}},
       .move_windows = {},
       .close_windows = {4},
       .expected_desk_1_windows_after = {{1}, {3, 2}},
       .expected_desk_2_windows_after = {{1}, {5}},
       .expected_desk_1_final_active_window = 1},
      {.test_name = "Multiple displays multiple adw 6",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{}, {5}},
       .adw_windows = {1, 4},
       .activate_windows = {4, 3, 2, 1},
       .expected_desk_1_windows_before = {{1}, {4, 3, 2}},
       .expected_desk_2_windows_before = {{1}, {5, 4}},
       .move_windows = {5},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1}, {4, 3, 2, 5}},
       .expected_desk_2_windows_after = {{1}, {4}},
       .expected_desk_1_final_active_window = 5},
      {.test_name = "Multiple displays multiple adw 7",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{}, {5}},
       .adw_windows = {1, 4},
       .activate_windows = {1, 2, 3, 4},
       .expected_desk_1_windows_before = {{1}, {2, 3, 4}},
       .expected_desk_2_windows_before = {{1}, {5, 4}},
       .move_windows = {5},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1}, {2, 3, 4, 5}},
       .expected_desk_2_windows_after = {{1}, {4}},
       .expected_desk_1_final_active_window = 5},
      {.test_name = "Multiple displays multiple adw 8",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{5}, {}},
       .adw_windows = {1, 4},
       .activate_windows = {4, 3, 2, 1},
       .expected_desk_1_windows_before = {{1}, {4, 3, 2}},
       .expected_desk_2_windows_before = {{5, 1}, {4}},
       .move_windows = {5},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1, 5}, {4, 3, 2}},
       .expected_desk_2_windows_after = {{1}, {4}},
       .expected_desk_1_final_active_window = 5},
      {.test_name = "Multiple displays multiple adw 9",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{5}, {}},
       .adw_windows = {1, 4},
       .activate_windows = {1, 2, 3, 4},
       .expected_desk_1_windows_before = {{1}, {2, 3, 4}},
       .expected_desk_2_windows_before = {{5, 1}, {4}},
       .move_windows = {5},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1, 5}, {2, 3, 4}},
       .expected_desk_2_windows_after = {{1}, {4}},
       .expected_desk_1_final_active_window = 5},
      {.test_name = "Multiple displays multiple adw on same root 1",
       .multi_display = true,
       .desk_1_windows = {{1, 2}, {3, 4}},
       .desk_2_windows = {{}, {5}},
       .adw_windows = {1, 2},
       .activate_windows = {4, 3, 2, 1},
       .expected_desk_1_windows_before = {{2, 1}, {4, 3}},
       .expected_desk_2_windows_before = {{1, 2}, {5}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{2, 1}, {4, 3}},
       .expected_desk_2_windows_after = {{1, 2}, {5}},
       .expected_desk_1_final_active_window = 1},
      {.test_name = "Multiple displays multiple adw on same root 2",
       .multi_display = true,
       .desk_1_windows = {{1, 2}, {3, 4}},
       .desk_2_windows = {{5}, {}},
       .adw_windows = {1, 2},
       .activate_windows = {4, 3, 2, 1},
       .expected_desk_1_windows_before = {{2, 1}, {4, 3}},
       .expected_desk_2_windows_before = {{5, 1, 2}, {}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{2, 1}, {4, 3}},
       .expected_desk_2_windows_after = {{5, 1, 2}, {}},
       .expected_desk_1_final_active_window = 1},
      {.test_name = "Multiple displays multiple adw on same root 3",
       .multi_display = true,
       .desk_1_windows = {{1, 2}, {3, 4}},
       .desk_2_windows = {{}, {5}},
       .adw_windows = {1, 2},
       .activate_windows = {4, 3, 2, 1},
       .expected_desk_1_windows_before = {{2, 1}, {4, 3}},
       .expected_desk_2_windows_before = {{1, 2}, {5}},
       .move_windows = {5},
       .close_windows = {},
       .expected_desk_1_windows_after = {{2, 1}, {4, 3, 5}},
       .expected_desk_2_windows_after = {{1, 2}, {}},
       .expected_desk_1_final_active_window = 5},
      {.test_name = "Multiple displays multiple adw on same root 4",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{5}, {}},
       .adw_windows = {3, 4},
       .activate_windows = {1, 2, 3, 4},
       .expected_desk_1_windows_before = {{1}, {2, 3, 4}},
       .expected_desk_2_windows_before = {{5}, {3, 4}},
       .move_windows = {},
       .close_windows = {},
       .expected_desk_1_windows_after = {{1}, {2, 3, 4}},
       .expected_desk_2_windows_after = {{5}, {3, 4}},
       .expected_desk_1_final_active_window = 4},
  });
}

TEST_F(DesksPerDeskZOrderTest, MultiDisplayMultipleAdwWithMoving) {
  RunTests(std::vector<PerDeskZOrderTestCase>{
      {.test_name = "Multiple displays moving windows 1",
       .multi_display = true,
       .desk_1_windows = {{1}, {}},
       .desk_2_windows = {{}, {}},
       .adw_windows = {1},
       .activate_windows = {1},
       .expected_desk_1_windows_before = {{1}, {}},
       .expected_desk_2_windows_before = {{1}, {}},
       .move_windows = {},
       .move_windows_to_other_display = {1},
       .close_windows = {},
       .expected_desk_1_windows_after = {{}, {1}},
       .expected_desk_2_windows_after = {{}, {1}},
       .expected_desk_1_final_active_window = 1},
      {.test_name = "Multiple displays moving windows 2",
       .multi_display = true,
       .desk_1_windows = {{1, 2}, {}},
       .desk_2_windows = {{}, {}},
       .adw_windows = {1, 2},
       .activate_windows = {1, 2},
       .expected_desk_1_windows_before = {{1, 2}, {}},
       .expected_desk_2_windows_before = {{1, 2}, {}},
       .move_windows = {},
       .move_windows_to_other_display = {1},
       .close_windows = {},
       .expected_desk_1_windows_after = {{2}, {1}},
       .expected_desk_2_windows_after = {{2}, {1}},
       .expected_desk_1_final_active_window = 1},
      {.test_name = "Multiple displays moving windows 3",
       .multi_display = true,
       .desk_1_windows = {{1}, {2, 3, 4}},
       .desk_2_windows = {{}, {}},
       .adw_windows = {1, 4},
       .activate_windows = {1, 2, 3},
       .expected_desk_1_windows_before = {{1}, {4, 2, 3}},
       .expected_desk_2_windows_before = {{1}, {4}},
       .move_windows = {},
       .move_windows_to_other_display = {1},
       .close_windows = {},
       .expected_desk_1_windows_after = {{}, {4, 2, 3, 1}},
       .expected_desk_2_windows_after = {{}, {1, 4}},
       .expected_desk_1_final_active_window = 1},
  });
}

using FloatAllDesksWithZOrderTest = AshTestBase;

// Tests that floating a window that is already visible on all desks removes its
// z-ordering data, and unfloating the window restores the data.
TEST_F(FloatAllDesksWithZOrderTest, TrackOrderAfterUnfloat) {
  // Start the test with two desks.
  NewDesk();

  // Create a window that is visible on both desks.
  auto window = CreateAppWindow();
  views::Widget::GetWidgetForNativeWindow(window.get())
      ->SetVisibleOnAllWorkspaces(true);
  auto* desks_controller = DesksController::Get();
  ASSERT_EQ(1u, desks_controller->visible_on_all_desks_windows().size());
  ASSERT_TRUE(desks_util::IsWindowVisibleOnAllWorkspaces(window.get()));
  EXPECT_TRUE(desks_util::BelongsToActiveDesk(window.get()));

  // Each desk should be tracking the window's z-ordering data.
  for (auto& desk : desks_controller->desks()) {
    for (aura::Window* root : Shell::GetAllRootWindows()) {
      auto& adw_data = desk->all_desk_window_stacking().at(root);
      ASSERT_EQ(adw_data.size(), 1u);
      ASSERT_EQ(adw_data.begin()->window, window.get());
    }
  }

  // Float the window.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(WindowState::Get(window.get())->IsFloated());

  // The desks are no longer tracking the window's z-ordering data.
  for (auto& desk : desks_controller->desks()) {
    for (aura::Window* root : Shell::GetAllRootWindows()) {
      auto& adw_data = desk->all_desk_window_stacking().at(root);
      ASSERT_EQ(adw_data.size(), 0u);
    }
  }
}

// Tests that sending a floated window to all desks then restoring it to a
// single desk works as intended.
TEST_F(FloatAllDesksWithZOrderTest, FloatThenAllDesks) {
  // Start the test with two desks.
  NewDesk();

  // Create a floated window.
  auto window = CreateAppWindow();
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(WindowState::Get(window.get())->IsFloated());

  // Send the floated window to all desks.
  views::Widget::GetWidgetForNativeWindow(window.get())
      ->SetVisibleOnAllWorkspaces(true);
  auto* desks_controller = DesksController::Get();
  ASSERT_EQ(1u, desks_controller->visible_on_all_desks_windows().size());

  // The desks are not tracking the window's z-ordering data.
  for (auto& desk : desks_controller->desks()) {
    desk->all_desk_window_stacking().empty();
  }

  // Restore the floated window back to a single desk.
  views::Widget::GetWidgetForNativeWindow(window.get())
      ->SetVisibleOnAllWorkspaces(false);
  ASSERT_EQ(0u, desks_controller->visible_on_all_desks_windows().size());
}

// Regression test for b/345815639.
TEST_F(FloatAllDesksWithZOrderTest, AllDesksThenFloatThenClose) {
  // Start the test with two desks.
  NewDesk();

  auto window = CreateAppWindow();

  // Make the window visible on all desks.
  views::Widget::GetWidgetForNativeWindow(window.get())
      ->SetVisibleOnAllWorkspaces(true);

  // Float the window and then close it.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(WindowState::Get(window.get())->IsFloated());
  window.reset();

  // Then switch desks.
  ActivateDesk(DesksController::Get()->GetDeskAtIndex(1));
}

constexpr char kUser1Email[] = "user1@desks";
constexpr char kUser2Email[] = "user2@desks";

class DesksMultiUserTest : public NoSessionAshTestBase,
                           public MultiUserWindowManagerDelegate {
 public:
  DesksMultiUserTest() = default;

  DesksMultiUserTest(const DesksMultiUserTest&) = delete;
  DesksMultiUserTest& operator=(const DesksMultiUserTest&) = delete;

  ~DesksMultiUserTest() override = default;

  MultiUserWindowManager* multi_user_window_manager() {
    return multi_user_window_manager_.get();
  }
  TestingPrefServiceSimple* user_1_prefs() { return user_1_prefs_; }
  TestingPrefServiceSimple* user_2_prefs() { return user_2_prefs_; }

  // AshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    TestSessionControllerClient* session_controller =
        GetSessionControllerClient();
    session_controller->Reset();

    // Inject our own PrefServices for each user which enables us to setup the
    // desks restore data before the user signs in.
    auto user_1_prefs = std::make_unique<TestingPrefServiceSimple>();
    user_1_prefs_ = user_1_prefs.get();
    RegisterUserProfilePrefs(user_1_prefs_->registry(), /*country=*/"",
                             /*for_test=*/true);
    auto user_2_prefs = std::make_unique<TestingPrefServiceSimple>();
    user_2_prefs_ = user_2_prefs.get();
    RegisterUserProfilePrefs(user_2_prefs_->registry(), /*country=*/"",
                             /*for_test=*/true);
    session_controller->AddUserSession(kUser1Email,
                                       user_manager::UserType::kRegular,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(GetUser1AccountId(),
                                           std::move(user_1_prefs));
    session_controller->AddUserSession(kUser2Email,
                                       user_manager::UserType::kRegular,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(GetUser2AccountId(),
                                           std::move(user_2_prefs));
  }

  void TearDown() override {
    multi_user_window_manager_.reset();
    NoSessionAshTestBase::TearDown();
  }

  // MultiUserWindowManagerDelegate:
  void OnWindowOwnerEntryChanged(aura::Window* window,
                                 const AccountId& account_id,
                                 bool was_minimized,
                                 bool teleported) override {}
  void OnTransitionUserShelfToNewAccount() override {}

  AccountId GetUser1AccountId() const {
    return AccountId::FromUserEmail(kUser1Email);
  }

  AccountId GetUser2AccountId() const {
    return AccountId::FromUserEmail(kUser2Email);
  }

  void SwitchActiveUser(const AccountId& account_id) {
    GetSessionControllerClient()->SwitchActiveUser(account_id);
  }

  // Initializes the given |prefs| with a desks restore data of 3 desks, with
  // the third desk named "code", and the rest are default-named.
  void InitPrefsWithDesksRestoreData(PrefService* prefs) {
    InitPrefsWithDesksRestoreData(
        prefs, {std::string(), std::string(), std::string("code")});
  }

  // Initializes the given |prefs| with a desks restore data from the
  // |desk_names| list.
  void InitPrefsWithDesksRestoreData(PrefService* prefs,
                                     std::vector<std::string> desk_names) {
    DCHECK(prefs);
    ScopedListPrefUpdate update(prefs, prefs::kDesksNamesList);
    base::Value::List& pref_data = update.Get();
    ASSERT_TRUE(pref_data.empty());
    for (auto desk_name : desk_names) {
      pref_data.Append(desk_name);
    }
  }

  void SimulateUserLogin(const AccountId& account_id) {
    SwitchActiveUser(account_id);
    multi_user_window_manager_ =
        MultiUserWindowManager::Create(this, account_id);
    MultiUserWindowManagerImpl::Get()->SetAnimationSpeedForTest(
        MultiUserWindowManagerImpl::ANIMATION_SPEED_DISABLED);
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::ACTIVE);
  }

 private:
  std::unique_ptr<MultiUserWindowManager> multi_user_window_manager_;

  raw_ptr<TestingPrefServiceSimple, DanglingUntriaged> user_1_prefs_ = nullptr;
  raw_ptr<TestingPrefServiceSimple, DanglingUntriaged> user_2_prefs_ = nullptr;
};

TEST_F(DesksMultiUserTest, SwitchUsersBackAndForth) {
  SimulateUserLogin(GetUser1AccountId());
  auto* controller = DesksController::Get();
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  Desk* desk_3 = controller->GetDeskAtIndex(2);
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  multi_user_window_manager()->SetWindowOwner(win0.get(), GetUser1AccountId());
  EXPECT_TRUE(win0->IsVisible());
  ActivateDesk(desk_2);
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  multi_user_window_manager()->SetWindowOwner(win1.get(), GetUser1AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_TRUE(win1->IsVisible());

  // Switch to user_2 and expect no windows from user_1 is visible regardless of
  // the desk.
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());

  // Since this is the first time this user logs in, desk_1 will be activated
  // for this user.
  EXPECT_TRUE(desk_1->is_active());

  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win2.get(), GetUser2AccountId());
  EXPECT_TRUE(win2->IsVisible());
  ActivateDesk(desk_3);
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win3.get(), GetUser2AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_TRUE(win3->IsVisible());

  // When switching back to user_1, the active desk should be restored to
  // desk_2.
  SwitchActiveUser(GetUser1AccountId());
  EXPECT_TRUE(desk_2->is_active());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_TRUE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_FALSE(win3->IsVisible());

  // When switching to user_2, the active desk should be restored to desk_3.
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_TRUE(win3->IsVisible());
}

TEST_F(DesksMultiUserTest, RemoveDesks) {
  SimulateUserLogin(GetUser1AccountId());
  // Create two desks with several windows with different app types that
  // belong to different users.
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  multi_user_window_manager()->SetWindowOwner(win0.get(), GetUser1AccountId());
  EXPECT_TRUE(win0->IsVisible());
  ActivateDesk(desk_2);
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto win2 =
      CreateAppWindow(gfx::Rect(50, 50, 200, 200), chromeos::AppType::ARC_APP);
  // Non-app window.
  auto win3 =
      CreateAppWindow(gfx::Rect(50, 50, 200, 200), chromeos::AppType::NON_APP);
  multi_user_window_manager()->SetWindowOwner(win2.get(), GetUser1AccountId());
  multi_user_window_manager()->SetWindowOwner(win1.get(), GetUser1AccountId());
  multi_user_window_manager()->SetWindowOwner(win3.get(), GetUser1AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_TRUE(win1->IsVisible());
  EXPECT_TRUE(win2->IsVisible());
  EXPECT_TRUE(win3->IsVisible());

  // Switch to user_2 and expect no windows from user_1 is visible regardless of
  // the desk.
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_FALSE(win3->IsVisible());

  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win4.get(), GetUser2AccountId());
  EXPECT_TRUE(win4->IsVisible());
  ActivateDesk(desk_2);
  auto win5 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win5.get(), GetUser2AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_FALSE(win3->IsVisible());
  EXPECT_FALSE(win4->IsVisible());
  EXPECT_TRUE(win5->IsVisible());

  // Delete desk_2, and expect all app windows move to desk_1.
  RemoveDesk(desk_2);
  EXPECT_TRUE(desk_1->is_active());
  auto* desk_1_container =
      desk_1->GetDeskContainerForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(desk_1_container, win0->parent());
  EXPECT_EQ(desk_1_container, win1->parent());
  EXPECT_EQ(desk_1_container, win2->parent());
  // The non-app window didn't move.
  EXPECT_NE(desk_1_container, win3->parent());
  EXPECT_EQ(desk_1_container, win4->parent());
  EXPECT_EQ(desk_1_container, win5->parent());

  // Only user_2's window are visible.
  EXPECT_TRUE(win4->IsVisible());
  EXPECT_TRUE(win5->IsVisible());

  // The non-app window will always be hidden.
  EXPECT_FALSE(win3->IsVisible());

  // Switch to user_1 and expect the correct windows' visibility.
  SwitchActiveUser(GetUser1AccountId());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_TRUE(win0->IsVisible());
  EXPECT_TRUE(win1->IsVisible());
  EXPECT_TRUE(win2->IsVisible());
  EXPECT_FALSE(win3->IsVisible());

  // Create two more desks, switch to user_2, and activate the third desk.
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  desk_2 = controller->GetDeskAtIndex(1);
  Desk* desk_3 = controller->GetDeskAtIndex(2);
  SwitchActiveUser(GetUser2AccountId());
  ActivateDesk(desk_3);
  auto win6 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win5.get(), GetUser2AccountId());

  // Switch back to user_1, and remove the first desk. When switching back to
  // user_2 after that, we should see that what used to be the third desk is now
  // active.
  SwitchActiveUser(GetUser1AccountId());
  EXPECT_TRUE(desk_1->is_active());
  RemoveDesk(desk_1);
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_TRUE(win6->IsVisible());
}

TEST_F(DesksMultiUserTest, SwitchingUsersEndsOverview) {
  SimulateUserLogin(GetUser1AccountId());
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(EnterOverview());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

using DesksRestoreMultiUserTest = DesksMultiUserTest;

TEST_F(DesksRestoreMultiUserTest, DesksRestoredFromPrimaryUserPrefsOnly) {
  constexpr int kDefaultActiveDesk = 0;
  constexpr int kUser1StoredActiveDesk = 2;
  InitPrefsWithDesksRestoreData(user_1_prefs());

  // Set the primary user1's active desk prefs to kUser1StoredActiveDesk.
  user_1_prefs()->SetInteger(prefs::kDesksActiveDesk, kUser1StoredActiveDesk);

  SimulateUserLogin(GetUser1AccountId());
  // User 1 is the first to login, hence the primary user.
  auto* controller = DesksController::Get();
  const auto& desks = controller->desks();

  auto verify_desks = [&](const std::string& trace_name) {
    SCOPED_TRACE(trace_name);
    EXPECT_EQ(3u, desks.size());
    EXPECT_EQ(u"Desk 1", desks[0]->name());
    EXPECT_EQ(u"Desk 2", desks[1]->name());
    EXPECT_EQ(u"code", desks[2]->name());
    // Restored non-default names should be marked as `set_by_user`.
    EXPECT_FALSE(desks[0]->is_name_set_by_user());
    EXPECT_FALSE(desks[1]->is_name_set_by_user());
    EXPECT_TRUE(desks[2]->is_name_set_by_user());
  };

  verify_desks("Before switching users");
  // The primary user1 should restore the saved active desk from its pref.
  EXPECT_EQ(desks[kUser1StoredActiveDesk]->container_id(),
            desks_util::GetActiveDeskContainerId());

  // Switching users should not change anything as restoring happens only at
  // the time when the first user signs in.
  SwitchActiveUser(GetUser2AccountId());
  verify_desks("After switching users");
  // The secondary user2 should start with a default active desk.
  EXPECT_EQ(desks[kDefaultActiveDesk]->container_id(),
            desks_util::GetActiveDeskContainerId());

  // Activating the second desk in the secondary user session should not
  // affect the primary user1's active desk pref. Moreover, switching back to
  // user1 session should activate the user1's previously active desk
  // correctly.
  ActivateDesk(desks[1].get());
  EXPECT_EQ(user_1_prefs()->GetInteger(prefs::kDesksActiveDesk),
            kUser1StoredActiveDesk);
  SwitchActiveUser(GetUser1AccountId());
  EXPECT_EQ(desks[kUser1StoredActiveDesk]->container_id(),
            desks_util::GetActiveDeskContainerId());
}

TEST_F(DesksRestoreMultiUserTest,
       ChangesMadeBySecondaryUserAffectsOnlyPrimaryUserPrefs) {
  InitPrefsWithDesksRestoreData(user_1_prefs());
  SimulateUserLogin(GetUser1AccountId());

  auto* controller = DesksController::Get();
  const auto& desks = controller->desks();
  ASSERT_EQ(3u, desks.size());

  // Activate the last desk and verify that this is reflected in prefs.
  ActivateDesk(controller->desks().back().get());
  EXPECT_EQ(user_1_prefs()->GetInteger(prefs::kDesksActiveDesk), 2);

  // Switch to user 2 (secondary) and make some desks changes. Those changes
  // should be persisted to user 1's prefs only.
  SwitchActiveUser(GetUser2AccountId());

  // Create a fourth desk.
  NewDesk();
  EXPECT_THAT(GetDeskRestoreNames(user_1_prefs()),
              ElementsAre("", "", "code", ""));

  // User 2's prefs are unaffected (empty list of desks).
  EXPECT_THAT(GetDeskRestoreNames(user_2_prefs()), ElementsAre());

  // Delete the second desk.
  RemoveDesk(desks[1].get());
  EXPECT_THAT(GetDeskRestoreNames(user_1_prefs()), ElementsAre("", "code", ""));
  EXPECT_THAT(GetDeskRestoreNames(user_2_prefs()), ElementsAre());
  // This should adjust the active desk prefs for the primary user.
  EXPECT_EQ(user_1_prefs()->GetInteger(prefs::kDesksActiveDesk), 1);

  // Move the third desk to the second to test desks reordering.
  controller->ReorderDesk(/*old_index=*/2, /*new_index=*/1);
  EXPECT_THAT(GetDeskRestoreNames(user_1_prefs()), ElementsAre("", "", "code"));
  EXPECT_THAT(GetDeskRestoreNames(user_2_prefs()), ElementsAre());
  // This should again adjust the active desk prefs for the primary user.
  EXPECT_EQ(user_1_prefs()->GetInteger(prefs::kDesksActiveDesk), 2);
}

// Tests that desks reordering updates workspaces of all windows in affected
// desks for all simultaneously logged-in users.
TEST_F(DesksRestoreMultiUserTest,
       DeskIndexChangesMadeByActiveUserAffectsAllUsers) {
  // Setup two users and four desks with one window in each desk --------------
  // Create user1 and user2 with four shared desks named numerically by their
  // initial order. Set the user1 active desk to the fourth desk.
  const int n_desks_per_user = 4;
  int user_1_active_desk_index = 3;
  user_1_prefs()->SetInteger(prefs::kDesksActiveDesk, user_1_active_desk_index);
  InitPrefsWithDesksRestoreData(user_1_prefs(),
                                std::vector<std::string>{"0", "1", "2", "3"});
  SimulateUserLogin(GetUser1AccountId());
  auto* controller = DesksController::Get();
  const auto& desks = controller->desks();
  ASSERT_EQ(4u, desks.size());

  // Switch to user2 and activate the third desk. Thus, the desks initial state
  // is [0, 1, 2**, 3*] where * and ** label active desks of user1 and user2.
  EXPECT_EQ(desks[user_1_active_desk_index]->container_id(),
            desks_util::GetActiveDeskContainerId());
  int user_2_active_desk_index = 2;
  SwitchActiveUser(GetUser2AccountId());
  ActivateDesk(desks[user_2_active_desk_index].get());
  EXPECT_EQ(desks[user_2_active_desk_index]->container_id(),
            desks_util::GetActiveDeskContainerId());
  EXPECT_EQ(user_1_active_desk_index,
            user_1_prefs()->GetInteger(prefs::kDesksActiveDesk));

  // For each user's desk, create a window and set window workspace property.
  std::vector<std::unique_ptr<aura::Window>> user1_windows;
  std::vector<std::unique_ptr<aura::Window>> user2_windows;
  for (int i = 0; i < n_desks_per_user; i++) {
    user1_windows.push_back(CreateAppWindow());
    user2_windows.push_back(CreateAppWindow());
    multi_user_window_manager()->SetWindowOwner(user1_windows[i].get(),
                                                GetUser1AccountId());
    multi_user_window_manager()->SetWindowOwner(user2_windows[i].get(),
                                                GetUser2AccountId());
    controller->SendToDeskAtIndex(user1_windows[i].get(), i);
    controller->SendToDeskAtIndex(user2_windows[i].get(), i);
    EXPECT_EQ(i,
              user1_windows[i]->GetProperty(aura::client::kWindowWorkspaceKey));
    EXPECT_EQ(i,
              user2_windows[i]->GetProperty(aura::client::kWindowWorkspaceKey));
  }
  // Done setup four desks with the numerical desk names ----------------------

  // |desk_names_as_ints| represents the numerical desk names in desks order
  // after a series of desks reordering. The initial order is [0, 1, 2, 3].
  auto check_window_workspaces =
      [&](const std::vector<int> desk_names_as_ints) {
        DCHECK(n_desks_per_user == desk_names_as_ints.size());
        for (int desk_index = 0; desk_index < n_desks_per_user; desk_index++) {
          int desk_name_as_int = desk_names_as_ints[desk_index];
          EXPECT_EQ(desk_index, user1_windows[desk_name_as_int]->GetProperty(
                                    aura::client::kWindowWorkspaceKey));
          EXPECT_EQ(desk_index, user2_windows[desk_name_as_int]->GetProperty(
                                    aura::client::kWindowWorkspaceKey));
        }
      };

  // 1. Test that desks reordering from the secondary user updates both users'
  // saved active desks correctly. Move the fourth desk to the second, so the
  // desks state become [0, 3*, 1, 2**].
  controller->ReorderDesk(/*old_index=*/3, /*new_index=*/1);
  user_1_active_desk_index = 1;
  user_2_active_desk_index = 3;
  check_window_workspaces(std::vector<int>{0, 3, 1, 2});
  EXPECT_THAT(GetDeskRestoreNames(user_1_prefs()),
              ElementsAre("0", "3", "1", "2"));
  EXPECT_THAT(GetDeskRestoreNames(user_2_prefs()), ElementsAre());
  EXPECT_EQ(user_1_active_desk_index,
            user_1_prefs()->GetInteger(prefs::kDesksActiveDesk));
  EXPECT_EQ(desks[user_2_active_desk_index]->container_id(),
            desks_util::GetActiveDeskContainerId());

  // Switch to the primary user1.
  SwitchActiveUser(GetUser1AccountId());
  EXPECT_EQ(desks[user_1_active_desk_index]->container_id(),
            desks_util::GetActiveDeskContainerId());

  // 2. Test that desks reordering from the primary user updates both users'
  // saved active desks correctly. Move the first desk to the fourth, so the
  // desks state become [3*, 1, 2**, 0].
  controller->ReorderDesk(/*old_index=*/0, /*new_index=*/3);
  user_1_active_desk_index = 0;
  user_2_active_desk_index = 2;
  check_window_workspaces(std::vector<int>{3, 1, 2, 0});
  EXPECT_THAT(GetDeskRestoreNames(user_1_prefs()),
              ElementsAre("3", "1", "2", "0"));
  EXPECT_THAT(GetDeskRestoreNames(user_2_prefs()), ElementsAre());
  EXPECT_EQ(user_1_active_desk_index,
            user_1_prefs()->GetInteger(prefs::kDesksActiveDesk));
  EXPECT_EQ(desks[user_1_active_desk_index]->container_id(),
            desks_util::GetActiveDeskContainerId());
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_EQ(desks[user_2_active_desk_index]->container_id(),
            desks_util::GetActiveDeskContainerId());
}

}  // namespace

// Simulates the same behavior of event rewriting that key presses go through.
class DesksAcceleratorsTest : public DesksTest {
 public:
  DesksAcceleratorsTest() = default;

  DesksAcceleratorsTest(const DesksAcceleratorsTest&) = delete;
  DesksAcceleratorsTest& operator=(const DesksAcceleratorsTest&) = delete;

  ~DesksAcceleratorsTest() override = default;

  // DesksTest:
  void SetUp() override {
    DesksTest::SetUp();

    auto* event_rewriter_controller = EventRewriterController::Get();
    auto event_rewriter = std::make_unique<ui::EventRewriterAsh>(
        &event_rewriter_delegate_, Shell::Get()->keyboard_capability(),
        Shell::Get()->sticky_keys_controller(), false, &fake_ime_keyboard_);
    event_rewriter_controller->AddEventRewriter(std::move(event_rewriter));
  }

  void SendAccelerator(ui::KeyboardCode key_code, int flags) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->PressKey(key_code, flags);
    generator->ReleaseKey(key_code, flags);
  }

  // Moves the overview focus to the next item.
  void MoveOverviewFocusRing(OverviewSession* session) {
    session->Move(/*reverse=*/false);
  }

 private:
  ui::test::FakeEventRewriterAshDelegate event_rewriter_delegate_;
  input_method::FakeImeKeyboard fake_ime_keyboard_;
};

namespace {

TEST_P(DesksAcceleratorsTest, NewDesk) {
  auto* controller = DesksController::Get();
  // It's possible to add up to kMaxNumberOfDesks desks using the
  // shortcut.
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  for (size_t num_desks = 1; num_desks < desks_util::GetMaxNumberOfDesks();
       ++num_desks) {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_OEM_PLUS, flags);
    waiter.Wait();
    // The newly created desk should be activated.
    ASSERT_EQ(num_desks + 1, controller->desks().size());
    EXPECT_TRUE(controller->desks().back()->is_active());
  }

  // When we reach the limit, the shortcut does nothing.
  EXPECT_EQ(desks_util::GetMaxNumberOfDesks(), controller->desks().size());
  SendAccelerator(ui::VKEY_OEM_PLUS, flags);
  EXPECT_EQ(desks_util::GetMaxNumberOfDesks(), controller->desks().size());
}

TEST_P(DesksAcceleratorsTest, CannotRemoveLastDesk) {
  auto* controller = DesksController::Get();
  // Removing the last desk is not possible.
  ASSERT_EQ(1u, controller->desks().size());
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  SendAccelerator(ui::VKEY_OEM_MINUS, flags);
  ASSERT_EQ(1u, controller->desks().size());
}

TEST_P(DesksAcceleratorsTest, RemoveDesk) {
  auto* controller = DesksController::Get();
  // Create a few desks and remove them outside and inside overview using the
  // shortcut.
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  Desk* desk_3 = controller->GetDeskAtIndex(2);
  EXPECT_TRUE(desk_1->is_active());
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  DeskSwitchAnimationWaiter waiter;
  SendAccelerator(ui::VKEY_OEM_MINUS, flags);
  waiter.Wait();
  ASSERT_EQ(2u, controller->desks().size());
  EXPECT_TRUE(desk_2->is_active());

  // Using the accelerator doesn't result in exiting overview.
  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  SendAccelerator(ui::VKEY_OEM_MINUS, flags);
  ASSERT_EQ(1u, controller->desks().size());
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_TRUE(overview_controller->InOverviewSession());
}

TEST_P(DesksAcceleratorsTest, RemoveRightmostDesk) {
  auto* controller = DesksController::Get();
  // Create a few desks and remove them outside and inside overview using the
  // shortcut.
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  Desk* desk_3 = controller->GetDeskAtIndex(2);
  ActivateDesk(desk_3);
  EXPECT_TRUE(desk_3->is_active());
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_OEM_MINUS, flags);
    waiter.Wait();
  }
  ASSERT_EQ(2u, controller->desks().size());
  EXPECT_TRUE(desk_2->is_active());
  {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_OEM_MINUS, flags);
    waiter.Wait();
  }
  ASSERT_EQ(1u, controller->desks().size());
  EXPECT_TRUE(desk_1->is_active());
}

TEST_P(DesksAcceleratorsTest, LeftRightDeskActivation) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  EXPECT_TRUE(desk_1->is_active());
  // No desk on left, nothing should happen.
  const int flags = ui::EF_COMMAND_DOWN;
  SendAccelerator(ui::VKEY_OEM_4, flags);
  EXPECT_TRUE(desk_1->is_active());

  // Go right until there're no more desks on the right.
  {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_OEM_6, flags);
    waiter.Wait();
    EXPECT_TRUE(desk_2->is_active());
  }

  // Nothing happens.
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_TRUE(desk_2->is_active());

  // Go back left.
  {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_OEM_4, flags);
    waiter.Wait();
    EXPECT_TRUE(desk_1->is_active());
  }
}

TEST_P(DesksAcceleratorsTest, MoveWindowLeftRightDesk) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  EXPECT_TRUE(desk_1->is_active());

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Moving window left when this is the left-most desk. Nothing happens.
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  SendAccelerator(ui::VKEY_OEM_4, flags);
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
  EXPECT_TRUE(DoesActiveDeskContainWindow(window.get()));

  // Move window right, it should be deactivated.
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());
  EXPECT_TRUE(desk_1->windows().empty());
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));

  // No more active windows on this desk, nothing happens.
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_TRUE(desk_1->windows().empty());
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());

  // Activate desk 2, and do the same set of tests.
  ActivateDesk(desk_2);
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Move right does nothing.
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  SendAccelerator(ui::VKEY_OEM_4, flags);
  EXPECT_TRUE(desk_2->windows().empty());
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());
  EXPECT_TRUE(base::Contains(desk_1->windows(), window.get()));
}

TEST_P(DesksAcceleratorsTest, MoveWindowLeftRightDeskOverview) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  EXPECT_TRUE(desk_1->is_active());

  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  // In overview, while no window is focused, nothing should happen.
  const size_t num_windows_before = desk_1->windows().size();
  EXPECT_TRUE(desk_2->windows().empty());
  SendAccelerator(ui::VKEY_OEM_6, flags);
  ASSERT_EQ(num_windows_before, desk_1->windows().size());
  EXPECT_TRUE(desk_2->windows().empty());

  auto* overview_session = overview_controller->overview_session();
  ASSERT_TRUE(overview_session);
  // It's possible to move the focused window. `Move()` will cycle through
  // the desk items first, so call it until we are focusing an `OverviewItem`.
  while (!overview_session->GetFocusedWindow()) {
    MoveOverviewFocusRing(overview_session);
  }
  EXPECT_EQ(win0.get(), overview_session->GetFocusedWindow());
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_FALSE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_TRUE(base::Contains(desk_2->windows(), win0.get()));
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // The focus ring should move to the next window if we call
  // `MoveOverviewFocusRing()` again.
  MoveOverviewFocusRing(overview_session);
  EXPECT_EQ(win1.get(), overview_session->GetFocusedWindow());
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_FALSE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_TRUE(base::Contains(desk_2->windows(), win1.get()));
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Nothing is focused.
  EXPECT_FALSE(overview_session->GetFocusedWindow());
}

TEST_P(DesksAcceleratorsTest, CannotMoveAlwaysOnTopWindows) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  EXPECT_TRUE(desk_1->is_active());

  // An always-on-top window does not belong to any desk and hence cannot be
  // removed.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  win0->SetProperty(aura::client::kZOrderingKey,
                    ui::ZOrderLevel::kFloatingWindow);
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());
  EXPECT_FALSE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_FALSE(controller->MoveWindowFromActiveDeskTo(
      win0.get(), desk_2, win0->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kDragAndDrop));
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  SendAccelerator(ui::VKEY_OEM_4, flags);
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());
  EXPECT_TRUE(win0->IsVisible());

  // It remains visible even after switching desks.
  ActivateDesk(desk_2);
  EXPECT_TRUE(win0->IsVisible());
}

// Tests that hitting an acclerator to switch desks does not cause a crash if we
// are already at an edge desk. Regression test for https://crbug.com/1159068.
TEST_P(DesksAcceleratorsTest, HitAcceleratorWhenAlreadyAtEdge) {
  NewDesk();

  // Enable animations so that we can make sure that they occur.
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // First go right. Wait until the ending screenshot is taken.
  const int flags = ui::EF_COMMAND_DOWN;
  SendAccelerator(ui::VKEY_OEM_6, flags);

  DeskAnimationBase* animation = DesksController::Get()->animation();
  ASSERT_TRUE(animation);
  base::RunLoop run_loop;
  auto* desk_switch_animator =
      animation->GetDeskSwitchAnimatorAtIndexForTesting(0);
  ASSERT_TRUE(desk_switch_animator);
  RootWindowDeskSwitchAnimatorTestApi(desk_switch_animator)
      .SetOnEndingScreenshotTakenCallback(run_loop.QuitClosure());
  run_loop.Run();

  // Tap the accelerator to go left to desk 1, then tap again. There should be
  // no crash.
  SendAccelerator(ui::VKEY_OEM_4, flags);
  SendAccelerator(ui::VKEY_OEM_4, flags);
}

// Tests that the assign to all desks shortcut works as expected and that its
// metrics are recorded properly.
TEST_P(DesksAcceleratorsTest, AssignToAllDesksShortcut) {
  base::HistogramTester histogram_tester;

  // Create two new desks.
  NewDesk();
  NewDesk();
  auto* controller = DesksController::Get();
  ASSERT_EQ(3u, controller->desks().size());

  // Create a window and assign it to all desks via
  // chromeos::MoveToDesksMenuDelegate. This simulates assigning it to all desks
  // via the context menu. This should record to the "Assign to desk menu"
  // bucket.
  auto window_1 = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto* widget_1 = views::Widget::GetWidgetForNativeWindow(window_1.get());
  auto menu_delegate =
      std::make_unique<chromeos::MoveToDesksMenuDelegate>(widget_1);
  menu_delegate->ExecuteCommand(
      chromeos::MoveToDesksMenuModel::CommandId::TOGGLE_ASSIGN_TO_ALL_DESKS,
      /*event_flags=*/0);
  histogram_tester.ExpectBucketCount(
      chromeos::kDesksAssignToAllDesksSourceHistogramName,
      chromeos::DesksAssignToAllDesksSource::kMoveToDeskMenu, 1);
  EXPECT_TRUE(widget_1->IsVisibleOnAllWorkspaces());

  // Create another window and assign it to all desks via the keyboard shortcut.
  // This should record to the "Assign to all desks keyboard shortcut menu"
  // bucket.
  auto window_2 = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  ASSERT_EQ(window_2.get(), window_util::GetActiveWindow());
  SendAccelerator(ui::VKEY_A, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  histogram_tester.ExpectBucketCount(
      chromeos::kDesksAssignToAllDesksSourceHistogramName,
      chromeos::DesksAssignToAllDesksSource::kKeyboardShortcut, 1);
  EXPECT_TRUE(views::Widget::GetWidgetForNativeWindow(window_2.get())
                  ->IsVisibleOnAllWorkspaces());
}

// Tests that the indexed-desk activation keyboard shortcut works properly.
TEST_P(DesksAcceleratorsTest, IndexedDeskActivationShortcut) {
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  constexpr char kDesksSwitchHistogramName[] = "Ash.Desks.DesksSwitch";
  base::HistogramTester histogram_tester;

  // Create three new desks.
  NewDesk();
  NewDesk();
  NewDesk();
  auto* controller = DesksController::Get();
  auto& desks = controller->desks();
  ASSERT_EQ(4u, desks.size());

  // Switch to the third desk via the keyboard shortcut.
  ASSERT_TRUE(desks[0]->is_active());
  {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_3, flags);
    waiter.Wait();
  }
  EXPECT_TRUE(desks[2]->is_active());
  histogram_tester.ExpectBucketCount(
      kDesksSwitchHistogramName, DesksSwitchSource::kIndexedDeskSwitchShortcut,
      1);

  // Try to switch to the non-existent fifth desk via the keyboard shortcut.
  SendAccelerator(ui::VKEY_5, flags);
  EXPECT_TRUE(desks[2]->is_active());
  histogram_tester.ExpectBucketCount(
      kDesksSwitchHistogramName, DesksSwitchSource::kIndexedDeskSwitchShortcut,
      1);

  // Switch to the second desk via the keyboard shortcut.
  {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_2, flags);
    waiter.Wait();
  }
  EXPECT_TRUE(desks[1]->is_active());
  histogram_tester.ExpectBucketCount(
      kDesksSwitchHistogramName, DesksSwitchSource::kIndexedDeskSwitchShortcut,
      2);

  // Open overview and switch to the active desk. This should close overview.
  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
  SendAccelerator(ui::VKEY_2, flags);
  EXPECT_TRUE(desks[1]->is_active());
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
  histogram_tester.ExpectBucketCount(
      kDesksSwitchHistogramName, DesksSwitchSource::kIndexedDeskSwitchShortcut,
      2);
}

class PerDeskShelfTest : public AshTestBase,
                         public ::testing::WithParamInterface<bool> {
 public:
  PerDeskShelfTest() = default;
  PerDeskShelfTest(const PerDeskShelfTest&) = delete;
  PerDeskShelfTest& operator=(const PerDeskShelfTest&) = delete;
  ~PerDeskShelfTest() override = default;

  // AshTestBase:
  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(features::kPerDeskShelf);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kPerDeskShelf);
    }

    AshTestBase::SetUp();
  }

  // Creates and returns an app window that is asscoaited with a shelf item with
  // |type|.
  std::unique_ptr<aura::Window> CreateAppWithShelfItem(ShelfItemType type) {
    auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
    const ash::ShelfID shelf_id(base::StringPrintf("%d", current_shelf_id_++));
    window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
    window->SetProperty(ash::kAppIDKey, shelf_id.app_id);
    window->SetProperty<int>(ash::kShelfItemTypeKey, type);
    ShelfItem item;
    item.status = ShelfItemStatus::STATUS_RUNNING;
    item.type = type;
    item.id = shelf_id;
    // Non-empty title for the shelf icon is important to not crash the tooltip.
    // Please see b/293869853.
    item.title = u"Test Window";
    ShelfModel::Get()->Add(item,
                           std::make_unique<TestShelfItemDelegate>(item.id));
    return window;
  }

  bool IsPerDeskShelfEnabled() const { return GetParam(); }

  ShelfView* GetShelfView() const {
    return GetPrimaryShelf()->GetShelfViewForTesting();
  }

  // Returns the index of the shelf item associated with the given |window| or
  // -1 if no such item exists.
  int GetShelfItemIndexForWindow(aura::Window* window) const {
    const auto shelf_id =
        ShelfID::Deserialize(window->GetProperty(kShelfIDKey));
    EXPECT_FALSE(shelf_id.IsNull());
    return ShelfModel::Get()->ItemIndexByID(shelf_id);
  }

  // Verifies that the visibility of the shelf item view associated with the
  // given |window| is equal to the given |expected_visibility|.
  void VerifyViewVisibility(aura::Window* window,
                            bool expected_visibility) const {
    const size_t index = GetShelfItemIndexForWindow(window);
    auto* shelf_view = GetShelfView();
    auto* view_model = shelf_view->view_model();

    views::View* item_view = view_model->view_at(index);
    const bool contained_in_visible_indices =
        base::Contains(shelf_view->visible_views_indices(), index);

    EXPECT_EQ(expected_visibility, item_view->GetVisible());
    EXPECT_EQ(expected_visibility, contained_in_visible_indices);
  }

  void MoveWindowFromActiveDeskTo(aura::Window* window,
                                  Desk* target_desk) const {
    DesksController::Get()->MoveWindowFromActiveDeskTo(
        window, target_desk, window->GetRootWindow(),
        DesksMoveWindowFromActiveDeskSource::kDragAndDrop);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  int current_shelf_id_ = 0;
};

TEST_P(PerDeskShelfTest, MoveWindowOutOfActiveDesk) {
  auto* controller = DesksController::Get();
  NewDesk();

  // Create three app windows; a browser window, a pinned app, and a normal app.
  auto win0 = CreateAppWithShelfItem(ShelfItemType::TYPE_BROWSER_SHORTCUT);
  aura::Window* browser = win0.get();
  auto win1 = CreateAppWithShelfItem(ShelfItemType::TYPE_PINNED_APP);
  aura::Window* pinned = win1.get();
  auto win2 = CreateAppWithShelfItem(ShelfItemType::TYPE_APP);
  aura::Window* app = win2.get();

  // All items should be visible.
  VerifyViewVisibility(browser, true);
  VerifyViewVisibility(pinned, true);
  VerifyViewVisibility(app, true);

  // Move the app window, it should be removed from the shelf if the feature is
  // enabled.
  const bool visible_in_per_desk_shelf = IsPerDeskShelfEnabled() ? false : true;
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  MoveWindowFromActiveDeskTo(app, desk_2);
  VerifyViewVisibility(browser, true);
  VerifyViewVisibility(pinned, true);
  VerifyViewVisibility(app, visible_in_per_desk_shelf);

  // Move the pinned app and the browser window, they should remain visible on
  // the shelf even though they're on an inactive desk now.
  MoveWindowFromActiveDeskTo(pinned, desk_2);
  MoveWindowFromActiveDeskTo(browser, desk_2);
  VerifyViewVisibility(browser, true);
  VerifyViewVisibility(pinned, true);
  VerifyViewVisibility(app, visible_in_per_desk_shelf);
}

TEST_P(PerDeskShelfTest, DeskSwitching) {
  // Create two more desks, so total is three.
  auto* controller = DesksController::Get();
  NewDesk();
  NewDesk();

  // On desk_1, create a browser and a normal app.
  auto win0 = CreateAppWithShelfItem(ShelfItemType::TYPE_BROWSER_SHORTCUT);
  aura::Window* browser = win0.get();
  auto win1 = CreateAppWithShelfItem(ShelfItemType::TYPE_APP);
  aura::Window* app1 = win1.get();

  // Switch to desk_2, only the browser app should be visible on the shelf if
  // the feature is enabled.
  const bool visible_in_per_desk_shelf = IsPerDeskShelfEnabled() ? false : true;
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  ActivateDesk(desk_2);
  VerifyViewVisibility(browser, true);
  VerifyViewVisibility(app1, visible_in_per_desk_shelf);

  // On desk_2, create a pinned app.
  auto win2 = CreateAppWithShelfItem(ShelfItemType::TYPE_PINNED_APP);
  aura::Window* pinned = win2.get();
  VerifyViewVisibility(pinned, true);

  // Switch to desk_3, only the browser and the pinned app should be visible on
  // the shelf if the feature is enabled.
  Desk* desk_3 = controller->GetDeskAtIndex(2);
  ActivateDesk(desk_3);
  VerifyViewVisibility(browser, true);
  VerifyViewVisibility(app1, visible_in_per_desk_shelf);
  VerifyViewVisibility(pinned, true);

  // On desk_3, create a normal app, then switch back to desk_1. app1 should
  // show again on the shelf.
  auto win3 = CreateAppWithShelfItem(ShelfItemType::TYPE_APP);
  aura::Window* app2 = win3.get();
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  ActivateDesk(desk_1);
  VerifyViewVisibility(browser, true);
  VerifyViewVisibility(app1, true);
  VerifyViewVisibility(pinned, true);
  VerifyViewVisibility(app2, visible_in_per_desk_shelf);
}

TEST_P(PerDeskShelfTest, RemoveInactiveDesk) {
  auto* controller = DesksController::Get();
  NewDesk();

  // On desk_1, create two apps.
  auto win0 = CreateAppWithShelfItem(ShelfItemType::TYPE_APP);
  aura::Window* app1 = win0.get();
  auto win1 = CreateAppWithShelfItem(ShelfItemType::TYPE_APP);
  aura::Window* app2 = win1.get();

  // Switch to desk_2, no apps should show on the shelf if the feature is
  // enabled.
  const bool visible_in_per_desk_shelf = IsPerDeskShelfEnabled() ? false : true;
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  ActivateDesk(desk_2);
  VerifyViewVisibility(app1, visible_in_per_desk_shelf);
  VerifyViewVisibility(app2, visible_in_per_desk_shelf);

  // Remove desk_1 (inactive), apps should now show on the shelf.
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  RemoveDesk(desk_1);
  VerifyViewVisibility(app1, true);
  VerifyViewVisibility(app2, true);
}

TEST_P(PerDeskShelfTest, RemoveActiveDesk) {
  auto* controller = DesksController::Get();
  NewDesk();

  // On desk_1, create an app.
  auto win0 = CreateAppWithShelfItem(ShelfItemType::TYPE_APP);
  aura::Window* app1 = win0.get();

  // Switch to desk_2, no apps should show on the shelf if the feature is
  // enabled.
  const bool visible_in_per_desk_shelf = IsPerDeskShelfEnabled() ? false : true;
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  ActivateDesk(desk_2);
  VerifyViewVisibility(app1, visible_in_per_desk_shelf);

  // Create another app on desk_2.
  auto win1 = CreateAppWithShelfItem(ShelfItemType::TYPE_APP);
  aura::Window* app2 = win1.get();

  // Remove desk_2 (active), all apps should now show on the shelf.
  RemoveDesk(desk_2);
  VerifyViewVisibility(app1, true);
  VerifyViewVisibility(app2, true);
}

TEST_P(PerDeskShelfTest, ShelfViewTransformUpdatedForScrollWhenSwitchingDesks) {
  ScrollableShelfView* scrollable_shelf_view = GetPrimaryShelf()
                                                   ->shelf_widget()
                                                   ->hotseat_widget()
                                                   ->scrollable_shelf_view();
  ShelfView* shelf_view = scrollable_shelf_view->shelf_view();
  ShelfViewTestAPI shelf_view_test_api(shelf_view);
  shelf_view_test_api.SetAnimationDuration(base::Milliseconds(1));

  NewDesk();

  // Create apps running on the active desk, and not pinned to the shelf, until
  // the right or bottom scroll arrow appears.
  ScrollArrowView* right_arrow = scrollable_shelf_view->right_arrow();
  std::vector<std::unique_ptr<aura::Window>> windows;
  while (!right_arrow->GetVisible()) {
    windows.push_back(CreateAppWithShelfItem(ShelfItemType::TYPE_APP));
    shelf_view_test_api.RunMessageLoopUntilAnimationsDone();
  }

  // Scroll the shelf.
  LeftClickOn(right_arrow);
  shelf_view_test_api.RunMessageLoopUntilAnimationsDone();
  const gfx::Transform scrolled_transform = shelf_view->GetTransform();
  EXPECT_FALSE(scrolled_transform.IsIdentity());

  // Switch desks.
  ActivateDesk(DesksController::Get()->GetDeskAtIndex(1));
  if (IsPerDeskShelfEnabled()) {
    EXPECT_TRUE(shelf_view->GetTransform().IsIdentity());
  } else {
    EXPECT_EQ(scrolled_transform, shelf_view->GetTransform());
  }
}

// Tests desks name nudges, i.e. when a user creates a new desk, focus + clear
// the new desk's renaming textfield.
TEST_P(DesksTest, NameNudges) {
  UpdateDisplay("1200x800");
  auto* controller = DesksController::Get();

  // Start overview.
  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  // Hover over the new desk button.
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view->IsZeroState());

  LeftClickOn(desks_bar_view->default_desk_button());
  RunScheduledLayoutForAllOverviewDeskBars();
  EXPECT_EQ(1u, desks_bar_view->mini_views().size());

  auto* new_desk_button = desks_bar_view->new_desk_button();
  EXPECT_TRUE(new_desk_button->GetEnabled());

  // Click on the new desk button until the max number of desks is created. Each
  // time a new desk is created the new desk's name view should have focus, be
  // empty. Also, the previous desk should be left with a default name.
  for (size_t i = 1; i < desks_util::GetMaxNumberOfDesks(); ++i) {
    LeftClickOn(new_desk_button);
    RunScheduledLayoutForAllOverviewDeskBars();
    auto* desk_name_view = desks_bar_view->mini_views()[i]->desk_name_view();
    EXPECT_TRUE(desk_name_view->HasFocus());
    EXPECT_EQ(std::u16string(), controller->GetDeskAtIndex(i)->name());
    EXPECT_EQ(DesksController::GetDeskDefaultName(i - 1),
              controller->GetDeskAtIndex(i - 1)->name());
    // As desks are added, we will scroll the desks bar to keep the "new desk"
    // button in view.
    TryScrollOverviewDeskBar(DeskBarScrollDirection::kRight);
    RunScheduledLayoutForAllOverviewDeskBars();
  }
}

// Tests that name nudges works with multiple displays. When a user
// clicks/touches the new desk button, the newly created DeskNameView that
// resides on the same desk bar view as the clicked button should be focused.
// See crbug.com/1206013.
TEST_P(DesksTest, NameNudgesMultiDisplay) {
  UpdateDisplay("800x700,800x700");

  // Start overview.
  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  // Retrieve the desks bar view for each root window.
  auto root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const auto* desks_bar_view_1 =
      GetOverviewGridForRoot(root_windows[0])->desks_bar_view();
  const auto* desks_bar_view_2 =
      GetOverviewGridForRoot(root_windows[1])->desks_bar_view();
  ASSERT_TRUE(desks_bar_view_1->IsZeroState());
  ASSERT_TRUE(desks_bar_view_2->IsZeroState());

  // Click on the zero state default desk button for the second root window.
  auto* default_desk_button_2 = desks_bar_view_2->default_desk_button();
  EXPECT_TRUE(default_desk_button_2->GetEnabled());
  LeftClickOn(default_desk_button_2);
  RunScheduledLayoutForAllOverviewDeskBars();

  // The desk bar should not be in the zero state anymore and the existing
  // desk's name view should be focused, but not cleared.
  EXPECT_FALSE(desks_bar_view_2->IsZeroState());
  EXPECT_EQ(1u, desks_bar_view_2->mini_views().size());
  auto* desk_name_view_2 = desks_bar_view_2->mini_views()[0]->desk_name_view();
  EXPECT_TRUE(desk_name_view_2->HasFocus());
  EXPECT_EQ(DesksController::GetDeskDefaultName(/*desk_index=*/0),
            desk_name_view_2->GetText());

  // Restart overview to reset the zero state.
  ExitOverview();
  EnterOverview();
  desks_bar_view_1 = GetOverviewGridForRoot(root_windows[0])->desks_bar_view();
  desks_bar_view_2 = GetOverviewGridForRoot(root_windows[1])->desks_bar_view();
  ASSERT_TRUE(desks_bar_view_1->IsZeroState());
  ASSERT_TRUE(desks_bar_view_2->IsZeroState());

  // Click on the new desk button on the first root window.
  auto* new_desk_button_1 = desks_bar_view_1->new_desk_button();
  EXPECT_TRUE(new_desk_button_1->GetEnabled());
  LeftClickOn(new_desk_button_1);
  RunScheduledLayoutForAllOverviewDeskBars();

  // There should be 2 desks now and the name view on the first root window
  // should be focused. Each new name view on the 2 root windows should be
  // empty.
  EXPECT_EQ(2u, desks_bar_view_1->mini_views().size());
  auto* desk_name_view_1 = desks_bar_view_1->mini_views()[1]->desk_name_view();
  desk_name_view_2 = desks_bar_view_2->mini_views()[1]->desk_name_view();
  EXPECT_TRUE(desk_name_view_1->HasFocus());
  EXPECT_FALSE(desk_name_view_2->HasFocus());
  EXPECT_EQ(std::u16string(), desk_name_view_1->GetText());
  EXPECT_EQ(std::u16string(), desk_name_view_2->GetText());

  // Tap on the new desk button on the second root window.
  auto* new_desk_button_2 = desks_bar_view_2->new_desk_button();
  EXPECT_TRUE(new_desk_button_2->GetEnabled());
  GestureTapOn(new_desk_button_2);
  RunScheduledLayoutForAllOverviewDeskBars();

  // There should be 3 desks now and the name view on the second root window
  // should be focused. Each new name view on the 2 root windows should be
  // empty.
  EXPECT_EQ(3u, desks_bar_view_1->mini_views().size());
  desk_name_view_1 = desks_bar_view_1->mini_views()[2]->desk_name_view();
  desk_name_view_2 = desks_bar_view_2->mini_views()[2]->desk_name_view();
  EXPECT_FALSE(desk_name_view_1->HasFocus());
  EXPECT_TRUE(desk_name_view_2->HasFocus());
  EXPECT_EQ(std::u16string(), desk_name_view_1->GetText());
  EXPECT_EQ(std::u16string(), desk_name_view_2->GetText());
}

// Tests that when a user has a `DeskNameView` focused and clicks within the
// overview grid, the `DeskNameView` loses focus and the overview grid is not
// closed.
TEST_P(DesksTest, ClickingOverviewGridUnfocusesDeskNameView) {
  // Create a second desk so we don't start in zero state.
  NewDesk();

  // Start overview.
  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Focus on a `DeskNameView`.
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  auto* desk_name_view = desks_bar_view->mini_views()[0]->desk_name_view();
  desk_name_view->RequestFocus();
  ASSERT_TRUE(desk_name_view->HasFocus());

  // Click the center of the overview grid. This should not close overview mode
  // and should remove focus from the focused `desk_name_view`.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      OverviewGridTestApi(overview_grid).bounds().CenterPoint());
  event_generator->ClickLeftButton();
  EXPECT_FALSE(desk_name_view->HasFocus());
  EXPECT_TRUE(overview_controller->InOverviewSession());
}

TEST_P(DesksTest, ScrollableDesks) {
  UpdateDisplay("201x400");
  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  auto* root_window = Shell::GetPrimaryRootWindow();
  const auto* desks_bar_view =
      GetOverviewGridForRoot(root_window)->desks_bar_view();
  ASSERT_TRUE(desks_bar_view->IsZeroState());
  LeftClickOn(desks_bar_view->default_desk_button());
  RunScheduledLayoutForAllOverviewDeskBars();
  EXPECT_EQ(1u, desks_bar_view->mini_views().size());

  auto* new_desk_button = desks_bar_view->new_desk_button();

  // Set the scroll delta large enough to make sure the desks bar can be
  // scrolled to the end each time.
  const int x_scroll_delta = 500;
  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window);
  auto* event_generator = GetEventGenerator();
  for (size_t i = 1; i < desks_util::GetMaxNumberOfDesks(); i++) {
    gfx::Rect new_desk_button_bounds = new_desk_button->GetBoundsInScreen();
    EXPECT_TRUE(display_bounds.Contains(new_desk_button_bounds));
    LeftClickOn(new_desk_button);
    RunScheduledLayoutForAllOverviewDeskBars();
    // Scroll right to make sure the new desk button is always inside the
    // display.
    event_generator->MoveMouseWheel(-x_scroll_delta, 0);
  }

  auto* controller = DesksController::Get();
  EXPECT_EQ(desks_util::GetMaxNumberOfDesks(), controller->desks().size());
  EXPECT_FALSE(controller->CanCreateDesks());

  EXPECT_TRUE(display_bounds.Contains(new_desk_button->GetBoundsInScreen()));
  EXPECT_FALSE(display_bounds.Contains(
      desks_bar_view->mini_views()[0]->GetBoundsInScreen()));
  event_generator->MoveMouseWheel(x_scroll_delta, 0);
  // Tests that scroll to the left will put the first desk inside the display.
  EXPECT_TRUE(display_bounds.Contains(
      desks_bar_view->mini_views()[0]->GetBoundsInScreen()));
  EXPECT_FALSE(display_bounds.Contains(new_desk_button->GetBoundsInScreen()));
}

// Tests the visibility of the scroll buttons and behavior while clicking the
// corresponding scroll button.
TEST_P(DesksTest, ScrollButtonsVisibility) {
  const auto is_scroll_button_visible = [](DeskBarScrollDirection direction) {
    ScrollArrowButton* scroll_arrow = nullptr;
    switch (direction) {
      case DeskBarScrollDirection::kLeft:
        scroll_arrow = DesksTestApi::GetDeskBarLeftScrollButton(
            DeskBarViewBase::Type::kOverview);
        break;
      case DeskBarScrollDirection::kRight:
        scroll_arrow = DesksTestApi::GetDeskBarRightScrollButton(
            DeskBarViewBase::Type::kOverview);
        break;
    }
    return IsLazyInitViewVisible(scroll_arrow);
  };

  UpdateDisplay("600x400");
  for (size_t i = 1; i < desks_util::GetMaxNumberOfDesks(); i++) {
    NewDesk();
  }

  EXPECT_EQ(DesksController::Get()->desks().size(),
            desks_util::GetMaxNumberOfDesks());
  TabletModeControllerTestApi().EnterTabletMode();
  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  auto* desks_bar =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(desks_bar->GetBoundsInScreen().CenterPoint());

  // Set the scroll delta large enough to make sure the desks bar can be
  // scrolled to the end each time.
  const int x_scroll_delta = 1000;
  // Left scroll button should be hidden and right scroll button should be
  // visible while at the start position.
  event_generator->MoveMouseWheel(x_scroll_delta, 0);
  EXPECT_FALSE(is_scroll_button_visible(DeskBarScrollDirection::kLeft));
  EXPECT_TRUE(is_scroll_button_visible(DeskBarScrollDirection::kRight));

  // Click the right scroll button until it reaches to the right most of the
  // scroll view. Then verify the left scroll button is visible.
  TryScrollOverviewDeskBar(DeskBarScrollDirection::kRight);
  EXPECT_TRUE(is_scroll_button_visible(DeskBarScrollDirection::kLeft));

  // Click the left scroll button until it reaches to the right most of the
  // scroll view. In this case, it will scroll back to the start position and
  // left scroll button should be hidden and right scroll button should be
  // visible.
  TryScrollOverviewDeskBar(DeskBarScrollDirection::kLeft);
  EXPECT_TRUE(is_scroll_button_visible(DeskBarScrollDirection::kRight));

  // Left scroll button should be visible and right scroll button should be
  // hidden while at the end position.
  event_generator->MoveMouseTo(desks_bar->GetBoundsInScreen().CenterPoint());
  event_generator->MoveMouseWheel(-x_scroll_delta, 0);
  EXPECT_TRUE(is_scroll_button_visible(DeskBarScrollDirection::kLeft));
  EXPECT_FALSE(is_scroll_button_visible(DeskBarScrollDirection::kRight));
}

TEST_P(DesksTest, GradientsVisibility) {
  // Set a flat display size to make sure there are multiple pages in the desks
  // bar with maximum number of desks.
  UpdateDisplay("800x150");
  const size_t max_desks_size = desks_util::GetMaxNumberOfDesks();
  for (size_t i = 1; i < max_desks_size; i++) {
    NewDesk();
  }

  EnterOverview();
  auto* desks_bar =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();

  auto* left_button = DesksTestApi::GetDeskBarLeftScrollButton(
      DeskBarViewBase::Type::kOverview);
  auto* right_button = DesksTestApi::GetDeskBarRightScrollButton(
      DeskBarViewBase::Type::kOverview);

  // Only right graident is visible while at the first page.
  auto* scroll_view =
      DesksTestApi::GetDeskBarScrollView(DeskBarViewBase::Type::kOverview);
  ASSERT_TRUE(scroll_view);
  EXPECT_EQ(0, scroll_view->GetVisibleRect().x());
  ASSERT_TRUE(left_button);
  EXPECT_FALSE(left_button->GetVisible());
  EXPECT_FALSE(DesksTestApi::IsDeskBarLeftGradientVisible(
      DeskBarViewBase::Type::kOverview));
  ASSERT_TRUE(right_button);
  EXPECT_TRUE(right_button->GetVisible());
  EXPECT_TRUE(DesksTestApi::IsDeskBarRightGradientVisible(
      DeskBarViewBase::Type::kOverview));

  // Both left and right gradients should be visible while during scroll.
  const gfx::Point center_point = desks_bar->bounds().CenterPoint();
  ui::GestureEvent scroll_begin(
      center_point.x(), center_point.y(), ui::EF_NONE, base::TimeTicks::Now(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin, 1, 0));
  scroll_view->OnGestureEvent(&scroll_begin);
  ui::GestureEvent scroll_update(
      center_point.x(), center_point.y(), ui::EF_NONE, base::TimeTicks::Now(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, -100, 0));
  scroll_view->OnGestureEvent(&scroll_update);
  EXPECT_TRUE(scroll_view->is_scrolling());
  EXPECT_TRUE(left_button->GetVisible());
  EXPECT_TRUE(DesksTestApi::IsDeskBarLeftGradientVisible(
      DeskBarViewBase::Type::kOverview));
  EXPECT_TRUE(right_button->GetVisible());
  EXPECT_TRUE(DesksTestApi::IsDeskBarRightGradientVisible(
      DeskBarViewBase::Type::kOverview));

  // The gradient should be hidden if the corresponding scroll button is
  // invisible even though it is during scroll.
  ui::GestureEvent second_scroll_update(
      center_point.x() - 100, center_point.y(), ui::EF_NONE,
      base::TimeTicks::Now(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, 100, 0));
  scroll_view->OnGestureEvent(&second_scroll_update);
  EXPECT_TRUE(scroll_view->is_scrolling());
  EXPECT_FALSE(left_button->GetVisible());
  EXPECT_FALSE(DesksTestApi::IsDeskBarLeftGradientVisible(
      DeskBarViewBase::Type::kOverview));
  EXPECT_TRUE(right_button->GetVisible());
  EXPECT_TRUE(DesksTestApi::IsDeskBarRightGradientVisible(
      DeskBarViewBase::Type::kOverview));

  ui::GestureEvent scroll_end(
      center_point.x(), center_point.y(), ui::EF_NONE, base::TimeTicks::Now(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));
  scroll_view->OnGestureEvent(&scroll_end);
  EXPECT_FALSE(scroll_view->is_scrolling());
  EXPECT_FALSE(left_button->GetVisible());
  EXPECT_FALSE(DesksTestApi::IsDeskBarLeftGradientVisible(
      DeskBarViewBase::Type::kOverview));
  EXPECT_TRUE(right_button->GetVisible());
  EXPECT_TRUE(DesksTestApi::IsDeskBarRightGradientVisible(
      DeskBarViewBase::Type::kOverview));

  // Only right gradient should be shown at the middle page when it is not
  // during scroll even though the left scroll button is visible.
  LeftClickOn(right_button);
  EXPECT_TRUE(left_button->GetVisible());
  EXPECT_FALSE(DesksTestApi::IsDeskBarLeftGradientVisible(
      DeskBarViewBase::Type::kOverview));
  EXPECT_TRUE(right_button->GetVisible());
  EXPECT_TRUE(DesksTestApi::IsDeskBarRightGradientVisible(
      DeskBarViewBase::Type::kOverview));

  // Only the left gradient should be shown at the last page.
  while (right_button->GetVisible()) {
    LeftClickOn(right_button);
  }

  EXPECT_EQ(scroll_view->contents()->bounds().width() - scroll_view->width(),
            scroll_view->GetVisibleRect().x());
  EXPECT_TRUE(left_button->GetVisible());
  EXPECT_TRUE(DesksTestApi::IsDeskBarLeftGradientVisible(
      DeskBarViewBase::Type::kOverview));
  EXPECT_FALSE(right_button->GetVisible());
  EXPECT_FALSE(DesksTestApi::IsDeskBarRightGradientVisible(
      DeskBarViewBase::Type::kOverview));
}

// Tests the behavior when long press on the scroll buttons.
TEST_P(DesksTest, ContinueScrollBar) {
  const int scroll_timeout_ms = 20;
  base::AutoReset<base::TimeDelta> auto_reset =
      DesksTestApi::SetScrollTimeInterval(
          base::Milliseconds(scroll_timeout_ms));

  // Make a flat long window to generate multiple pages on desks bar.
  UpdateDisplay("800x150");
  const size_t max_desks_size = desks_util::GetMaxNumberOfDesks();
  for (size_t i = 1; i < max_desks_size; i++) {
    NewDesk();
  }

  auto* desks_controller = DesksController::Get();
  EXPECT_EQ(desks_controller->desks().size(), max_desks_size);
  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  auto* desks_bar =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();

  views::ScrollView* scroll_view =
      DesksTestApi::GetDeskBarScrollView(DeskBarViewBase::Type::kOverview);
  ASSERT_TRUE(scroll_view);
  const int page_size = scroll_view->width();
  const auto mini_views = desks_bar->mini_views();
  const int mini_view_width = mini_views[0]->bounds().width();
  int desks_in_one_page = page_size / mini_view_width;
  float fractional_page = static_cast<float>(page_size % mini_view_width) /
                          static_cast<float>(mini_view_width);
  if (fractional_page > 0.5) {
    desks_in_one_page++;
  }

  int current_index = 0;
  ScrollArrowButton* left_button = DesksTestApi::GetDeskBarLeftScrollButton(
      DeskBarViewBase::Type::kOverview);
  ASSERT_TRUE(left_button);
  ScrollArrowButton* right_button = DesksTestApi::GetDeskBarRightScrollButton(
      DeskBarViewBase::Type::kOverview);
  ASSERT_TRUE(right_button);

  // At first, left scroll button is hidden and right scroll button is visible.
  EXPECT_FALSE(left_button->GetVisible());
  EXPECT_TRUE(right_button->GetVisible());

  const int focus_ring_width_and_padding = 4;

  // Press on the right scroll button by mouse should scroll to the next page.
  // And the final scroll position should be adjusted to make sure the desk
  // preview will not be cropped.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(right_button->GetBoundsInScreen().CenterPoint());
  event_generator->PressLeftButton();
  current_index += desks_in_one_page;
  EXPECT_EQ(scroll_view->GetVisibleRect().x() + focus_ring_width_and_padding,
            mini_views[current_index]->bounds().x());

  // Both scroll buttons should be visible.
  EXPECT_TRUE(left_button->GetVisible());
  EXPECT_TRUE(right_button->GetVisible());

  // Wait for 20ms, there will be another scroll.
  WaitForMilliseconds(scroll_timeout_ms);
  current_index += desks_in_one_page;

  // When the maximum number of desks is 8, the new desk button is smaller, two
  // scrolls will reach the end of the desks bar view, thus we verify the right
  // of the visible scroll view.
  if (!features::Is16DesksEnabled()) {
    EXPECT_EQ(
        scroll_view->GetVisibleRect().right() - focus_ring_width_and_padding,
        desks_bar->new_desk_button()->bounds().right());
  } else {
    EXPECT_EQ(scroll_view->GetVisibleRect().x() + focus_ring_width_and_padding,
              mini_views[current_index]->bounds().x());
  }

  // Release and click a few times to make sure we end up at the maximum offset.
  event_generator->ReleaseLeftButton();
  for (int i = 0; i != 3; ++i) {
    event_generator->ClickLeftButton();
  }

  EXPECT_EQ(scroll_view->GetVisibleRect().x(),
            scroll_view->contents()->width() - page_size);

  // Left scroll button should be visible and right scroll button should be
  // hidden.
  EXPECT_TRUE(left_button->GetVisible());
  EXPECT_FALSE(right_button->GetVisible());

  // Since we're scrolled all the way to the right and the new desk button is
  // visible, this is the index of the leftmost visible mini view.
  current_index = max_desks_size - 1;

  // Press on left scroll button by gesture should scroll to the previous page.
  // And the final scroll position should also be adjusted while scrolling to
  // previous page to make sure the desk preview will not be cropped.
  event_generator->MoveTouch(left_button->GetBoundsInScreen().CenterPoint());
  event_generator->PressTouch();
  // The new desk button and the library button are smaller, thus when scroll to
  // the left from the right mode.
  current_index -= (desks_in_one_page + 1);
  EXPECT_EQ(scroll_view->GetVisibleRect().x() + focus_ring_width_and_padding,
            mini_views[current_index]->bounds().x());

  // Wait for 20ms, there is another scroll.
  WaitForMilliseconds(scroll_timeout_ms);
  current_index -= desks_in_one_page;
  EXPECT_EQ(scroll_view->GetVisibleRect().x() + focus_ring_width_and_padding,
            mini_views[current_index]->bounds().x());

  event_generator->ReleaseTouch();
}

// Tests that change the focused mini view should scroll the desks bar and put
// the focused mini view inside the visible bounds.
TEST_P(DesksTest, FocusedMiniViewIsVisible) {
  UpdateDisplay("600x400");
  for (size_t i = 1; i < desks_util::GetMaxNumberOfDesks(); i++) {
    NewDesk();
  }

  EXPECT_EQ(DesksController::Get()->desks().size(),
            desks_util::GetMaxNumberOfDesks());
  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  TabletModeControllerTestApi().EnterTabletMode();
  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  auto* desks_bar =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();
  // Check if scroll view is enabled.
  ASSERT_TRUE(DesksTestApi::GetDeskBarRightScrollButton(
      DeskBarViewBase::Type::kOverview));
  auto mini_views = desks_bar->mini_views();
  ASSERT_EQ(mini_views.size(), desks_util::GetMaxNumberOfDesks());

  auto* generator = GetEventGenerator();
  // Traverse all the desks mini views from left to right.
  for (size_t i = 0; i < desks_util::GetMaxNumberOfDesks(); i++) {
    // Move the focus to the mini view's associated preview view.
    PressAndReleaseKey(ui::VKEY_TAB);
    const views::ScrollView* const scroll_view =
        DesksTestApi::GetDeskBarScrollView(DeskBarViewBase::Type::kOverview);
    ASSERT_TRUE(scroll_view);
    EXPECT_TRUE(
        scroll_view->GetVisibleRect().Contains(mini_views[i]->bounds()));
    // Move the focus to the mini view's associated name view.
    ASSERT_TRUE(TabUntil(
        /*reverse=*/false, ViewFocused(mini_views[i]->desk_name_view()),
        generator));
  }

  // Traverse from all the desk mini views from right to left.
  for (size_t i = desks_util::GetMaxNumberOfDesks() - 1; i > 0; i--) {
    // Move the focus to previous mini view's name view.
    ASSERT_TRUE(TabUntil(
        /*reverse=*/true, ViewFocused(mini_views[i - 1]->desk_name_view()),
        generator));
    const views::ScrollView* const scroll_view =
        DesksTestApi::GetDeskBarScrollView(DeskBarViewBase::Type::kOverview);
    ASSERT_TRUE(scroll_view);
    EXPECT_TRUE(
        scroll_view->GetVisibleRect().Contains(mini_views[i - 1]->bounds()));
  }
}

// Tests that active desk mini view is visible when entering overview especially
// with 16 desks.
TEST_P(DesksTest, ActiveDeskMiniViewIsVisible) {
  for (size_t i = 1; i < desks_util::GetMaxNumberOfDesks(); i++) {
    // Create a new desk and go to that desk.
    NewDesk();
    ActivateDesk(DesksController::Get()->desks().back().get());

    // Enter overview and check the active mini view is fully visible.
    EnterOverview();
    auto* desks_bar =
        GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();
    for (ash::DeskMiniView* mini_view : desks_bar->mini_views()) {
      if (mini_view->desk()->is_active()) {
        EXPECT_EQ(mini_view->size(), mini_view->GetVisibleBounds().size());
      }
    }

    ExitOverview();
  }
}

// Tests that change the focused new desk button is fully visible.
TEST_P(DesksTest, FocusedButtonIsVisible) {
  // Create `GetMaxNumberOfDesks() - 1` desks so that the new desk button is
  // still enabled.
  for (size_t i = 1; i < desks_util::GetMaxNumberOfDesks() - 1; i++) {
    NewDesk();
  }

  EnterOverview();

  auto* desk_bar =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();
  auto* new_desk_button = desk_bar->new_desk_button();
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(new_desk_button->GetVisible());
  EXPECT_EQ(new_desk_button->size(),
            new_desk_button->GetVisibleBounds().size());

  ExitOverview();
}

// Tests that the bounds of a window that is visible on all desks is shared
// across desks.
TEST_P(DesksTest, VisibleOnAllDesksGlobalBounds) {
  auto* controller = DesksController::Get();
  NewDesk();
  const Desk* desk_1 = controller->GetDeskAtIndex(0);
  const Desk* desk_2 = controller->GetDeskAtIndex(1);
  auto* root = Shell::GetPrimaryRootWindow();
  const gfx::Rect window_initial_bounds(1, 1, 200, 200);
  const gfx::Rect window_moved_bounds(200, 200, 250, 250);

  auto window = CreateAppWindow(window_initial_bounds);
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());
  ASSERT_EQ(window_initial_bounds, window->bounds());

  // Assign |window| to all desks. It shouldn't change bounds.
  widget->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(desks_util::IsWindowVisibleOnAllWorkspaces(window.get()));
  EXPECT_EQ(window_initial_bounds, window->bounds());
  EXPECT_EQ(1u, controller->visible_on_all_desks_windows().size());

  // Move to desk 2. The only window on the new desk should be |window|
  // and it should have the same bounds.
  ActivateDesk(desk_2);
  auto desk_2_children = desk_2->GetDeskContainerForRoot(root)->children();
  EXPECT_EQ(1u, desk_2_children.size());
  EXPECT_EQ(window.get(), desk_2_children[0]);
  EXPECT_EQ(window_initial_bounds, window->bounds());

  // Change |window|'s bounds and move to desk 1. It should retain its moved
  // bounds.
  window->SetBounds(window_moved_bounds);
  EXPECT_EQ(window_moved_bounds, window->bounds());
  ActivateDesk(desk_1);
  auto desk_1_children = desk_1->GetDeskContainerForRoot(root)->children();
  EXPECT_EQ(1u, desk_1_children.size());
  EXPECT_EQ(window.get(), desk_1_children[0]);
  EXPECT_EQ(window_moved_bounds, window->bounds());
}

// Tests the behavior of windows that are visible on all desks when the active
// desk is removed.
TEST_P(DesksTest, VisibleOnAllDesksActiveDeskRemoval) {
  auto* controller = DesksController::Get();
  NewDesk();
  const Desk* desk_1 = controller->GetDeskAtIndex(0);
  const Desk* desk_2 = controller->GetDeskAtIndex(1);
  auto* root = Shell::GetPrimaryRootWindow();

  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto win1 = CreateAppWindow(gfx::Rect(1, 1, 150, 150));
  auto* widget0 = views::Widget::GetWidgetForNativeWindow(win0.get());
  auto* widget1 = views::Widget::GetWidgetForNativeWindow(win1.get());

  // Assign |win0| and |win1| to all desks.
  widget0->SetVisibleOnAllWorkspaces(true);
  widget1->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(desks_util::IsWindowVisibleOnAllWorkspaces(win0.get()));
  ASSERT_TRUE(desks_util::IsWindowVisibleOnAllWorkspaces(win1.get()));

  // Remove the active desk. The visible on all desks windows should be on
  // |desk_2|.
  RemoveDesk(desk_1);
  auto desk_2_children = desk_2->GetDeskContainerForRoot(root)->children();
  EXPECT_EQ(2u, desk_2_children.size());
  EXPECT_TRUE(window_util::IsStackedBelow(win0.get(), win1.get()));
  EXPECT_EQ(2u, controller->visible_on_all_desks_windows().size());
}

// Tests the behavior of a minimized window that is visible on all desks.
TEST_P(DesksTest, VisibleOnAllDesksMinimizedWindow) {
  auto* controller = DesksController::Get();
  NewDesk();
  const Desk* desk_2 = controller->GetDeskAtIndex(1);
  auto* root = Shell::GetPrimaryRootWindow();
  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());

  // Minimize |window| and then assign it to all desks. This shouldn't
  // unminimize it.
  auto* window_state = WindowState::Get(window.get());
  window_state->Minimize();
  ASSERT_TRUE(window_state->IsMinimized());
  widget->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(desks_util::IsWindowVisibleOnAllWorkspaces(window.get()));
  EXPECT_TRUE(window_state->IsMinimized());

  // Switch desks. |window| should be on the newly active desk and should still
  // be minimized.
  ActivateDesk(desk_2);
  auto desk_2_children = desk_2->GetDeskContainerForRoot(root)->children();
  EXPECT_EQ(1u, desk_2_children.size());
  EXPECT_EQ(window.get(), desk_2_children[0]);
  EXPECT_TRUE(window_state->IsMinimized());
}

// Tests the behavior of a window that is visible on all desks when a user tries
// to move it to another desk using drag and drop (overview mode).
TEST_P(DesksTest, VisibleOnAllDesksMoveWindowToDeskViaDragAndDrop) {
  auto* controller = DesksController::Get();
  auto* root = Shell::GetPrimaryRootWindow();
  NewDesk();
  const Desk* desk_1 = controller->GetDeskAtIndex(0);
  const Desk* desk_2 = controller->GetDeskAtIndex(1);

  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());

  // Assign |window| to all desks.
  widget->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(desks_util::IsWindowVisibleOnAllWorkspaces(window.get()));

  // Try to move |window| to |desk_2| via drag and drop. It should not be moved.
  EXPECT_FALSE(controller->MoveWindowFromActiveDeskTo(
      window.get(), const_cast<Desk*>(desk_2), root,
      DesksMoveWindowFromActiveDeskSource::kDragAndDrop));
  EXPECT_TRUE(desks_util::BelongsToActiveDesk(window.get()));
  EXPECT_EQ(1u, controller->visible_on_all_desks_windows().size());
  EXPECT_TRUE(desks_util::IsWindowVisibleOnAllWorkspaces(window.get()));
  EXPECT_TRUE(base::Contains(desk_1->windows(), window.get()));
}

// Tests the behavior of a window that is visible on all desks when a user tries
// to move it to another desk using keyboard shortcuts.
TEST_P(DesksTest, VisibleOnAllDesksMoveWindowToDeskViaShortcuts) {
  auto* controller = DesksController::Get();
  auto* root = Shell::GetPrimaryRootWindow();
  NewDesk();
  const Desk* desk_2 = controller->GetDeskAtIndex(1);

  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());

  // Assign |window| to all desks.
  widget->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(desks_util::IsWindowVisibleOnAllWorkspaces(window.get()));

  // Move |window| to |desk_2| via keyboard shortcut. It should be on |desk_2|
  // and should no longer be visible on all desks.
  EXPECT_TRUE(controller->MoveWindowFromActiveDeskTo(
      window.get(), const_cast<Desk*>(desk_2), root,
      DesksMoveWindowFromActiveDeskSource::kShortcut));
  EXPECT_FALSE(desks_util::BelongsToActiveDesk(window.get()));
  EXPECT_EQ(0u, controller->visible_on_all_desks_windows().size());
  EXPECT_FALSE(desks_util::IsWindowVisibleOnAllWorkspaces(window.get()));
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));
}

// Tests the behavior of a window that is visible on all desks when a user tries
// to move it using the context menu.
TEST_P(DesksTest, VisibleOnAllDesksMoveWindowToDeskViaContextMenu) {
  auto* controller = DesksController::Get();
  NewDesk();
  const Desk* desk_2 = controller->GetDeskAtIndex(1);

  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());

  // Assign |window| to all desks.
  widget->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(desks_util::IsWindowVisibleOnAllWorkspaces(window.get()));

  // Move |window| to |desk_2| via keyboard shortcut. It should be on |desk_2|
  // and should no longer be visible on all desks.
  controller->SendToDeskAtIndex(window.get(), controller->GetDeskIndex(desk_2));
  EXPECT_FALSE(desks_util::BelongsToActiveDesk(window.get()));
  EXPECT_EQ(0u, controller->visible_on_all_desks_windows().size());
  EXPECT_FALSE(desks_util::IsWindowVisibleOnAllWorkspaces(window.get()));
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));
}

// Tests that when a window that is visible on all desks is destroyed it is
// removed from DesksController.visible_on_all_desks_windows_.
TEST_P(DesksTest, VisibleOnAllDesksWindowDestruction) {
  auto* controller = DesksController::Get();
  NewDesk();
  const Desk* desk_1 = controller->GetDeskAtIndex(0);
  auto* root = Shell::GetPrimaryRootWindow();

  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());

  // Assign |window| to all desks.
  widget->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(desks_util::IsWindowVisibleOnAllWorkspaces(window.get()));
  EXPECT_EQ(1u, controller->visible_on_all_desks_windows().size());
  EXPECT_EQ(1u, desk_1->GetDeskContainerForRoot(root)->children().size());

  // Destroy |window|. It should be removed from
  // DesksController.visible_on_all_desks_windows_.
  window.reset();
  EXPECT_EQ(0u, controller->visible_on_all_desks_windows().size());
  EXPECT_EQ(0u, desk_1->GetDeskContainerForRoot(root)->children().size());
}

// Tests that when a window that isn't on the currently active desk is made
// visible on all desks, it is also moved to the active desk.
TEST_P(DesksTest, VisibleOnAllDesksInactiveDesk) {
  auto* controller = DesksController::Get();
  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());

  NewDesk();
  auto* desk1 = controller->GetDeskAtIndex(0);
  auto* desk2 = controller->GetDeskAtIndex(1);

  ActivateDesk(desk2);
  ASSERT_FALSE(desk1->is_active());
  ASSERT_TRUE(desk2->is_active());

  // The test `window` should exist on the first desk.
  ASSERT_THAT(desk1->windows(), ElementsAre(window.get()));
  ASSERT_THAT(desk2->windows(), ElementsAre());

  // Assign `window` to all desks, this should move it to the active desk.
  widget->SetVisibleOnAllWorkspaces(true);
  EXPECT_THAT(desk1->windows(), ElementsAre());
  EXPECT_THAT(desk2->windows(), ElementsAre(window.get()));
}

// Tests that the desk bar exit animation would not cause any crash or UAF.
// Please refer to b/274497402.
TEST_P(DesksTest, DesksBarExitAnimation) {
  // Create another desk so that the desk bar is not zero state.
  NewDesk();

  // Set to non-zero animation.
  ui::ScopedAnimationDurationScaleMode animation(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Enter then exit overview. When shutting down overview grid, the desk bar
  // slide animation will take over the ownership of the desk bar widget. This
  // is to ensure no crash or UAF after the ownership change.
  EnterOverview();
  ExitOverview();
}

TEST_P(DesksTest, EnterOverviewWithCorrectDesksBarState) {
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  EnterOverview();

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto* desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();

  // Desks bar should stay in zero state if there is only one desk and no mini
  // view has been created.
  EXPECT_TRUE(desks_bar_view->IsZeroState());
  EXPECT_TRUE(desks_bar_view->mini_views().empty());

  // Click new desk button in the zero state bar to create a new desk.
  LeftClickOn(desks_bar_view->new_desk_button());
  ExitOverview();

  // Desks bar should not stay in zero state if there are more than one desks.
  EXPECT_EQ(2u, controller->desks().size());
  EnterOverview();
  desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();
  EXPECT_EQ(2u, desks_bar_view->mini_views().size());
  EXPECT_FALSE(desks_bar_view->IsZeroState());
}

// Tests the behavior of desks bar zero state.
TEST_P(DesksTest, DesksBarZeroState) {
  EnterOverview();

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto* desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();
  ASSERT_TRUE(desks_bar_view->IsZeroState());
  ASSERT_TRUE(desks_bar_view->mini_views().empty());

  auto* default_desk_button = desks_bar_view->default_desk_button();
  auto* new_desk_button = desks_bar_view->new_desk_button();
  LeftClickOn(default_desk_button);
  // Click the zero state default desk button should switch to expanded desks
  // bar and focus on the default desk's name view. The two buttons in zero
  // state bar should also be hidden.
  EXPECT_FALSE(desks_bar_view->IsZeroState());
  EXPECT_EQ(1u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(desks_bar_view->mini_views()[0]->desk_name_view()->HasFocus());
  EXPECT_FALSE(default_desk_button->GetVisible());
  VerifyZeroStateNewDeskButtonVisibility(desks_bar_view, false);

  ExitOverview();
  EnterOverview();
  desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();
  ASSERT_TRUE(desks_bar_view->IsZeroState());

  default_desk_button = desks_bar_view->default_desk_button();
  new_desk_button = desks_bar_view->new_desk_button();
  LeftClickOn(new_desk_button);
  // Click the zero state new desk button should switch to expand desks bar,
  // create a new desk and focus on the new created desk's name view. The two
  // buttons in zero state bar should also be hidden.
  EXPECT_FALSE(desks_bar_view->IsZeroState());
  EXPECT_EQ(2u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(desks_bar_view->mini_views()[1]->desk_name_view()->HasFocus());
  EXPECT_FALSE(default_desk_button->GetVisible());
  VerifyZeroStateNewDeskButtonVisibility(desks_bar_view, false);

  CloseDeskFromMiniView(desks_bar_view->mini_views()[0], GetEventGenerator());
  // Desks bar doesn't switch to zero state even if there is only one desk after
  // removing.
  EXPECT_FALSE(desks_bar_view->IsZeroState());
  EXPECT_FALSE(default_desk_button->GetVisible());
  VerifyZeroStateNewDeskButtonVisibility(desks_bar_view, false);
}

// Tests that buttons in the desk bar are shown and hidden correctly when
// transitioning into zero state to ensure ChromeVox navigation works properly.
// Regression test for https://crbug.com/1351501.
TEST_P(DesksTest, DesksBarButtonVisibility) {
  auto* controller = DesksController::Get();

  // Create a new desk so when we enter overview mode the desks bar is in the
  // expanded state.
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  // Enter overview mode and check that the desks bar is in the expanded state.
  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* desks_bar_view =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_FALSE(desks_bar_view->IsZeroState());

  // Verify that the expanded state button is visible, while the zero state
  // buttons are not visible.
  auto* default_desk_button = desks_bar_view->default_desk_button();
  VerifyExpandedStateNewDeskButtonVisibility(desks_bar_view, true);
  VerifyZeroStateNewDeskButtonVisibility(desks_bar_view, false);
  EXPECT_FALSE(default_desk_button->GetVisible());

  // Close a desk and check that the desks bar switches into zero state.
  auto* mini_view = desks_bar_view->mini_views().front().get();
  CloseDeskFromMiniView(mini_view, GetEventGenerator());
  ASSERT_EQ(1u, controller->desks().size());
  ASSERT_FALSE(desks_bar_view->IsZeroState());
  EXPECT_EQ(1u, desks_bar_view->mini_views().size());

  // The desk bar doesn't go back to zero state.
  VerifyExpandedStateNewDeskButtonVisibility(desks_bar_view, true);
  VerifyZeroStateNewDeskButtonVisibility(desks_bar_view, false);
  EXPECT_FALSE(default_desk_button->GetVisible());
}

TEST_P(DesksTest, NewDeskButton) {
  auto* controller = DesksController::Get();
  EnterOverview();

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto* desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();

  // Click the default desk button in the zero state bar to switch to expanded
  // desks bar.
  LeftClickOn(desks_bar_view->default_desk_button());
  RunScheduledLayoutForAllOverviewDeskBars();
  auto* new_desk_button = desks_bar_view->new_desk_button();
  EXPECT_TRUE(new_desk_button->GetVisible());
  EXPECT_TRUE(new_desk_button->GetEnabled());

  for (size_t i = 1; i < desks_util::GetMaxNumberOfDesks(); i++) {
    LeftClickOn(new_desk_button);
    RunScheduledLayoutForAllOverviewDeskBars();

    // When a new desk is created, ensure its desk mini view fully visible.
    auto* mini_view = desks_bar_view->mini_views().back().get();
    EXPECT_EQ(mini_view->size(), mini_view->GetVisibleBounds().size());

    // TODO(b/277081702): When desk order is adjusted for RTL, remove the check
    // below to always make new desk button visible.
    if (!base::i18n::IsRTL()) {
      EXPECT_EQ(new_desk_button->size(),
                new_desk_button->GetVisibleBounds().size());
    }

    TryScrollOverviewDeskBar(DeskBarScrollDirection::kRight);
    RunScheduledLayoutForAllOverviewDeskBars();
  }

  // The new desk button should become disabled after maximum number of desks
  // have been created.
  EXPECT_FALSE(controller->CanCreateDesks());
  EXPECT_FALSE(new_desk_button->GetEnabled());

  ASSERT_EQ(controller->desks().size(), desks_util::GetMaxNumberOfDesks());

  // The new desk button should become enabled again after removing a desk.
  CloseDeskFromMiniView(desks_bar_view->mini_views().back(),
                        GetEventGenerator());
  RunScheduledLayoutForAllOverviewDeskBars();
  EXPECT_TRUE(controller->CanCreateDesks());
  EXPECT_TRUE(new_desk_button->GetEnabled());
}

TEST_P(DesksTest, ZeroStateDeskButtonText) {
  UpdateDisplay("1600x1200");
  EnterOverview();

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto* desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();
  ASSERT_TRUE(desks_bar_view->IsZeroState());
  // Show the default name "Desk 1" while initializing the desks bar at the
  // first time.
  EXPECT_EQ(u"Desk 1", desks_bar_view->default_desk_button()->GetText());

  LeftClickOn(desks_bar_view->default_desk_button());
  EXPECT_TRUE(desks_bar_view->mini_views()[0]->desk_name_view()->HasFocus());

  // Change the desk name to "test".
  PressAndReleaseKey(ui::VKEY_T);
  PressAndReleaseKey(ui::VKEY_E);
  PressAndReleaseKey(ui::VKEY_S);
  PressAndReleaseKey(ui::VKEY_T);
  PressAndReleaseKey(ui::VKEY_RETURN);
  ExitOverview();
  EnterOverview();

  desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();
  EXPECT_TRUE(desks_bar_view->IsZeroState());
  // Should show the desk's current name "test" instead of the default name.
  EXPECT_EQ(u"test", desks_bar_view->default_desk_button()->GetText());

  // Create 'Desk 2'.
  LeftClickOn(desks_bar_view->new_desk_button());
  EXPECT_FALSE(desks_bar_view->IsZeroState());
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_EQ(u"Desk 2", DesksController::Get()->GetDeskAtIndex(1)->name());

  // Close desk 'test' should return to zero state and the zero state default
  // desk button should show current desk's name, which is 'Desk 1'.
  CloseDeskFromMiniView(desks_bar_view->mini_views()[0], GetEventGenerator());

  // Once we leave zero state, we can't go back without exiting overview.
  ExitOverview();
  EnterOverview();
  desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();
  EXPECT_TRUE(desks_bar_view->IsZeroState());
  EXPECT_EQ(u"Desk 1", desks_bar_view->default_desk_button()->GetText());

  // Set a super long desk name.
  LeftClickOn(desks_bar_view->default_desk_button());
  for (size_t i = 0; i < DeskTextfield::kMaxLength + 5; i++) {
    PressAndReleaseKey(ui::VKEY_A);
  }
  PressAndReleaseKey(ui::VKEY_RETURN);
  ExitOverview();
  EnterOverview();

  desks_bar_view = GetOverviewGridForRoot(root_window)->desks_bar_view();
  auto* default_desk_button = desks_bar_view->default_desk_button();
  std::u16string desk_button_text = default_desk_button->GetText();
  std::u16string expected_desk_name(DeskTextfield::kMaxLength, L'a');
  // Zero state desk button should show the elided name as the DeskNameView.
  EXPECT_EQ(expected_desk_name,
            DesksController::Get()->GetDeskAtIndex(0)->name());
  EXPECT_NE(expected_desk_name, desk_button_text);
  EXPECT_TRUE(base::StartsWith(base::UTF16ToUTF8(desk_button_text), "aaa",
                               base::CompareCase::SENSITIVE));
  EXPECT_FALSE(base::EndsWith(base::UTF16ToUTF8(desk_button_text), "aaa",
                              base::CompareCase::SENSITIVE));
}

TEST_P(DesksTest, ReorderDesksByMouse) {
  auto* desks_controller = DesksController::Get();

  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  auto* root_window = Shell::GetPrimaryRootWindow();
  const auto* desks_bar_view =
      GetOverviewGridForRoot(root_window)->desks_bar_view();

  auto* event_generator = GetEventGenerator();

  // Add two desks (Now we have three desks).
  NewDesk();
  NewDesk();

  // Cache the mini view and corresponding desks.
  std::vector<raw_ptr<DeskMiniView, VectorExperimental>> mini_views =
      desks_bar_view->mini_views();
  DeskMiniView* mini_view_0 = mini_views[0];
  Desk* desk_0 = mini_view_0->desk();
  DeskMiniView* mini_view_1 = mini_views[1];
  Desk* desk_1 = mini_view_1->desk();
  DeskMiniView* mini_view_2 = mini_views[2];
  Desk* desk_2 = mini_view_2->desk();

  // Set desk names. Force update user prefs because `SetName()` does not
  // trigger it but `DeskMiniView::OnViewBlurred`.
  desk_0->SetName(u"0", /*set_by_user=*/true);
  desk_1->SetName(u"1", /*set_by_user=*/true);
  desk_2->SetName(u"2", /*set_by_user=*/true);
  desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();

  auto* prefs = Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  EXPECT_THAT(GetDeskRestoreNames(prefs), ElementsAre("0", "1", "2"));

  // Dragging the desk preview will trigger drag & drop.
  StartDragDeskPreview(mini_view_1, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  event_generator->ReleaseLeftButton();
  RunScheduledLayoutForAllOverviewDeskBars();

  // Reorder the second desk
  StartDragDeskPreview(mini_view_1, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  // Swap the positions of the second desk and the third desk.
  gfx::Point desk_center_2 =
      mini_view_2->GetPreviewBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(desk_center_2);
  RunScheduledLayoutForAllOverviewDeskBars();

  // Now, the desks order should be [0, 2, 1]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_2));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_1));
  EXPECT_THAT(GetDeskRestoreNames(prefs), ElementsAre("0", "2", "1"));

  // Swap the positions of the second desk and the first desk.
  gfx::Point desk_center_0 =
      mini_view_0->GetPreviewBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(desk_center_0);
  RunScheduledLayoutForAllOverviewDeskBars();

  // Now, the desks order should be [1, 0, 2]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_1));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_2));
  EXPECT_THAT(GetDeskRestoreNames(prefs), ElementsAre("1", "0", "2"));

  event_generator->ReleaseLeftButton();
  RunScheduledLayoutForAllOverviewDeskBars();
}

TEST_P(DesksTest, ReorderDesksByGesture) {
  auto* desks_controller = DesksController::Get();

  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  auto* root_window = Shell::GetPrimaryRootWindow();
  const auto* desks_bar_view =
      GetOverviewGridForRoot(root_window)->desks_bar_view();

  auto* event_generator = GetEventGenerator();

  // Add two desks (Now we have three desks).
  NewDesk();
  NewDesk();
  RunScheduledLayoutForAllOverviewDeskBars();

  // Cache the mini view and corresponding desks.
  std::vector<raw_ptr<DeskMiniView, VectorExperimental>> mini_views =
      desks_bar_view->mini_views();
  DeskMiniView* mini_view_0 = mini_views[0];
  Desk* desk_0 = mini_view_0->desk();
  DeskMiniView* mini_view_1 = mini_views[1];
  Desk* desk_1 = mini_view_1->desk();
  DeskMiniView* mini_view_2 = mini_views[2];
  Desk* desk_2 = mini_view_2->desk();

  // Set desk names and save to user prefs.
  desk_0->SetName(u"0", /*set_by_user=*/true);
  desk_1->SetName(u"1", /*set_by_user=*/true);
  desk_2->SetName(u"2", /*set_by_user=*/true);
  desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();

  auto* prefs = Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  EXPECT_THAT(GetDeskRestoreNames(prefs), ElementsAre("0", "1", "2"));

  // If long press on the second desk preview, drag & drop will be triggered.
  // Perform by gesture:
  LongTapOnDeskPreview(mini_view_1, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  event_generator->ReleaseTouch();

  // The context menu appears on a long press on the desk preview. In order to
  // drag the desk again, we first need to get rid of the context menu by
  // tapping and waiting for the menu to disappear.
  event_generator->GestureTapDownAndUp(
      mini_view_1->desk_preview()->GetBoundsInScreen().CenterPoint());
  base::RunLoop().RunUntilIdle();

  // Reorder the second desk
  LongTapOnDeskPreview(mini_view_1, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  // Swap the positions of the second desk and the third desk.
  gfx::Point desk_center_2 =
      mini_view_2->GetPreviewBoundsInScreen().CenterPoint();

  // We need to drag the mouse a bit after long-tapping the desk preview to
  // start the closing of the context menu before rearranging the desk.
  event_generator->MoveTouchBy(10, 0);

  event_generator->MoveTouch(desk_center_2);
  RunScheduledLayoutForAllOverviewDeskBars();

  // Now, the desks order should be [0, 2, 1]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_2));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_1));
  EXPECT_THAT(GetDeskRestoreNames(prefs), ElementsAre("0", "2", "1"));

  // Swap the positions of the second desk and the first desk.
  gfx::Point desk_center_0 =
      mini_view_0->GetPreviewBoundsInScreen().CenterPoint();
  event_generator->MoveTouch(desk_center_0);

  // Now, the desks order should be [1, 0, 2]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_1));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_2));
  EXPECT_THAT(GetDeskRestoreNames(prefs), ElementsAre("1", "0", "2"));

  event_generator->ReleaseTouch();
}

TEST_P(DesksTest, ReorderDesksByKeyboard) {
  auto* desks_controller = DesksController::Get();

  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  auto* root_window = Shell::GetPrimaryRootWindow();
  auto* overview_grid = GetOverviewGridForRoot(root_window);
  const auto* desks_bar_view = overview_grid->desks_bar_view();

  auto* event_generator = GetEventGenerator();

  // Add two desks (Now we have three desks).
  NewDesk();
  NewDesk();

  overview_grid->CommitNameChanges();

  // Cache the mini view and corresponding desks.
  std::vector<raw_ptr<DeskMiniView, VectorExperimental>> mini_views =
      desks_bar_view->mini_views();
  DeskMiniView* mini_view_0 = mini_views[0];
  Desk* desk_0 = mini_view_0->desk();
  DeskMiniView* mini_view_1 = mini_views[1];
  Desk* desk_1 = mini_view_1->desk();
  DeskMiniView* mini_view_2 = mini_views[2];
  Desk* desk_2 = mini_view_2->desk();

  // Set desk names and save to user prefs.
  desk_0->SetName(u"0", /*set_by_user=*/true);
  desk_1->SetName(u"1", /*set_by_user=*/true);
  desk_2->SetName(u"2", /*set_by_user=*/true);
  desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();

  auto* prefs = Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  EXPECT_THAT(GetDeskRestoreNames(prefs), ElementsAre("0", "1", "2"));

  // Focus the second desk.
  mini_view_1->desk_preview()->RequestFocus();

  // Swap the positions of the second desk and the third desk by pressing Ctrl +
  // ->.
  event_generator->PressKey(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);

  // Now, the desks order should be [0, 2, 1]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_2));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_1));
  EXPECT_THAT(GetDeskRestoreNames(prefs), ElementsAre("0", "2", "1"));

  // Keep pressing -> won't swap desks.
  event_generator->PressKey(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);

  // Now, the desks order should be [0, 2, 1]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_2));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_1));
  EXPECT_THAT(GetDeskRestoreNames(prefs), ElementsAre("0", "2", "1"));

  // Press Ctrl + <- twice will swap the positions of the second and first desk.
  event_generator->PressKey(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);
  event_generator->PressKey(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);

  // Now, the desks order should be [1, 0, 2]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_1));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_2));
  EXPECT_THAT(GetDeskRestoreNames(prefs), ElementsAre("1", "0", "2"));

  // Keep pressing <- won't swap desks.
  event_generator->PressKey(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);

  // Now, the desks order should be [1, 0, 2]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_1));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_2));
  EXPECT_THAT(GetDeskRestoreNames(prefs), ElementsAre("1", "0", "2"));
}

// Test reordering desks in RTL mode.
TEST_P(DesksTest, ReorderDesksInRTLMode) {
  // Turn on RTL mode.
  const bool default_rtl = base::i18n::IsRTL();
  base::i18n::SetRTLForTesting(true);
  EXPECT_TRUE(base::i18n::IsRTL());

  auto* desks_controller = DesksController::Get();

  auto* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  auto* root_window = Shell::GetPrimaryRootWindow();
  const auto* desks_bar_view =
      GetOverviewGridForRoot(root_window)->desks_bar_view();

  auto* event_generator = GetEventGenerator();

  // Add two desks (Now we have three desks).
  NewDesk();
  NewDesk();

  // Cache the mini view and corresponding desks.
  std::vector<raw_ptr<DeskMiniView, VectorExperimental>> mini_views =
      desks_bar_view->mini_views();
  DeskMiniView* mini_view_0 = mini_views[0];
  Desk* desk_0 = mini_view_0->desk();
  DeskMiniView* mini_view_1 = mini_views[1];
  Desk* desk_1 = mini_view_1->desk();
  DeskMiniView* mini_view_2 = mini_views[2];
  Desk* desk_2 = mini_view_2->desk();

  // Set desk names. Force update user prefs because `SetName()` does not
  // trigger it but `DeskMiniView::OnViewBlurred`.
  desk_0->SetName(u"0", /*set_by_user=*/true);
  desk_1->SetName(u"1", /*set_by_user=*/true);
  desk_2->SetName(u"2", /*set_by_user=*/true);
  desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();

  auto* prefs = Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  EXPECT_THAT(GetDeskRestoreNames(prefs), ElementsAre("0", "1", "2"));

  // Swap the positions of the |desk_1| and the |desk_2| by mouse.
  StartDragDeskPreview(mini_view_1, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  gfx::Point desk_center_2 =
      mini_view_2->GetPreviewBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(desk_center_2);

  // Now, the desks order should be [0, 2, 1]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_2));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_1));
  EXPECT_THAT(GetDeskRestoreNames(prefs), ElementsAre("0", "2", "1"));
  event_generator->ReleaseLeftButton();

  // Swap the positions of the |desk_1| and the |desk_0| by gesture.
  LongTapOnDeskPreview(mini_view_1, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  gfx::Point desk_center_0 =
      mini_view_0->GetPreviewBoundsInScreen().CenterPoint();

  // We need to drag the mouse a bit after long-tapping the desk preview to
  // start the closing of the context menu before rearranging the desk.
  event_generator->MoveTouchBy(-10, 0);

  event_generator->MoveTouch(desk_center_0);

  // Now, the desks order should be [1, 0, 2]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_1));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_0));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_2));
  EXPECT_THAT(GetDeskRestoreNames(prefs), ElementsAre("1", "0", "2"));
  event_generator->ReleaseTouch();

  // Swap the positions of `desk_0` and `desk_2` by keyboard. Focus `desk_0`.
  // Focus the second desk.
  mini_view_0->desk_preview()->RequestFocus();

  // Swap the positions of the |desk_0| and the |desk_2| by pressing Ctrl + ->.
  event_generator->PressKey(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);

  // Now, the desks order should be [1, 2, 0]:
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_1));
  EXPECT_EQ(1, desks_controller->GetDeskIndex(desk_2));
  EXPECT_EQ(2, desks_controller->GetDeskIndex(desk_0));
  EXPECT_THAT(GetDeskRestoreNames(prefs), ElementsAre("1", "2", "0"));

  // Recover to default RTL mode.
  base::i18n::SetRTLForTesting(default_rtl);
}

// Tests the behavior when dragging a desk on the scroll button.
TEST_P(DesksTest, ScrollBarByDraggedDesk) {
  const int scroll_timeout_ms = 20;
  base::AutoReset<base::TimeDelta> auto_reset =
      DesksTestApi::SetScrollTimeInterval(
          base::Milliseconds(scroll_timeout_ms));

  // Make a flat long window to generate multiple pages on desks bar.
  UpdateDisplay("800x150");
  const size_t max_desks_size = desks_util::GetMaxNumberOfDesks();
  for (size_t i = 1; i < max_desks_size; i++) {
    NewDesk();
  }

  auto* desks_controller = DesksController::Get();
  EXPECT_EQ(desks_controller->desks().size(), max_desks_size);
  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  auto* desks_bar =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();

  views::ScrollView* scroll_view =
      DesksTestApi::GetDeskBarScrollView(DeskBarViewBase::Type::kOverview);
  ASSERT_TRUE(scroll_view);
  const int page_size = scroll_view->width();
  auto mini_views = desks_bar->mini_views();
  const int mini_view_width = mini_views[0]->bounds().width();
  int desks_in_one_page = page_size / mini_view_width;
  float fractional_page = static_cast<float>(page_size % mini_view_width) /
                          static_cast<float>(mini_view_width);
  if (fractional_page > 0.5) {
    desks_in_one_page++;
  }

  int current_index = 0;
  ScrollArrowButton* left_button = DesksTestApi::GetDeskBarLeftScrollButton(
      DeskBarViewBase::Type::kOverview);
  ASSERT_TRUE(left_button);
  ScrollArrowButton* right_button = DesksTestApi::GetDeskBarRightScrollButton(
      DeskBarViewBase::Type::kOverview);
  ASSERT_TRUE(right_button);

  // At first, left scroll button is hidden and right scroll button is visible.
  EXPECT_FALSE(left_button->GetVisible());
  EXPECT_TRUE(right_button->GetVisible());

  // Dragging a desk on the right scroll button should scroll to the next page.
  // And the final scroll position should be adjusted to make sure the desk
  // preview will not be cropped.
  auto* event_generator = GetEventGenerator();
  DeskMiniView* mini_view_0 = mini_views[0];
  Desk* desk_0 = mini_view_0->desk();

  const int focus_ring_width_and_padding = 4;

  StartDragDeskPreview(mini_view_0, event_generator);
  EXPECT_TRUE(desks_bar->IsDraggingDesk());
  event_generator->MoveMouseTo(right_button->GetBoundsInScreen().CenterPoint());
  RunScheduledLayoutForAllOverviewDeskBars();
  current_index += desks_in_one_page;
  EXPECT_EQ(scroll_view->GetVisibleRect().x() + focus_ring_width_and_padding,
            mini_views[current_index]->bounds().x());

  // Both scroll buttons should be visible.
  EXPECT_TRUE(left_button->GetVisible());
  EXPECT_TRUE(right_button->GetVisible());

  // Wait for 20ms, there will be another scroll.
  WaitForMilliseconds(scroll_timeout_ms);
  current_index += desks_in_one_page;

  // When the maximum number of desks is 8, the new desk button is smaller, two
  // scrolls will reach the end of the desks bar view, thus we verify the right
  // of the visible scroll view.
  if (!features::Is16DesksEnabled()) {
    EXPECT_EQ(
        scroll_view->GetVisibleRect().right() - focus_ring_width_and_padding,
        desks_bar->new_desk_button()->bounds().right());
  } else {
    EXPECT_EQ(scroll_view->GetVisibleRect().x() + focus_ring_width_and_padding,
              mini_views[current_index]->bounds().x());
  }

  // While scrolling, the desk cannot be reordered.
  EXPECT_EQ(0, desks_controller->GetDeskIndex(desk_0));

  // Wait longer, it will scroll to the maximum offset. When 16 desks are
  // enabled, we need to allow more time for scrolling.
  WaitForMilliseconds(GetParam().use_16_desks ? 10 * scroll_timeout_ms
                                              : scroll_timeout_ms);
  EXPECT_EQ(scroll_view->GetVisibleRect().x(),
            scroll_view->contents()->width() - page_size);

  // Left scroll button should be visible and right scroll button should be
  // hidden.
  EXPECT_TRUE(left_button->GetVisible());
  EXPECT_FALSE(right_button->GetVisible());

  // Move the dragged desk to the center of the last desk.
  event_generator->MoveMouseTo(
      mini_views[max_desks_size - 1]->GetBoundsInScreen().CenterPoint());
  RunScheduledLayoutForAllOverviewDeskBars();
  // The dragged desk is reordered to the end.
  const int max_index = static_cast<int>(max_desks_size) - 1;
  // Now the desk is reordered to the last position.
  EXPECT_EQ(max_index, desks_controller->GetDeskIndex(desk_0));
  event_generator->ReleaseLeftButton();
  RunScheduledLayoutForAllOverviewDeskBars();

  current_index = max_desks_size - 1;

  // Dragging the desk to left scroll button should scroll to the previous page.
  // And the final scroll position should also be adjusted while scrolling to
  // the previous page to make sure the desk preview will not be cropped.
  mini_views = desks_bar->mini_views();
  StartDragDeskPreview(mini_views[max_desks_size - 1], event_generator);
  event_generator->MoveMouseTo(left_button->GetBoundsInScreen().CenterPoint());
  RunScheduledLayoutForAllOverviewDeskBars();
  // The new desk button and the library button are smaller, thus when scroll to
  // the left from the right most.
  current_index -= (desks_in_one_page + 1);
  EXPECT_EQ(scroll_view->GetVisibleRect().x() + focus_ring_width_and_padding,
            mini_views[current_index]->bounds().x());

  // Wait for 20ms, there is another scroll.
  WaitForMilliseconds(scroll_timeout_ms);
  current_index -= desks_in_one_page;
  EXPECT_EQ(scroll_view->GetVisibleRect().x() + focus_ring_width_and_padding,
            mini_views[current_index]->bounds().x());

  // The desk is still not reordered while scrolling backward.
  EXPECT_EQ(max_index, desks_controller->GetDeskIndex(desk_0));

  // Drop the desk. Desks bar will scroll to show the desk's target position.
  event_generator->ReleaseLeftButton();
  RunScheduledLayoutForAllOverviewDeskBars();
  // Wait 100 milliseconds for the animation of the scrollable bar to end.
  // Otherwise, the test could be flaky, i.e, the visible bounds of the scroll
  // bar is not updated yet if we get its bounds immediately after the desk is
  // dropped.
  WaitForMilliseconds(100);
  gfx::Rect bounds_0 = mini_view_0->bounds();
  gfx::Rect bounds_visible = scroll_view->GetVisibleRect();
  EXPECT_LE(bounds_visible.x(), bounds_0.x());
  EXPECT_GE(bounds_visible.right(), bounds_0.right());
}

// Tests that while reordering desks by drag & drop, when a desk is snapping
// back, click its target location won't cause any crashes.
// Regression test of https://crbug.com/1171880.
TEST_P(DesksTest, ClickTargetLocationOfDroppedDesk) {
  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  DeskBarViewBase* desk_bar_view = overview_grid->desks_bar_view();

  // Add a desk.
  NewDesk();

  // Cache the second desk's mini view.
  DeskMiniView* mini_view = desk_bar_view->mini_views()[1];

  auto* event_generator = GetEventGenerator();

  // Drag the second desk away from the desk bar.
  StartDragDeskPreview(mini_view, event_generator);
  EXPECT_TRUE(desk_bar_view->IsDraggingDesk());

  event_generator->MoveMouseBy(0, desk_bar_view->height());

  ui::ScopedAnimationDurationScaleMode normal_anim(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  // Drop the desk and click on the mini view.
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(desk_bar_view->IsDraggingDesk());
  DeskSwitchAnimationWaiter waiter;
  LeftClickOn(mini_view);
  DesksTestApi::WaitForDeskBarUiUpdate(desk_bar_view);
  waiter.Wait();
}

// Tests that while reordering desks by drag & drop, when a desk is snapping
// back, dragging a desk preview on the shelf will start a new drag.
TEST_P(DesksTest, DragNewDeskWhileSnappingBack) {
  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();

  // Add a desk.
  NewDesk();

  // Cache the desks' mini views.
  DeskMiniView* mini_view_1 = desks_bar_view->mini_views()[0];
  DeskMiniView* mini_view_2 = desks_bar_view->mini_views()[1];

  auto* event_generator = GetEventGenerator();

  // Drag the second desk away from the desk bar.
  StartDragDeskPreview(mini_view_2, event_generator);
  EXPECT_EQ(DesksTestApi::GetDeskBarDragView(DeskBarViewBase::Type::kOverview),
            mini_view_2);

  event_generator->MoveMouseBy(0, desks_bar_view->height());

  ui::ScopedAnimationDurationScaleMode normal_anim(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Drop the desk and drag first desk.
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(DesksTestApi::GetDeskBarDragView(DeskBarViewBase::Type::kOverview),
            mini_view_2);
  StartDragDeskPreview(mini_view_1, event_generator);
  EXPECT_EQ(DesksTestApi::GetDeskBarDragView(DeskBarViewBase::Type::kOverview),
            mini_view_1);
}

// Tests that dragging desk is ended in two cases: (1) removing a dragged desk.
// (2) removing a desk makes the dragged desk the only one left. Then, releasing
// mouse or exiting overview will not have UAF issues
// (https://crbug.com/1222120).
TEST_P(DesksTest, RemoveDeskWhileDragging) {
  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  const auto* desks_bar_view =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();

  auto* event_generator = GetEventGenerator();

  // Add two desks (Now we have three desks).
  NewDesk();
  NewDesk();

  // Cache the mini views.
  const std::vector<raw_ptr<DeskMiniView, VectorExperimental>>& mini_views =
      desks_bar_view->mini_views();
  DeskMiniView* mini_view_0 = mini_views[0];
  DeskMiniView* mini_view_1 = mini_views[1];
  DeskMiniView* mini_view_2 = mini_views[2];

  // Dragging the first desk preview will trigger drag & drop.
  StartDragDeskPreview(mini_view_0, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  // Removing the first desk will end dragging.
  RemoveDesk(mini_view_0->desk());
  EXPECT_FALSE(desks_bar_view->IsDraggingDesk());

  // Releasing mouse will not have any issue.
  event_generator->ReleaseLeftButton();

  // There are only two desks left.
  EXPECT_EQ(2u, mini_views.size());

  // Dragging the second desk preview.
  StartDragDeskPreview(mini_view_1, event_generator);
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  RemoveDesk(mini_view_2->desk());
  // Removing the third desk won't end dragging, since the desk bar doesn't
  // switch to zero state.
  EXPECT_TRUE(desks_bar_view->IsDraggingDesk());

  // Exiting overview will not have any issue.
  ExitOverview();
}

// Used to wait for all animations to complete in a `LayerAnimator`.
class LayerAnimationWaiter : public ui::LayerAnimationObserver {
 public:
  explicit LayerAnimationWaiter(ui::LayerAnimator* animator)
      : animator_(animator) {
    animator_->AddObserver(this);
  }

  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    OnAnimationCompleted();
  }

  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {
    OnAnimationCompleted();
  }

  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  void Wait() { run_loop_.Run(); }

 private:
  void OnAnimationCompleted() {
    if (animator_->is_animating() == false) {
      animator_->RemoveObserver(this);
      run_loop_.Quit();
    }
  }

  raw_ptr<ui::LayerAnimator> animator_;
  base::RunLoop run_loop_;
};

// Regression test for the asan failure at https://crbug.com/1274641.
TEST_P(DesksTest, DragMiniViewWhileRemoving) {
  NewDesk();
  NewDesk();

  EnterOverview();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  const auto* desks_bar_view =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();

  auto* event_generator = GetEventGenerator();

  // Cache the center point of the desk preview that is about to be removed.
  auto* mini_view = desks_bar_view->mini_views().back().get();
  const gfx::Point desk_preview_center =
      mini_view->GetPreviewBoundsInScreen().CenterPoint();

  LayerAnimationWaiter animation_waiter(mini_view->layer()->GetAnimator());
  {
    // This test requires animation to repro the asan failure.
    ui::ScopedAnimationDurationScaleMode animation_scale(
        ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

    // This will trigger the mini view removal animation, and the miniview won't
    // be removed immediately.
    CloseDeskFromMiniView(mini_view, event_generator);

    // Drag the mini view that is being animated to be removed, and expect drag
    // not to start, nor trigger a crash or an asan failure.
    event_generator->set_current_screen_location(desk_preview_center);
    event_generator->PressLeftButton();
    event_generator->MoveMouseBy(0, 50);
    EXPECT_FALSE(desks_bar_view->IsDraggingDesk());
  }

  animation_waiter.Wait();
}

// Tests that the right desk containers are visible when switching between desks
// really fast. Regression test for https://crbug.com/1194757.
TEST_P(DesksTest, FastDeskSwitches) {
  // Add 3 more desks and add a couple windows on each one.
  CreateTestWindow();
  CreateTestWindow();

  for (int i = 0; i < 3; ++i) {
    NewDesk();
    CreateTestWindow();
    CreateTestWindow();
  }

  // Start at the rightmost, 4th desk.
  auto* desks_controller = DesksController::Get();
  ASSERT_EQ(4u, desks_controller->desks().size());
  desks_controller->ActivateDesk(desks_controller->GetDeskAtIndex(3),
                                 DesksSwitchSource::kUserSwitch);

  ui::ScopedAnimationDurationScaleMode normal_anim(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Activate the first 2 desks very quickly, without waiting for screenshots to
  // be taken.
  desks_controller->ActivateAdjacentDesk(
      /*going_left=*/true, DesksSwitchSource::kDeskSwitchShortcut);
  desks_controller->ActivateAdjacentDesk(
      /*going_left=*/true, DesksSwitchSource::kDeskSwitchShortcut);

  // Let the last desk animation to complete.
  desks_controller->ActivateAdjacentDesk(
      /*going_left=*/true, DesksSwitchSource::kDeskSwitchShortcut);
  // Note that `DeskController::ActivateAdjacentDesk()` will trigger two
  // `OnDeskSwitchAnimationFinished()` calls. The first is from the previous
  // animation which we destroy since its screenshots have not been taken yet.
  // The second is the animation to the first desk that we want to wait to take
  // the screenshots and animate.
  DeskSwitchAnimationWaiter waiter;
  waiter.Wait();

  // Check the desk containers. Test that only the first desk container is
  // visible, but they should all be opaque.
  std::vector<aura::Window*> desk_containers =
      desks_util::GetDesksContainers(Shell::GetPrimaryRootWindow());
  ASSERT_EQ(desks_util::GetMaxNumberOfDesks(), desk_containers.size());
  EXPECT_TRUE(desk_containers[0]->IsVisible());
  EXPECT_EQ(1.f, desk_containers[0]->layer()->opacity());

  for (size_t i = 1; i < desk_containers.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Desk #%lu", i + 1));
    EXPECT_FALSE(desk_containers[i]->IsVisible());
    EXPECT_EQ(1.f, desk_containers[i]->layer()->opacity());
  }
}

// Tests that when the user is in tablet mode, the virtual keyboard is opened
// during name nudges.
TEST_P(DesksTest, NameNudgesTabletMode) {
  TabletModeControllerTestApi().EnterTabletMode();

  // Tablet mode requires at least two desks so create one.
  NewDesk();
  ASSERT_EQ(2u, DesksController::Get()->desks().size());

  // Start overview.
  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Setup an internal keyboard and an external keyboard.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      std::vector<ui::KeyboardDevice>{
          {1, ui::INPUT_DEVICE_INTERNAL, "internal keyboard"},
          {2, ui::INPUT_DEVICE_USB, "external keyboard"}});
  auto* device_data_manager = ui::DeviceDataManager::GetInstance();
  auto keyboard_devices = device_data_manager->GetKeyboardDevices();
  ASSERT_EQ(2u, keyboard_devices.size());

  // Tap the new desk button. There should be a new DeskNameView that is created
  // and the virtual keyboard should not be shown because there is at least one
  // external keyboard attached.
  const auto* desks_bar_view =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();
  GestureTapOn(desks_bar_view->new_desk_button());
  RunScheduledLayoutForAllOverviewDeskBars();
  EXPECT_FALSE(keyboard::KeyboardUIController::Get()->IsKeyboardVisible());
  EXPECT_EQ(3u, desks_bar_view->mini_views().size());
  auto* desk_name_view = desks_bar_view->mini_views()[2]->desk_name_view();
  EXPECT_TRUE(desk_name_view->HasFocus());
  EXPECT_EQ(std::u16string(), desk_name_view->GetText());

  // Reset the devices and make it so there's only an internal keyboard.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      std::vector<ui::KeyboardDevice>{
          {1, ui::INPUT_DEVICE_INTERNAL, "internal keyboard"},
      });
  keyboard_devices = device_data_manager->GetKeyboardDevices();
  ASSERT_EQ(1u, keyboard_devices.size());

  // Tap the new desk button again. There should be a new DeskNameView that is
  // created and the virtual keyboard should be shown.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
  GestureTapOn(desks_bar_view->new_desk_button());
  RunScheduledLayoutForAllOverviewDeskBars();
  ASSERT_TRUE(keyboard::test::WaitUntilShown());
  EXPECT_TRUE(keyboard::KeyboardUIController::Get()->IsKeyboardVisible());
  EXPECT_EQ(4u, desks_bar_view->mini_views().size());
  desk_name_view = desks_bar_view->mini_views()[3]->desk_name_view();
  EXPECT_TRUE(desk_name_view->HasFocus());
  EXPECT_EQ(std::u16string(), desk_name_view->GetText());
}

// Tests that metrics are being recorded when a desk is renamed, when new desks
// are added, and when a desk is being removed.
TEST_P(DesksTest, TestCustomDeskNameMetricsRecording) {
  // Actions that should cause the histogram to update.
  enum class UpdateSource {
    kDeskRenamed,
    kDeskAdded,
    kDeskRemoved,
  };

  struct {
    const std::string scope_trace;
    const UpdateSource update_source;
    const int expected_number_of_custom_desks;
    const int expected_percentage_of_custom_desks;
    const int expected_custom_name_change_true_hits;
    const int expected_custom_name_change_false_hits;
  } kTestCases[] = {
      {"Rename a desk", UpdateSource::kDeskRenamed, 1, 50, 1, 1},
      {"Add a desk", UpdateSource::kDeskAdded, 1, 33, 1, 2},
      {"Remove a desk", UpdateSource::kDeskRemoved, 1, 50, 1, 2},
  };

  base::HistogramTester histogram_tester;

  // Create a new desk. At this point we should have had one update for the new
  // desk addition because adding a new desk requires confirming the new desk's
  // name.
  NewDesk();
  int number_of_updates = 1;
  histogram_tester.ExpectTotalCount(kNumberOfCustomNamesHistogramName,
                                    number_of_updates);
  histogram_tester.ExpectTotalCount(kPercentageOfCustomNamesHistogramName,
                                    number_of_updates);
  histogram_tester.ExpectTotalCount(kCustomNameCreatedHistogramName,
                                    number_of_updates);
  const auto& desks = DesksController::Get()->desks();

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.scope_trace);

    switch (test_case.update_source) {
      case UpdateSource::kDeskRenamed:
        desks[0]->SetName(u"Hello", /*set_by_user=*/true);
        break;
      case UpdateSource::kDeskAdded:
        NewDesk();
        break;
      case UpdateSource::kDeskRemoved:
        RemoveDesk(desks.back().get());
        break;
    }

    ++number_of_updates;
    histogram_tester.ExpectTotalCount(kNumberOfCustomNamesHistogramName,
                                      number_of_updates);
    histogram_tester.ExpectTotalCount(kPercentageOfCustomNamesHistogramName,
                                      number_of_updates);

    // There should be at least one recording of the expected custom desk count
    // and percentage.
    EXPECT_NE(0, histogram_tester.GetBucketCount(
                     kNumberOfCustomNamesHistogramName,
                     test_case.expected_number_of_custom_desks));
    EXPECT_NE(0, histogram_tester.GetBucketCount(
                     kPercentageOfCustomNamesHistogramName,
                     test_case.expected_percentage_of_custom_desks));
    EXPECT_EQ(
        test_case.expected_custom_name_change_true_hits,
        histogram_tester.GetBucketCount(kCustomNameCreatedHistogramName, true));
    EXPECT_EQ(test_case.expected_custom_name_change_false_hits,
              histogram_tester.GetBucketCount(kCustomNameCreatedHistogramName,
                                              false));
  }
}

// Tests that no crash when entering overview  and clicking on the new desk
// button in guest mode. This is a regression test for b/287376870 and
// b/291718574.
TEST_P(DesksTest, EnterOverviewAndAddDeskInGuestMode) {
  // Simulate a guest user logging in, where the session is ephemeral.
  SimulateGuestLogin();

  // Verify no crash while entering overview mode.
  EnterOverview();

  auto* overview_controller = OverviewController::Get();
  ASSERT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();

  auto* new_desk_button = desks_bar_view->new_desk_button();
  // Verify no crash while clicking on the new desk button.
  LeftClickOn(new_desk_button);

  // Verify that desk bar is transformed to the expanded state from zero state.
  ASSERT_FALSE(desks_bar_view->IsZeroState());
  new_desk_button = desks_bar_view->new_desk_button();
  // Click on the expanded new desk button to add one more desk, verify no
  // crash.
  LeftClickOn(new_desk_button);

  // Verify that there're 3 desks in total now.
  EXPECT_EQ(3u, DesksController::Get()->desks().size());
}

// A test class that uses a mock time test environment.
class DesksMockTimeTest : public DesksTest {
 public:
  DesksMockTimeTest()
      : DesksTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  DesksMockTimeTest(const DesksMockTimeTest&) = delete;
  DesksMockTimeTest& operator=(const DesksMockTimeTest&) = delete;
  ~DesksMockTimeTest() override = default;

  // DesksTest:
  void SetUp() override {
    // The `g_weekly_active_desks` global counter may be non-zero at the
    // beginning of tests so reset it. See https://crbug.com/1200684.
    Desk::SetWeeklyActiveDesks(0);
    DesksTest::SetUp();
  }
};

// Tests that the weekly active desks metric is properly recorded.
TEST_P(DesksMockTimeTest, WeeklyActiveDesks) {
  constexpr char kWeeklyActiveDesksHistogram[] = "Ash.Desks.WeeklyActiveDesks";
  base::HistogramTester histogram_tester;

  // Create three new desks.
  NewDesk();
  NewDesk();
  NewDesk();
  auto* controller = DesksController::Get();
  ASSERT_EQ(4u, controller->desks().size());
  const Desk* desk_1 = controller->GetDeskAtIndex(0);
  const Desk* desk_2 = controller->GetDeskAtIndex(1);
  Desk* desk_3 = controller->GetDeskAtIndex(2);
  Desk* desk_4 = controller->GetDeskAtIndex(3);

  // Let a week elapse. There should be a new entry for four since there were
  // three created desks and the initial active desk.
  task_environment()->AdvanceClock(base::Days(7));
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(kWeeklyActiveDesksHistogram, 4, 1);
  EXPECT_EQ(1u,
            histogram_tester.GetAllSamples(kWeeklyActiveDesksHistogram).size());

  // Activate the second desk and quickly activate the first desk. Let a week
  // elapse. There should be a new entry for one since we shouldn't count a desk
  // that was briefly visited.
  EXPECT_EQ(1, Desk::GetWeeklyActiveDesks());
  ActivateDesk(desk_2);
  ActivateDesk(desk_1);
  task_environment()->AdvanceClock(base::Days(7));
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(kWeeklyActiveDesksHistogram, 1, 1);
  EXPECT_EQ(2u,
            histogram_tester.GetAllSamples(kWeeklyActiveDesksHistogram).size());

  // Activate the second desk and wait on it. Activate the first desk and wait
  // on it. Activate the second desk again. Let a week elapse. There should be a
  // new entry for two since we shouldn't count activating the second desk
  // twice.
  EXPECT_EQ(1, Desk::GetWeeklyActiveDesks());
  ActivateDesk(desk_2);
  task_environment()->FastForwardBy(base::Seconds(5));
  ActivateDesk(desk_1);
  task_environment()->FastForwardBy(base::Seconds(5));
  ActivateDesk(desk_2);
  task_environment()->AdvanceClock(base::Days(7));
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(kWeeklyActiveDesksHistogram, 2, 1);
  EXPECT_EQ(3u,
            histogram_tester.GetAllSamples(kWeeklyActiveDesksHistogram).size());

  // Rename the third desk twice and move two windows to the fourth desk. Let a
  // week elapse. There should be a new entry for three.
  EXPECT_EQ(1, Desk::GetWeeklyActiveDesks());
  desk_3->SetName(u"foo", /*set_by_user=*/true);
  desk_3->SetName(u"bar", /*set_by_user=*/true);

  auto win1 = CreateAppWindow();
  auto win2 = CreateAppWindow();
  controller->MoveWindowFromActiveDeskTo(
      win1.get(), desk_4, win1->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kSendToDesk);
  controller->MoveWindowFromActiveDeskTo(
      win2.get(), desk_4, win2->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kSendToDesk);

  task_environment()->AdvanceClock(base::Days(7));
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(kWeeklyActiveDesksHistogram, 3, 1);
  EXPECT_EQ(4u,
            histogram_tester.GetAllSamples(kWeeklyActiveDesksHistogram).size());

  // Wait six days on the current desk. Since a week hasn't elapsed relative to
  // the previous report, this should not report anything. Currently the weekly
  // active desks count should be one, so check the "one-count" bucket to make
  // sure it matches its previous number of entries.
  EXPECT_EQ(1, Desk::GetWeeklyActiveDesks());
  const int number_of_one_bucket_entries =
      histogram_tester.GetBucketCount(kWeeklyActiveDesksHistogram, 1);
  task_environment()->AdvanceClock(base::Days(6));
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(kWeeklyActiveDesksHistogram, 1,
                                     number_of_one_bucket_entries);

  // Wait one more day and it should now report an entry for one, accounting for
  // the current active desk.
  task_environment()->AdvanceClock(base::Days(1));
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(kWeeklyActiveDesksHistogram, 1,
                                     number_of_one_bucket_entries + 1);
}

// A class that maintains a window created inside of a test. If the window is
// destroyed from outside of the class, it releases the window's unique pointer
// to prevent use-after-free issues.
class WindowHolder : public aura::WindowObserver {
 public:
  explicit WindowHolder(std::unique_ptr<aura::Window> window)
      : window_(std::move(window)) {
    DCHECK(window_);
    window_->AddObserver(this);
  }

  WindowHolder(const WindowHolder&) = delete;
  WindowHolder& operator=(const WindowHolder&) = delete;

  ~WindowHolder() override {
    if (window_) {
      window_->RemoveObserver(this);
    }
    // `window_` is destroyed automatically here through the unique pointer.
  }

  aura::Window* window() { return window_.get(); }

  bool is_valid() const { return !!window_; }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    // The window was destroyed from outside of this WindowHolder (such as
    // through the `Desk::CloseAllAppWindows` function). In this case, we do not
    // want the unique pointer `window_` to try to destroy the already-destroyed
    // window, so we need to remove this observer and release the unique
    // pointer.
    DCHECK_EQ(window, window_.get());
    window_->RemoveObserver(this);
    window_.release();
  }

 private:
  std::unique_ptr<aura::Window> window_;
};

class DesksCloseAllTest : public DesksTest {
 public:
  DesksCloseAllTest()
      : test_close_all_window_close_timeout_(
            DesksTestApi::SetCloseAllWindowCloseTimeout(
                base::Milliseconds(close_all_window_close_timeout_ms_))) {}
  DesksCloseAllTest(const DesksCloseAllTest&) = delete;
  DesksCloseAllTest& operator=(const DesksCloseAllTest&) = delete;
  ~DesksCloseAllTest() override = default;

  // Clicks on the close-all button for the desk at index `index`.
  void ClickOnCloseAllButtonForDesk(size_t index) {
    ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
    const auto* desks_bar_view = GetPrimaryRootDesksBarView();
    ASSERT_LT(index, desks_bar_view->mini_views().size());

    LeftClickOn(desks_bar_view->mini_views()[index]
                    ->desk_action_view()
                    ->close_all_button());
    RunScheduledLayoutForAllOverviewDeskBars();
  }

  // Executes the close-all context menu command for the desk at index `index`.
  void ExecuteContextMenuCloseAllForDesk(size_t index) {
    ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
    const auto* desks_bar_view = GetPrimaryRootDesksBarView();
    ASSERT_LT(index, desks_bar_view->mini_views().size());

    // Run the context menu command for closing a desk with all of its windows.
    auto* menu_controller = DesksTestApi::GetContextMenuForDesk(
        DeskBarViewBase::Type::kOverview, index);
    menu_controller->ExecuteCommand(
        static_cast<int>(DeskActionContextMenu::CommandId::kCloseAll),
        /*event_flags=*/0);
  }

  // Opens the context menu for the mini view at `index`.
  void OpenContextMenuForMiniView(int index) {
    ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
    const DeskPreviewView* desk_preview_view =
        GetPrimaryRootDesksBarView()->mini_views()[index]->desk_preview();
    const gfx::Point desk_preview_view_center =
        desk_preview_view->GetBoundsInScreen().CenterPoint();
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(desk_preview_view_center);
    event_generator->ClickRightButton();
    ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
  }

  void ClickOnUndoDeskRemovalButton() {
    views::LabelButton* dismiss_button =
        DesksTestApi::GetCloseAllUndoToastDismissButton();
    const gfx::Point button_center =
        dismiss_button->GetBoundsInScreen().CenterPoint();
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(button_center);
    event_generator->ClickLeftButton();
  }

 protected:
  const int close_all_window_close_timeout_ms_ = 20;
  base::AutoReset<base::TimeDelta> test_close_all_window_close_timeout_;
};

// Runs through test cases for closing active and inactive desks with windows in
// overview.
TEST_P(DesksCloseAllTest, CloseDesksWithWindowsInOverview) {
  // Possible sources for "close all" actions. `kCloseAllButton` means that we
  // will be trying to close the desk through the designated button on the
  // desk's `DeskActionView`, while `kContextMenu` means we will be trying to
  // close the desk by executing the desk's "close all" context menu option. As
  // we add more ways to perform a "close all" action (such as through an
  // accelerator) we can add more cases to this enum.
  enum class CloseAllSource {
    kCloseAllButton,
    kContextMenu,
  };

  struct {
    const std::string scope_trace;
    const CloseAllSource source;
  } kTestCases[] = {
      {"Remove desks using close all button", CloseAllSource::kCloseAllButton},
      {"Remove desks using close all context menu option",
       CloseAllSource::kContextMenu},
  };

  auto* controller = DesksController::Get();

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.scope_trace);

    // Create three desks so that we can close one inactive desk and one active
    // desk.
    NewDesk();
    NewDesk();
    ASSERT_EQ(3u, controller->desks().size());
    Desk* desk_1 = controller->GetDeskAtIndex(0);
    Desk* desk_2 = controller->GetDeskAtIndex(1);
    Desk* desk_3 = controller->GetDeskAtIndex(2);
    ASSERT_TRUE(desk_1->is_active());

    // Create two `WindowHolder`s and assign one window to each desk.
    WindowHolder win1(CreateAppWindow());
    WindowHolder win2(CreateAppWindow());
    controller->SendToDeskAtIndex(win1.window(), 0);
    controller->SendToDeskAtIndex(win2.window(), 1);
    ASSERT_EQ(1u, desk_1->windows().size());
    ASSERT_EQ(1u, desk_2->windows().size());

    EnterOverview();
    ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

    // Execute and evaluate a test case for an inactive desk and an active desk.
    // We do it in this order so that we do not switch the active desk.
    if (test_case.source == CloseAllSource::kCloseAllButton) {
      ClickOnCloseAllButtonForDesk(1);
      SimulateWaitForCloseAll();
      EXPECT_FALSE(DesksTestApi::DesksControllerHasDesk(desk_2));
      EXPECT_TRUE(win1.is_valid());
      // `win2` is closed along with `desk_2` and should become invalid.
      EXPECT_FALSE(win2.is_valid());

      ClickOnCloseAllButtonForDesk(0);
      SimulateWaitForCloseAll();
      EXPECT_FALSE(DesksTestApi::DesksControllerHasDesk(desk_1));
      // `win1` is closed along with `desk_1` and should become invalid.
      EXPECT_FALSE(win1.is_valid());
    } else if (test_case.source == CloseAllSource::kContextMenu) {
      ExecuteContextMenuCloseAllForDesk(1);
      SimulateWaitForCloseAll();
      EXPECT_FALSE(DesksTestApi::DesksControllerHasDesk(desk_2));
      // `win1` and `win2` should both be valid at this point because `desk_2`
      // is being preserved.
      EXPECT_TRUE(win1.is_valid());
      // `win2` is closed along with `desk_2` and should become invalid.
      EXPECT_FALSE(win2.is_valid());

      ExecuteContextMenuCloseAllForDesk(0);
      SimulateWaitForCloseAll();
      EXPECT_FALSE(DesksTestApi::DesksControllerHasDesk(desk_1));
      // `win1` is closed along with `desk_1` and should become invalid.
      EXPECT_FALSE(win1.is_valid());
    }

    // Desk activation should have changed to `desk_3` and we should still be in
    // overview.
    EXPECT_TRUE(desk_3->is_active());
    EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

    // We should have one desk remaining.
    EXPECT_EQ(1u, GetPrimaryRootDesksBarView()->mini_views().size());
  }
}

// Test that a stored desk is cleared when we add a desk.
TEST_P(DesksCloseAllTest, ClearStoredDeskWhenDeskAdded) {
  // Create two desks with one app window each.
  WindowHolder win1(CreateAppWindow());
  WindowHolder win2(CreateAppWindow());
  NewDesk();
  auto* controller = DesksController::Get();
  controller->SendToDeskAtIndex(win1.window(), 0);
  controller->SendToDeskAtIndex(win2.window(), 1);
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  ASSERT_EQ(1u, desk_1->windows().size());
  ASSERT_EQ(1u, desk_2->windows().size());
  ASSERT_TRUE(desk_1->is_active());

  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Remove `desk_2` to store it in the `controller`.
  RemoveDesk(desk_2, DeskCloseType::kCloseAllWindowsAndWait);
  ASSERT_TRUE(desk_2->is_desk_being_removed());
  ASSERT_EQ(1u, controller->desks().size());
  ASSERT_TRUE(DesksTestApi::DesksControllerCanUndoDeskRemoval());

  // Creating a new desk should clear the stored desk.
  NewDesk();
  EXPECT_EQ(2u, controller->desks().size());
  EXPECT_FALSE(DesksTestApi::DesksControllerCanUndoDeskRemoval());
}

// Test that a stored desk is cleared when closing another desk. If the second
// desk is being removed by a close-all-and-wait operation, then the second desk
// should replace the first desk in storage. If the second desk is being removed
// by a combine desks operation or an immediate close-all operation, then the
// stored desk should be cleared.
TEST_P(DesksCloseAllTest, ClearStoredDeskWhenClosingAnotherDesk) {
  // These test cases are differentiated on whether they use the
  // close-all-and-wait operation to remove the second desk (where
  // `desk_close_type` is `DeskCloseType::kCloseAllWindowsAndWait`), if they
  // use the combine desks operation to remove the second desk (where
  // `desk_close_type` is `DeskCloseType::kCombineDesks), or if they use the
  // immediate close-all operation to remove the second desk (where
  // `desk_close_type` is `DeskCloseType::kCloseAllWindows`).
  struct {
    const std::string scope_trace;
    const DeskCloseType desk_close_type;
  } kTestCases[] = {
      {"Remove second desk with close-all-and-wait",
       DeskCloseType::kCloseAllWindowsAndWait},
      {"Remove second desk with combine desks", DeskCloseType::kCombineDesks},
      {"Remove second desk with close-all", DeskCloseType::kCloseAllWindows},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histogram_tester;
    SCOPED_TRACE(test_case.scope_trace);
    // Create two desks with one app window each.
    WindowHolder win1(CreateAppWindow());
    WindowHolder win2(CreateAppWindow());

    // Create one additional desk (so that we can remove the first two).
    NewDesk();
    NewDesk();
    auto* controller = DesksController::Get();
    controller->SendToDeskAtIndex(win1.window(), 0);
    controller->SendToDeskAtIndex(win2.window(), 1);
    Desk* desk_1 = controller->GetDeskAtIndex(0);
    Desk* desk_2 = controller->GetDeskAtIndex(1);
    Desk* desk_3 = controller->GetDeskAtIndex(2);
    ASSERT_EQ(1u, desk_1->windows().size());
    ASSERT_EQ(1u, desk_2->windows().size());
    ASSERT_EQ(0u, desk_3->windows().size());
    ASSERT_TRUE(desk_1->is_active());

    EnterOverview();
    ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

    // Remove `desk_1` by a close-all-and-wait operation to store it in the
    // `controller`.
    RemoveDesk(desk_1, DeskCloseType::kCloseAllWindowsAndWait);
    ASSERT_TRUE(desk_1->is_desk_being_removed());
    ASSERT_EQ(2u, controller->desks().size());
    ASSERT_TRUE(desk_2->is_active());
    ASSERT_TRUE(DesksTestApi::DesksControllerCanUndoDeskRemoval());
    EXPECT_EQ(GetPrimaryUserPrefService()->GetInteger(prefs::kDesksActiveDesk),
              0);

    RemoveDesk(desk_2, test_case.desk_close_type);
    EXPECT_EQ(1u, controller->desks().size());
    EXPECT_NE(desk_1, controller->GetDeskAtIndex(0));
    EXPECT_NE(desk_2, controller->GetDeskAtIndex(0));

    switch (test_case.desk_close_type) {
      case DeskCloseType::kCloseAllWindowsAndWait:
        // If we are waiting after using close-all to remove the second desk,
        // then desk_2 should still be preserved, and will maintain
        // `desk_2->is_desk_being_removed()` to be true.
        EXPECT_TRUE(desk_2->is_desk_being_removed());
        EXPECT_TRUE(DesksTestApi::DesksControllerCanUndoDeskRemoval());
        break;
      case DeskCloseType::kCombineDesks:
      case DeskCloseType::kCloseAllWindows:
        // `kCombineDesks` and `kCloseAllWindows` should both clear the removed
        // desk immediately.
        EXPECT_FALSE(DesksTestApi::DesksControllerCanUndoDeskRemoval());
        break;
    }
  }
}

// Tests that a desk that is closed with all of its windows is restored to its
// original position and its original active state when the undo callback is
// run. Also tests that a desk is destroyed when we allow for the undo toast to
// expire.
TEST_P(DesksCloseAllTest, RestoreOrDestroyDeskWithToast) {
  // These test cases are differentiated by whether they run the undo toast's
  // callback to restore the desk (when `restore_desk` is true) or if they let
  // the undo toast expire and destroy the removed desk (when `restore_desk` is
  // false).
  struct {
    const std::string scope_trace;
    const bool restore_desk;
  } kTestCases[] = {
      {"Restore removed desk by undo", true},
      {"Allow undo toast to expire", false},
  };

  WindowHolder window(CreateAppWindow());
  NewDesk();
  auto* controller = DesksController::Get();
  ASSERT_EQ(2u, controller->desks().size());
  controller->SendToDeskAtIndex(window.window(), 0);
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  ASSERT_EQ(1u, desk_1->windows().size());
  ASSERT_EQ(0u, desk_2->windows().size());
  ASSERT_TRUE(desk_1->is_active());

  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.scope_trace);
    RemoveDesk(desk_1, DeskCloseType::kCloseAllWindowsAndWait);
    ASSERT_TRUE(desk_1->is_desk_being_removed());
    ASSERT_EQ(1u, controller->desks().size());
    ASSERT_TRUE(desk_2->is_active());
    ASSERT_TRUE(DesksTestApi::DesksControllerCanUndoDeskRemoval());
    ASSERT_TRUE(window.is_valid());
    EXPECT_EQ(GetPrimaryUserPrefService()->GetInteger(prefs::kDesksActiveDesk),
              0);

    if (test_case.restore_desk) {
      // When `desk_1` is restored it should be back in its original position
      // and should be active again.
      ClickOnUndoDeskRemovalButton();
      EXPECT_FALSE(desk_1->is_desk_being_removed());
      EXPECT_EQ(2u, controller->desks().size());
      EXPECT_TRUE(desk_1->is_active());
      EXPECT_EQ(desk_1, controller->GetDeskAtIndex(0));
      EXPECT_FALSE(DesksTestApi::DesksControllerCanUndoDeskRemoval());
      EXPECT_TRUE(window.is_valid());
      EXPECT_EQ(
          GetPrimaryUserPrefService()->GetInteger(prefs::kDesksActiveDesk), 0);
    } else {
      // Because undo toasts persist on hover, we need to move the cursor
      // outside of the undo toast to start the countdown for its expiration.
      GetEventGenerator()->MoveMouseTo(gfx::Point(0, 0));

      // When we wait for the undo toast to expire, `desk_1` should be
      // destroyed.
      SimulateWaitForCloseAll();
      EXPECT_EQ(1u, controller->desks().size());
      EXPECT_FALSE(DesksTestApi::DesksControllerCanUndoDeskRemoval());
      EXPECT_FALSE(window.is_valid());
    }
  }
}

// Checks that the combine desks button and context menu option are not visible
// when there are no windows on a desk, and that they are visible on desks with
// windows.
TEST_P(DesksCloseAllTest, HideCombineDesksOptionWhenNoWindowsOnDesk) {
  // Create a new desk with no windows to have an expanded desks bar view with
  // mini views.
  NewDesk();
  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  DeskMiniView* mini_view = GetPrimaryRootDesksBarView()->mini_views()[0];
  auto* event_generator = GetEventGenerator();
  if (!features::IsSavedDeskUiRevampEnabled()) {
    // We need to hover over the desk preview to properly check the combine
    // desks button's visibility.
    event_generator->MoveMouseTo(
        mini_view->desk_preview()->GetBoundsInScreen().CenterPoint());
    EXPECT_FALSE(
        mini_view->desk_action_view()->combine_desks_button()->GetVisible());
  }

  // We need to open the context menu to trigger its creation.
  OpenContextMenuForMiniView(0);
  // If there are no windows, there should only be 1 option in the context menu.
  EXPECT_EQ(1u, DesksTestApi::GetContextMenuModelForDesk(
                    DeskBarViewBase::Type::kOverview, 0)
                    .GetItemCount());
  event_generator->ClickLeftButton();

  // Add a window and check to see if that causes the creation of the context
  // option for combining desks. Closing and reopening overview will invalidate
  // the `mini_view` object, so we need to get another one.
  auto window = CreateAppWindow();
  DesksController::Get()->SendToDeskAtIndex(window.get(), 0);
  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
  mini_view = GetPrimaryRootDesksBarView()->mini_views()[0];

  if (!features::IsSavedDeskUiRevampEnabled()) {
    event_generator->MoveMouseTo(
        mini_view->desk_preview()->GetBoundsInScreen().CenterPoint());
    EXPECT_TRUE(
        mini_view->desk_action_view()->combine_desks_button()->GetVisible());
  }
  OpenContextMenuForMiniView(0);

  if (features::IsSavedDeskUiRevampEnabled()) {
    // The saved desk is part of the context menu with this feature on, although
    // it may be disabled.
    EXPECT_EQ(3u, DesksTestApi::GetContextMenuModelForDesk(
                      DeskBarViewBase::Type::kOverview, 0)
                      .GetItemCount());
  } else {
    EXPECT_EQ(2u, DesksTestApi::GetContextMenuModelForDesk(
                      DeskBarViewBase::Type::kOverview, 0)
                      .GetItemCount());
  }
}

TEST_P(DesksCloseAllTest, AccessibleName) {
  // Create a new desk with no windows to have an expanded desks bar view with
  // mini views.
  NewDesk();
  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  DeskMiniView* mini_view = GetPrimaryRootDesksBarView()->mini_views()[0];

  ui::AXNodeData data;
  mini_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role,
            mini_view->desk_preview()->GetViewAccessibility().GetCachedRole());
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(IDS_ASH_DESKS_DESK_ACCESSIBLE_NAME,
                                       mini_view->desk()->name()));

  mini_view->desk()->SetName(u"Sample Desk Name", /*set_by_user=*/true);

  data = ui::AXNodeData();
  mini_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringFUTF16(IDS_ASH_DESKS_DESK_ACCESSIBLE_NAME,
                                       mini_view->desk()->name()));
}

// Tests that the shortcut to close all (Ctrl + Shift + W) on a desk mini view
// works as expected.
TEST_P(DesksCloseAllTest, ShortcutCloseAll) {
  WindowHolder window1(CreateAppWindow());
  WindowHolder window2(CreateAppWindow());
  NewDesk();
  auto* controller = DesksController::Get();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  ASSERT_TRUE(desk_1->is_active());
  ASSERT_TRUE(base::Contains(desk_1->windows(), window1.window()));
  ASSERT_TRUE(base::Contains(desk_1->windows(), window2.window()));

  EnterOverview();
  auto* overview_session = OverviewController::Get()->overview_session();
  ASSERT_TRUE(overview_session);

  auto* desks_bar =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();
  auto* mini_view = desks_bar->mini_views()[0].get();

  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_TRUE(mini_view->desk_preview()->HasFocus());

  // Tests that after hitting Ctrl + Shift + W, the desk is destroyed along with
  // all it's app windows.
  PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  SimulateWaitForCloseAll();
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_FALSE(window1.is_valid());
  EXPECT_FALSE(window2.is_valid());
}

TEST_P(DesksCloseAllTest, ShortcutUndoCloseAll) {
  WindowHolder window1(CreateAppWindow());
  WindowHolder window2(CreateAppWindow());
  NewDesk();
  auto* controller = DesksController::Get();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  ASSERT_TRUE(desk_1->is_active());
  ASSERT_TRUE(base::Contains(desk_1->windows(), window1.window()));
  ASSERT_TRUE(base::Contains(desk_1->windows(), window2.window()));

  EnterOverview();
  auto* overview_session = OverviewController::Get()->overview_session();
  ASSERT_TRUE(overview_session);

  // Closes the active desk.
  ClickOnCloseAllButtonForDesk(0);
  ASSERT_TRUE(DesksTestApi::DesksControllerCanUndoDeskRemoval());
  ASSERT_EQ(1u, controller->desks().size());

  // Tests that after hitting Ctrl + Z, the desk deleting is undone.
  PressAndReleaseKey(ui::VKEY_Z, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(2u, controller->desks().size());
  EXPECT_TRUE(window1.is_valid());
  EXPECT_TRUE(window2.is_valid());
}

// Tests CloseAll on active desk will close app windows on it.
TEST_P(DesksCloseAllTest, CloseActiveDeskCloseWindows) {
  WindowHolder window1(CreateAppWindow());
  WindowHolder window2(CreateAppWindow());

  NewDesk();

  auto* controller = DesksController::Get();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  ASSERT_TRUE(desk_1->is_active());
  ASSERT_TRUE(base::Contains(desk_1->windows(), window1.window()));
  ASSERT_TRUE(base::Contains(desk_1->windows(), window2.window()));

  // Closes the active desk.
  RemoveDesk(desk_1, DeskCloseType::kCloseAllWindowsAndWait);

  // Waits for the toast to disappear.
  SimulateWaitForCloseAll();
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_FALSE(window1.is_valid());
  EXPECT_FALSE(window2.is_valid());
}

// Tests CloseAll will forcefully close window that is not closed in time.
TEST_P(DesksCloseAllTest, ForceCloseWindows) {
  WindowHolder window1(CreateAppWindow());

  WindowHolder window2(CreateAppWindow(
      gfx::Rect(), chromeos::AppType::SYSTEM_APP,
      ShellWindowId::kShellWindowId_Invalid, new StuckWidgetDelegate()));

  NewDesk();
  auto* controller = DesksController::Get();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  ASSERT_TRUE(desk_1->is_active());
  ASSERT_TRUE(base::Contains(desk_1->windows(), window1.window()));
  ASSERT_TRUE(base::Contains(desk_1->windows(), window2.window()));

  RemoveDesk(desk_1, DeskCloseType::kCloseAllWindowsAndWait);

  // Wait, but not long enough for slow window (`window2`) to forcefully be
  // closed.
  DesksController::Get()->MaybeCommitPendingDeskRemoval();
  WaitForMilliseconds(close_all_window_close_timeout_ms_ / 2);
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_FALSE(window1.is_valid());
  EXPECT_TRUE(window2.is_valid());

  // Waits for long enough for `DesksController` to forcefully close the window.
  WaitForMilliseconds(close_all_window_close_timeout_ms_);
  EXPECT_FALSE(window2.is_valid());
}

// Checks that the desk preview highlight overlay is visible on a desk preview
// view only when its corresponding desk mini view's `DeskActionContextMenu` is
// active.
TEST_P(DesksCloseAllTest, DeskPreviewHighlightShowsWhenContextMenuIsOpen) {
  // We need to make the display this large so that the preview view is
  // right-clickable.
  UpdateDisplay("1366x768");
  NewDesk();

  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // The highlight overlay should start out invisible.
  views::View* highlight_overlay =
      DesksTestApi::GetHighlightOverlayForDeskPreview(
          DeskBarViewBase::Type::kOverview, 0);
  ASSERT_FALSE(highlight_overlay->GetVisible());

  // Open the context menu for the first desk and check that highlight overlay
  // is now visible.
  DeskPreviewView* desk_preview_view =
      GetPrimaryRootDesksBarView()->mini_views()[0]->desk_preview();
  gfx::Point desk_preview_view_center =
      desk_preview_view->GetBoundsInScreen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(desk_preview_view_center);
  event_generator->ClickRightButton();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
  ASSERT_TRUE(highlight_overlay->GetVisible());

  // Close the context menu and check that the highlight overlay is no longer
  // visible.
  event_generator->ClickLeftButton();

  // We need to wait for the `DeskActionContextMenu` to close, because
  // `SimpleMenuModel::MenuWillClose()` runs post tasks asynchronously.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(highlight_overlay->GetVisible());
}

// Checks that the combine desks tooltip's validity is maintained whenever the
// user adds a desk, closes a desk, moves a desk, or changes the name of a desk.
TEST_P(DesksCloseAllTest, CombineDesksTooltipIsUpdatedOnUserActions) {
  if (features::IsSavedDeskUiRevampEnabled()) {
    GTEST_SKIP()
        << "Save desk buttons have been moved to the desk context menu. The "
           "associated context menu item has text so it doesn't need a tooltip";
  }
  // Possible sources for tooltip updates.
  enum class UpdateSource {
    kAddDesk,
    kCloseDesk,
    kMoveActiveDesk,
    kMoveNonActiveDesk,
    kChangeActiveDeskName,
    kChangeNonActiveDeskName,
  };

  struct {
    const std::string scope_trace;
    const UpdateSource source;

    // The desk name that we expect `desk_1` in the test case to point to as the
    // target for its combine desks operation after the test is performed.
    const std::u16string expected_target_1;

    // The desk name that we expect `desk_2` in the test case to point to as the
    // target for its combine desks operation after the test is performed.
    const std::u16string expected_target_2;
  } kTestCases[] = {
      {"Adding desk", UpdateSource::kAddDesk, u"Desk 2", u"Desk 1"},
      {"Closing desk", UpdateSource::kCloseDesk, u"Desk 2", u"Desk 1"},
      {"Moving active desk", UpdateSource::kMoveActiveDesk, u"Desk 1",
       u"Desk 2"},
      {"Moving non-active desk", UpdateSource::kMoveNonActiveDesk, u"Desk 2",
       u"Desk 1"},
      {"Changing active desk name", UpdateSource::kChangeActiveDeskName,
       u"Desk 2", u"goo"},
      {"Changing non-active desk name", UpdateSource::kChangeNonActiveDeskName,
       u"gle", u"goo"},
  };

  // We need to make the display this large so that the mini views are
  // draggable.
  UpdateDisplay("1366x768");
  auto* controller = DesksController::Get();

  // Create two initial desks with one window each.
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  Desk* desk_2 = controller->GetDeskAtIndex(1);

  WindowHolder win1(CreateAppWindow());
  WindowHolder win2(CreateAppWindow());
  controller->SendToDeskAtIndex(win1.window(), 0);
  controller->SendToDeskAtIndex(win2.window(), 1);
  ASSERT_EQ(1u, desk_1->windows().size());
  ASSERT_EQ(1u, desk_2->windows().size());

  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  const DeskBarViewBase* desks_bar_view = GetPrimaryRootDesksBarView();

  // Cache the mini views and their name views and combine desks buttons.
  DeskMiniView* mini_view_1 = desks_bar_view->mini_views()[0];
  DeskMiniView* mini_view_2 = desks_bar_view->mini_views()[1];
  DeskNameView* desk_name_view_1 = mini_view_1->desk_name_view();
  DeskNameView* desk_name_view_2 = mini_view_2->desk_name_view();
  const CloseButton* combine_desks_button_1 =
      mini_view_1->desk_action_view()->combine_desks_button();
  const CloseButton* combine_desks_button_2 =
      mini_view_2->desk_action_view()->combine_desks_button();

  const std::u16string tooltip_prefix = u"Combine with ";
  auto* event_generator = GetEventGenerator();
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.scope_trace);

    ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
    ASSERT_EQ(tooltip_prefix + controller->GetCombineDesksTargetName(desk_1),
              combine_desks_button_1->GetTooltipText());
    ASSERT_EQ(tooltip_prefix + controller->GetCombineDesksTargetName(desk_2),
              combine_desks_button_2->GetTooltipText());

    switch (test_case.source) {
      case UpdateSource::kAddDesk:
        NewDesk();
        break;
      case UpdateSource::kCloseDesk:
        ASSERT_EQ(3u, controller->desks().size());
        RemoveDesk(controller->GetDeskAtIndex(2));
        break;
      case UpdateSource::kMoveActiveDesk:
        ASSERT_TRUE(controller->GetDeskAtIndex(0)->is_active());
        StartDragDeskPreview(mini_view_1, event_generator);
        ASSERT_TRUE(desks_bar_view->IsDraggingDesk());
        event_generator->MoveMouseTo(
            mini_view_2->GetPreviewBoundsInScreen().CenterPoint());
        event_generator->ReleaseLeftButton();
        RunScheduledLayoutForAllOverviewDeskBars();
        break;
      case UpdateSource::kMoveNonActiveDesk:
        ASSERT_FALSE(controller->GetDeskAtIndex(0)->is_active());
        StartDragDeskPreview(mini_view_2, event_generator);
        EXPECT_TRUE(desks_bar_view->IsDraggingDesk());
        event_generator->MoveMouseTo(
            mini_view_1->GetPreviewBoundsInScreen().CenterPoint());
        event_generator->ReleaseLeftButton();
        RunScheduledLayoutForAllOverviewDeskBars();
        break;
      case UpdateSource::kChangeActiveDeskName:
        ASSERT_TRUE(controller->GetDeskAtIndex(0)->is_active());
        event_generator->MoveMouseTo(
            desk_name_view_1->GetBoundsInScreen().CenterPoint());
        event_generator->ClickLeftButton();
        PressAndReleaseKey(ui::VKEY_G);
        PressAndReleaseKey(ui::VKEY_O);
        PressAndReleaseKey(ui::VKEY_O);
        PressAndReleaseKey(ui::VKEY_RETURN);
        RunScheduledLayoutForAllOverviewDeskBars();
        break;
      case UpdateSource::kChangeNonActiveDeskName:
        ASSERT_EQ(u"goo", mini_view_1->desk()->name());
        ASSERT_FALSE(controller->GetDeskAtIndex(1)->is_active());
        LeftClickOn(desk_name_view_2);
        PressAndReleaseKey(ui::VKEY_G);
        PressAndReleaseKey(ui::VKEY_L);
        PressAndReleaseKey(ui::VKEY_E);
        PressAndReleaseKey(ui::VKEY_RETURN);
        RunScheduledLayoutForAllOverviewDeskBars();
        break;
    }

    EXPECT_EQ(tooltip_prefix + test_case.expected_target_1,
              combine_desks_button_1->GetTooltipText());
    EXPECT_EQ(tooltip_prefix + test_case.expected_target_2,
              combine_desks_button_2->GetTooltipText());
  }
}

// Test metrics are being recorded in close all case.
TEST_P(DesksCloseAllTest, TestMetricsRecordingWhenCloseAllWindows) {
  struct {
    const std::string scope_trace;
    const bool restore_desk;
  } kTestCases[] = {
      {"Restore removed desk by undo", true},
      {"Allow undo toast to expire", false},
  };
  base::HistogramTester histogram_tester;

  // Set up a new desk with two windows
  WindowHolder window(CreateAppWindow());
  WindowHolder window1(CreateAppWindow());
  NewDesk();
  auto* controller = DesksController::Get();
  controller->SendToDeskAtIndex(window.window(), 0);
  controller->SendToDeskAtIndex(window1.window(), 0);
  EnterOverview();

  int remove_desk_type_count = 0;
  int undo_toast_expired_count = 0;
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.scope_trace);
    auto* menu_controller = DesksTestApi::GetContextMenuForDesk(
        DeskBarViewBase::Type::kOverview, 0);
    menu_controller->ExecuteCommand(
        static_cast<int>(DeskActionContextMenu::CommandId::kCloseAll),
        /*event_flags=*/0);
    RunScheduledLayoutForAllOverviewDeskBars();
    // Record RemoveDeskType: closeAll, combine and closeAllAndWait
    histogram_tester.ExpectBucketCount("Ash.Desks.RemoveDeskType",
                                       DeskCloseType::kCloseAllWindowsAndWait,
                                       ++remove_desk_type_count);

    if (test_case.restore_desk) {
      // When `desk_1` is restored it should be back in its original position
      // and should be active again.
      views::LabelButton* dismiss_button =
          DesksTestApi::GetCloseAllUndoToastDismissButton();
      const gfx::Point button_center =
          dismiss_button->GetBoundsInScreen().CenterPoint();
      auto* event_generator = GetEventGenerator();
      event_generator->MoveMouseTo(button_center);
      event_generator->ClickLeftButton();
      RunScheduledLayoutForAllOverviewDeskBars();
      // Record click undo button in toast after remove all
      histogram_tester.ExpectTotalCount("Ash.Desks.CloseAllUndo", 1);
      // Record undo toast expired
      histogram_tester.ExpectTotalCount("Ash.Desks.CloseAllTotal",
                                        ++undo_toast_expired_count);
      // Record number of windows being closed.
      histogram_tester.ExpectTotalCount("Ash.Desks.NumberOfWindowsClosed2", 0);
      // Record number of windows being closed per source.
      histogram_tester.ExpectTotalCount(
          "Ash.Desks.NumberOfWindowsClosed2.Button", 0);

    } else {
      // Because undo toasts persist on hover, we need to move the cursor
      // outside of the undo toast to start the countdown for its expiration.
      GetEventGenerator()->MoveMouseTo(gfx::Point(0, 0));
      RunScheduledLayoutForAllOverviewDeskBars();

      // Simulate the toast timing out.
      DesksController::Get()->MaybeCommitPendingDeskRemoval();

      // Record undo toast expired.
      histogram_tester.ExpectTotalCount("Ash.Desks.CloseAllTotal",
                                        ++undo_toast_expired_count);
      // Record number of windows being closed.
      histogram_tester.ExpectUniqueSample("Ash.Desks.NumberOfWindowsClosed2", 2,
                                          1);
      // Record number of windows being closed per source.
      histogram_tester.ExpectUniqueSample(
          "Ash.Desks.NumberOfWindowsClosed2.Button", 2, 1);
    }
  }
}

TEST_P(DesksCloseAllTest, DeskPreviewAccessibleProperties) {
  NewDesk();
  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Long press on the first desk preview view.
  DeskPreviewView* desk_preview_view =
      GetPrimaryRootDesksBarView()->mini_views()[0]->desk_preview();

  ui::AXNodeData data;
  desk_preview_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(
      data.GetString16Attribute(ax::mojom::StringAttribute::kRoleDescription),
      l10n_util::GetStringUTF16(IDS_ASH_DESKS_DESK_PREVIEW_ROLE_DESCRIPTION));
}

// Checks that a `DeskActionContextMenu` opens when the user long-presses a
// desk's mini view.
TEST_P(DesksCloseAllTest, ContextMenuOpensOnLongPress) {
  NewDesk();
  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Long press on the first desk preview view.
  const DeskPreviewView* desk_preview_view =
      GetPrimaryRootDesksBarView()->mini_views()[0]->desk_preview();
  const gfx::Point desk_preview_view_center =
      desk_preview_view->GetBoundsInScreen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  LongGestureTap(desk_preview_view_center, event_generator);

  EXPECT_TRUE(DesksTestApi::IsContextMenuRunningForDesk(
      DeskBarViewBase::Type::kOverview, 0));
}

// Tests that desks can be closed in quick succession while still saving the
// removed desk.
TEST_P(DesksCloseAllTest, CanCloseMultipleDesksInSuccessionAndUndo) {
  NewDesk();
  NewDesk();
  auto* controller = DesksController::Get();
  ASSERT_EQ(3u, controller->desks().size());

  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  ClickOnCloseAllButtonForDesk(0);
  ASSERT_TRUE(DesksTestApi::DesksControllerCanUndoDeskRemoval());
  ClickOnCloseAllButtonForDesk(0);
  ASSERT_TRUE(DesksTestApi::DesksControllerCanUndoDeskRemoval());
  ClickOnUndoDeskRemovalButton();
  EXPECT_EQ(2u, controller->desks().size());
}

// Tests that an active desk's windows are restored with an identity transform
// when it's removal is undone outside of overview mode.
TEST_P(DesksCloseAllTest,
       ActiveDeskWindowsAreRestoredProperlyOutsideOfOverview) {
  WindowHolder window(CreateAppWindow());
  NewDesk();
  auto* controller = DesksController::Get();
  ASSERT_EQ(2u, controller->desks().size());
  controller->SendToDeskAtIndex(window.window(), 0);
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  Desk* desk_2 = controller->GetDeskAtIndex(1);
  ASSERT_EQ(1u, desk_1->windows().size());
  ASSERT_EQ(0u, desk_2->windows().size());
  ASSERT_TRUE(desk_1->is_active());

  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_FALSE(window.window()->transform().IsIdentity());

  ClickOnCloseAllButtonForDesk(0);
  ExitOverview();
  ASSERT_FALSE(OverviewController::Get()->InOverviewSession());

  // Overview is destroyed asynchronously so we have to wait for it to finish.
  base::RunLoop().RunUntilIdle();

  // Clicking the undo button outside of overview triggers a desk switch
  // animation, so we have to wait for that.
  DeskSwitchAnimationWaiter waiter;
  ClickOnUndoDeskRemovalButton();
  waiter.Wait();
  EXPECT_TRUE(window.is_valid());

  // At this point, the transformation applied to the window by overview mode
  // should have been removed by the desk removal operation.
  EXPECT_TRUE(window.window()->transform().IsIdentity());
}

// Tests that we can create the maximum number of desks, remove one, and add one
// before the toast asking if the user would like to undo goes away.
TEST_P(DesksCloseAllTest, CanAddLastDeskWhileUndoToastIsBeingDisplayed) {
  auto* controller = DesksController::Get();
  while (controller->desks().size() < desks_util::GetMaxNumberOfDesks()) {
    NewDesk();
  }
  ASSERT_EQ(desks_util::GetMaxNumberOfDesks(), controller->desks().size());

  // Create a window to ensure that closing windows after removing the last desk
  // still occurs correctly.
  WindowHolder window(CreateAppWindow());
  const int last_desk_index = desks_util::GetMaxNumberOfDesks() - 1;
  controller->SendToDeskAtIndex(window.window(), last_desk_index);

  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Remove the last desk with close-all. This should show the undo toast.
  RemoveDesk(controller->GetDeskAtIndex(last_desk_index),
             DeskCloseType::kCloseAllWindowsAndWait);
  ASSERT_EQ(desks_util::GetMaxNumberOfDesks() - 1, controller->desks().size());
  ASSERT_TRUE(DesksTestApi::DesksControllerCanUndoDeskRemoval());

  // The new desk button should be enabled at this point.
  auto* new_desk_button = GetPrimaryRootDesksBarView()->new_desk_button();
  ASSERT_TRUE(new_desk_button->GetEnabled());

  // Scroll all the way to the right to ensure that the new button is visible.
  for (int i = 0; i != 3; ++i) {
    TryScrollOverviewDeskBar(DeskBarScrollDirection::kRight);
  }

  // If we click on the `new_desk_button`, we should create a new desk and
  // destroy the previously removed desk and the window inside of it.
  LeftClickOn(new_desk_button);
  EXPECT_EQ(desks_util::GetMaxNumberOfDesks(), controller->desks().size());
  EXPECT_FALSE(DesksTestApi::DesksControllerCanUndoDeskRemoval());

  // Ensure that the window is still closed properly.
  WaitForMilliseconds(close_all_window_close_timeout_ms_);
  EXPECT_FALSE(window.is_valid());
}

// Tests that windows in CloseAll will not be unparented while they are closing
// asynchronously.
TEST_P(DesksCloseAllTest, ClosingWindowsHaveParent) {
  WindowHolder window(CreateAppWindow(
      gfx::Rect(), chromeos::AppType::SYSTEM_APP,
      ShellWindowId::kShellWindowId_Invalid, new StuckWidgetDelegate()));

  NewDesk();
  auto* controller = DesksController::Get();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->GetDeskAtIndex(0);
  ASSERT_TRUE(desk_1->is_active());
  EXPECT_TRUE(base::Contains(desk_1->windows(), window.window()));

  RemoveDesk(desk_1, DeskCloseType::kCloseAllWindowsAndWait);

  // Window will still be open because it has not yet been force-closed.
  DesksController::Get()->MaybeCommitPendingDeskRemoval();
  WaitForMilliseconds(close_all_window_close_timeout_ms_ / 2);
  ASSERT_EQ(1u, controller->desks().size());
  ASSERT_TRUE(window.is_valid());

  // Closing windows should be reparented to the
  // `kShellWindowId_UnparentedContainer` window.
  EXPECT_TRUE(window.window()->GetRootWindow());
  EXPECT_EQ(kShellWindowId_UnparentedContainer,
            window.window()->parent()->GetId());
}

TEST_P(DesksCloseAllTest, TestRecordingNumerOfClosedWindowsMetrics) {
  struct {
    const std::string scope_trace;
    const DeskCloseType desk_close_type;
  } kTestCases[] = {
      {"Remove second desk with combine desks", DeskCloseType::kCombineDesks},
      {"Remove second desk with close-all-and-wait",
       DeskCloseType::kCloseAllWindowsAndWait},
      {"Remove second desk with close-all", DeskCloseType::kCloseAllWindows},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histogram_tester;

    SCOPED_TRACE(test_case.scope_trace);
    WindowHolder win1(CreateAppWindow());

    NewDesk();
    auto* controller = DesksController::Get();
    controller->SendToDeskAtIndex(win1.window(), 1);
    Desk* desk = controller->GetDeskAtIndex(1);

    EnterOverview();

    RemoveDesk(desk, test_case.desk_close_type);

    switch (test_case.desk_close_type) {
      case DeskCloseType::kCombineDesks:
        histogram_tester.ExpectTotalCount("Ash.Desks.NumberOfWindowsClosed2",
                                          0);
        histogram_tester.ExpectTotalCount(
            "Ash.Desks.NumberOfWindowsClosed2.Button", 0);
        break;
      // Wait and toast expired.
      case DeskCloseType::kCloseAllWindowsAndWait:
        // Because undo toasts persist on hover, we need to move the cursor
        // outside of the undo toast to start the countdown for its expiration.
        GetEventGenerator()->MoveMouseTo(gfx::Point(0, 0));
        DesksController::Get()->MaybeCommitPendingDeskRemoval();
        WaitForMilliseconds(200);
        histogram_tester.ExpectTotalCount("Ash.Desks.NumberOfWindowsClosed2",
                                          1);
        histogram_tester.ExpectUniqueSample(
            "Ash.Desks.NumberOfWindowsClosed2.Button", 1, 1);
        break;

      case DeskCloseType::kCloseAllWindows:
        // `kCombineDesks` and `kCloseAllWindows` should both clear the removed
        // desk immediately.
        histogram_tester.ExpectTotalCount("Ash.Desks.NumberOfWindowsClosed2",
                                          1);
        histogram_tester.ExpectUniqueSample(
            "Ash.Desks.NumberOfWindowsClosed2.Button", 1, 1);
        break;
    }
  }
}

// Tests that when a desk is created its GUID is saved in the desks restore
// data, and when a desk is removed its GUID is removed.
TEST_P(DesksTest, DeskGuidsSaved) {
  // We don't need to save the desk GUID for restore if there is only one desk.
  NewDesk();
  auto* controller = DesksController::Get();
  base::Uuid desk1_guid = controller->GetDeskAtIndex(0)->uuid();
  base::Uuid desk2_guid = controller->GetDeskAtIndex(1)->uuid();
  EXPECT_THAT(GetDeskRestoreGuids(GetPrimaryUserPrefService()),
              testing::ElementsAre(desk1_guid, desk2_guid));

  // Add a third desk, close the second desk, and check the GUIDs.
  NewDesk();
  base::Uuid desk3_guid = controller->GetDeskAtIndex(2)->uuid();
  EnterOverview();
  CloseDeskFromMiniView(GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
                            ->desks_bar_view()
                            ->mini_views()[1],
                        GetEventGenerator());
  EXPECT_THAT(GetDeskRestoreGuids(GetPrimaryUserPrefService()),
              testing::ElementsAre(desk1_guid, desk3_guid));
}

// Tests that when desks are reordered their GUIDs are reordered in the desk
// restore data correspondingly.
TEST_P(DesksTest, DeskGuidsReorder) {
  NewDesk();
  NewDesk();
  auto* controller = DesksController::Get();
  base::Uuid desk1_guid = controller->GetDeskAtIndex(0)->uuid();
  base::Uuid desk2_guid = controller->GetDeskAtIndex(1)->uuid();
  base::Uuid desk3_guid = controller->GetDeskAtIndex(2)->uuid();
  EXPECT_THAT(GetDeskRestoreGuids(GetPrimaryUserPrefService()),
              testing::ElementsAre(desk1_guid, desk2_guid, desk3_guid));

  // Swap the 2nd and 3rd desks with the mouse.
  EnterOverview();
  const auto* desks_bar_view =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();
  std::vector<raw_ptr<DeskMiniView, VectorExperimental>> mini_views =
      desks_bar_view->mini_views();
  auto* event_generator = GetEventGenerator();

  StartDragDeskPreview(mini_views[1], event_generator);
  gfx::Point desk_center_2 =
      mini_views[2]->GetPreviewBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(desk_center_2);
  event_generator->ReleaseLeftButton();

  EXPECT_THAT(GetDeskRestoreGuids(GetPrimaryUserPrefService()),
              testing::ElementsAre(desk1_guid, desk3_guid, desk2_guid));
}

TEST_P(DesksTest, DeskLacrosIdPrefs) {
  NewDesk();
  NewDesk();

  auto* controller = DesksController::Get();
  // Set some lacros profile IDs for the three desks.
  controller->GetDeskAtIndex(0)->SetLacrosProfileId(1001,
                                                    /*source=*/std::nullopt);
  controller->GetDeskAtIndex(1)->SetLacrosProfileId(2001,
                                                    /*source=*/std::nullopt);
  controller->GetDeskAtIndex(2)->SetLacrosProfileId(3001,
                                                    /*source=*/std::nullopt);
  EXPECT_THAT(GetDeskRestoreLacrosProfileIds(GetPrimaryUserPrefService()),
              testing::ElementsAre(1001, 2001, 3001));

  // Reorder the last two desks. We expect the prefs to update to match.
  controller->ReorderDesk(1, 2);
  EXPECT_THAT(GetDeskRestoreLacrosProfileIds(GetPrimaryUserPrefService()),
              testing::ElementsAre(1001, 3001, 2001));

  // Remove the first desk.
  RemoveDesk(controller->GetDeskAtIndex(0));
  EXPECT_THAT(GetDeskRestoreLacrosProfileIds(GetPrimaryUserPrefService()),
              testing::ElementsAre(3001, 2001));

  // Create a new desk, its lacros profile ID should default to 0.
  NewDesk();
  EXPECT_THAT(GetDeskRestoreLacrosProfileIds(GetPrimaryUserPrefService()),
              testing::ElementsAre(3001, 2001, 0));
}

// Tests that windows are closed when the user interacts with the shelf.
TEST_P(DesksCloseAllTest, InteractingWithShelfClosesToast) {
  auto* shelf_model = ShelfModel::Get();
  NewDesk();

  // Create a window and a shelf item for the window.
  WindowHolder window(CreateAppWindow());
  const ash::ShelfID shelf_id("cool_app");
  window.window()->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
  window.window()->SetProperty(ash::kAppIDKey, shelf_id.app_id);
  window.window()->SetProperty<int>(ash::kShelfItemTypeKey,
                                    ShelfItemType::TYPE_PINNED_APP);
  ShelfItem item;
  item.status = ShelfItemStatus::STATUS_RUNNING;
  item.type = ShelfItemType::TYPE_PINNED_APP;
  item.id = shelf_id;
  shelf_model->Add(item, std::make_unique<TestShelfItemDelegate>(item.id));

  // Enter overview and close the desk.
  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
  ClickOnCloseAllButtonForDesk(0);

  // Get the view for the shelf item.
  int item_index = shelf_model->ItemIndexByID(shelf_id);
  auto* view_model = GetPrimaryShelf()->GetShelfViewForTesting()->view_model();
  views::View* item_view = view_model->view_at(item_index);

  // Try opening the context menu for the shelf item with a right click.
  gfx::Point item_view_center = item_view->GetBoundsInScreen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(item_view_center);
  event_generator->ClickRightButton();

  // The right click should destroy the desk and the window.
  EXPECT_FALSE(DesksTestApi::DesksControllerCanUndoDeskRemoval());

  // The window will destroy asynchronously.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(window.is_valid());
}

// Regression test for http://b/356460723.
TEST_P(DesksCloseAllTest, UndoToastWorksWithChromevox) {
  NewDesk();
  Shell::Get()->accessibility_controller()->spoken_feedback().SetEnabled(true);

  // Enter overview and close the desk.
  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());
  ClickOnCloseAllButtonForDesk(/*index=*/0);
  ASSERT_EQ(1, DesksController::Get()->GetNumberOfDesks());

  views::LabelButton* button =
      DesksTestApi::GetCloseAllUndoToastDismissButton();
  ASSERT_TRUE(button);
  LeftClickOn(button);
  EXPECT_EQ(2, DesksController::Get()->GetNumberOfDesks());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
}

class DeskBarTest
    : public AshTestBase,
      public ::testing::WithParamInterface<
          testing::tuple<bool, bool, bool, DeskBarViewBase::Type>> {
 public:
  DeskBarTest()
      : test_close_all_window_close_timeout_(
            DesksTestApi::SetCloseAllWindowCloseTimeout(
                base::Milliseconds(20))) {}
  DeskBarTest(const DeskBarTest&) = delete;
  DeskBarTest& operator=(const DeskBarTest&) = delete;
  ~DeskBarTest() override = default;

  void SetUp() override {
    std::tie(use_touch_gestures_, use_16_desks_, use_desk_profiles_,
             bar_type_) = GetParam();

    scoped_feature_list_.InitWithFeatureStates(
        {{features::kFeatureManagement16Desks, use_16_desks_},
         {chromeos::features::kDeskProfiles, use_desk_profiles_}});

    AshTestBase::SetUp();

    // Wait for the desk model to have completed its initialization. Not doing
    // this would lead to flaky tests.
    ash_test_helper()->saved_desk_test_helper()->WaitForDeskModels();

    SetShowDeskButtonInShelfPref(GetPrimaryUserPrefService(), true);

    if (use_desk_profiles_) {
      // Add two lacros profiles so that the desk profile avatar shows up in
      // desk mini views.
      AddDummyLacrosDeskProfiles(2);
    }
  }

  void TearDown() override {
    AshTestBase::TearDown();
    scoped_feature_list_.Reset();
  }

  desks_storage::DeskModel* desk_model() {
    return ash_test_helper()->saved_desk_test_helper()->desk_model();
  }

  void DeleteAllSavedDesks() {
    base::RunLoop loop;
    desk_model()->DeleteAllEntries(base::BindLambdaForTesting(
        [&](desks_storage::DeskModel::DeleteEntryStatus status) {
          loop.Quit();
        }));
    loop.Run();
  }

  void ClickOrPressOnPoint(const gfx::Point& p) {
    auto* event_generator = GetEventGenerator();
    if (use_touch_gestures_) {
      event_generator->PressTouch(p);
      event_generator->ReleaseTouch();
    } else {
      event_generator->MoveMouseTo(p);
      event_generator->ClickLeftButton();
    }
  }

  void ClickOrPressOnView(const views::View* view) {
    const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
    ClickOrPressOnPoint(view_center);
  }

  void OpenDeskBar(aura::Window* root = Shell::Get()->GetPrimaryRootWindow()) {
    OpenDeskBar(root, bar_type_);
  }

  void OpenDeskBar(aura::Window* root, DeskBarViewBase::Type bar_type) {
    switch (bar_type) {
      case DeskBarViewBase::Type::kOverview:
        EnterOverview();
        break;
      case DeskBarViewBase::Type::kDeskButton:
        DesksController::Get()->desk_bar_controller()->OpenDeskBar(root);
        break;
    }
    DeskBarViewBase* const desk_bar_view = GetDeskBarView(root, bar_type);
    if (desk_bar_view) {
      views::test::RunScheduledLayout(desk_bar_view);
    }
  }

  void CloseDeskBar(aura::Window* root = Shell::Get()->GetPrimaryRootWindow()) {
    CloseDeskBar(root, bar_type_);
  }

  void CloseDeskBar(aura::Window* root, DeskBarViewBase::Type bar_type) {
    switch (bar_type) {
      case DeskBarViewBase::Type::kOverview:
        ExitOverview();
        break;
      case DeskBarViewBase::Type::kDeskButton:
        DesksController::Get()->desk_bar_controller()->CloseDeskBar(root);
        break;
    }
  }

  DeskBarViewBase* GetDeskBarView(
      aura::Window* root = Shell::Get()->GetPrimaryRootWindow()) {
    return GetDeskBarView(root, bar_type_);
  }

  DeskBarViewBase* GetDeskBarView(aura::Window* root,
                                  DeskBarViewBase::Type bar_type) {
    DeskBarViewBase* desk_bar_view = nullptr;
    switch (bar_type) {
      case DeskBarViewBase::Type::kOverview:
        if (OverviewController::Get()->InOverviewSession()) {
          desk_bar_view = GetOverviewGridForRoot(root)->desks_bar_view();
        }
        break;
      case DeskBarViewBase::Type::kDeskButton:
        desk_bar_view =
            DesksController::Get()->desk_bar_controller()->GetDeskBarView(root);
        break;
    }
    return desk_bar_view;
  }

  void EnterLibrary(aura::Window* root = Shell::Get()->GetPrimaryRootWindow()) {
    EnterLibrary(root, bar_type_);
  }

  void EnterLibrary(aura::Window* root, DeskBarViewBase::Type bar_type) {
    auto* desk_bar_view = GetDeskBarView(root, bar_type);
    ASSERT_TRUE(desk_bar_view);

    // Clicking the library button on the desk button desk bar.
    ASSERT_TRUE(desk_bar_view->library_button());
    ASSERT_TRUE(desk_bar_view->library_button()->GetVisible());
    ClickOrPressOnView(desk_bar_view->library_button());

    // It should enter overview mode and the saved desk library should be
    // visible. Desk button desk bar should be gone, and the overview desk bar
    // should show up.
    auto* overview_controller = OverviewController::Get();
    ASSERT_TRUE(overview_controller->InOverviewSession());
    RunScheduledLayoutForAllOverviewDeskBars();
    auto* overview_session = overview_controller->overview_session();
    EXPECT_TRUE(overview_session &&
                overview_session->IsShowingSavedDeskLibrary());
    EXPECT_FALSE(GetDeskBarView(root, DeskBarViewBase::Type::kDeskButton));
    EXPECT_TRUE(GetDeskBarView(root, DeskBarViewBase::Type::kOverview));
  }

  // Executes a context menu command for the desk at index `index`. `close_all`
  // indicates whether we want to close a desk with windows or combine desks.
  void ExecuteContextMenuDeskActionForDesk(size_t index,
                                           bool close_all,
                                           aura::Window* root,
                                           DeskBarViewBase::Type bar_type) {
    const auto* desks_bar_view = GetDeskBarView(root, bar_type);
    ASSERT_LT(index, desks_bar_view->mini_views().size());

    // Run the context menu command for closing a desk with all of its windows.
    auto* menu_controller =
        DesksTestApi::GetContextMenuForDesk(bar_type, index);

    const int command_id = static_cast<int>(
        close_all ? DeskActionContextMenu::CommandId::kCloseAll
                  : DeskActionContextMenu::CommandId::kCombineDesks);
    if (bar_type == DeskBarViewBase::Type::kDeskButton &&
        desks_bar_view->mini_views()[index]->desk()->is_active()) {
      DeskSwitchAnimationWaiter waiter;
      menu_controller->ExecuteCommand(command_id,
                                      /*event_flags=*/0);
      waiter.Wait();
    } else {
      menu_controller->ExecuteCommand(command_id,
                                      /*event_flags=*/0);
    }
  }

  // Closes the desk at `index` with the selected method, using the context menu
  // if we are using touch gestures.
  void CloseDeskWithButton(size_t index,
                           bool close_all,
                           aura::Window* root,
                           DeskBarViewBase::Type bar_type) {
    if (use_touch_gestures_) {
      ExecuteContextMenuDeskActionForDesk(index, close_all, root, bar_type);
    } else {
      const auto* desk_bar_view = GetDeskBarView(root, bar_type);
      ASSERT_LT(index, desk_bar_view->mini_views().size());
      auto* target_mini_view = desk_bar_view->mini_views()[index].get();
      GetEventGenerator()->MoveMouseTo(
          target_mini_view->desk_preview()->GetBoundsInScreen().CenterPoint());
      auto* target_button =
          close_all
              ? target_mini_view->desk_action_view()->close_all_button()
              : target_mini_view->desk_action_view()->combine_desks_button();
      ASSERT_TRUE(target_button->GetVisible());

      if (bar_type == DeskBarViewBase::Type::kDeskButton &&
          target_mini_view->desk()->is_active()) {
        DeskSwitchAnimationWaiter waiter;
        LeftClickOn(target_button);
        waiter.Wait();
      } else {
        LeftClickOn(target_button);
      }
    }
  }

  // TODO(http://b/350771229): Fully replace `CloseDeskWithButton` with this
  // function once Forest is enabled by default (including touch gestures).
  // Closes the desk at `index` using the "Combine desk" option in the desk
  // context menu, accessed using the context menu button. Only available if the
  // Forest feature is enabled.
  void CombineDeskWithMenu(size_t index,
                           aura::Window* root,
                           DeskBarViewBase::Type bar_type) {
    CHECK(features::IsSavedDeskUiRevampEnabled());

    // Get the menu option to save the desk as a template and click it.
    views::MenuItemView* menu_item =
        DesksTestApi::OpenDeskContextMenuAndGetMenuItem(
            root, bar_type, index,
            DeskActionContextMenu::CommandId::kCombineDesks);
    ASSERT_TRUE(menu_item);

    if (bar_type == DeskBarViewBase::Type::kDeskButton &&
        DesksController::Get()->GetActiveDeskIndex() ==
            static_cast<int>(index)) {
      DeskSwitchAnimationWaiter waiter;
      LeftClickOn(menu_item);
      waiter.Wait();
    } else {
      LeftClickOn(menu_item);
    }
  }

  bool use_touch_gestures_;
  bool use_16_desks_;
  bool use_desk_profiles_;
  DeskBarViewBase::Type bar_type_;

 private:
  base::AutoReset<base::TimeDelta> test_close_all_window_close_timeout_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that `DeskTextfield` can be used outside overview.
TEST_P(DeskBarTest, DeskTextfieldOutsideOverview) {
  auto widget =
      TestWidgetBuilder()
          .SetDelegate(nullptr)
          .SetBounds(gfx::Rect(0, 0, 300, 300))
          .SetParent(Shell::GetPrimaryRootWindow())
          .SetShow(true)
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .BuildOwnsNativeWidget();
  auto* desk_text_view = widget->SetContentsView(
      std::make_unique<DeskTextfield>(SystemTextfield::Type::kSmall));

  // There is no crash for committing name changes for `DeskTextfield` outside
  // overview.
  desk_text_view->CommitChanges(widget.get());
}

// Tests that shutting down when desk bar is alive does not crash without
// animation.
TEST_P(DeskBarTest, ShutdownNoAnimation) {
  OpenDeskBar();
}

// Tests that shutting down when desk bar is alive does not crash with
// animation.
TEST_P(DeskBarTest, ShutdownWithAnimation) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  OpenDeskBar();
}

struct DeskBarTestBasicCase {
  std::string test_name;

  // IDs of all desks.
  std::vector<int> desks;

  // ID of active desk.
  int active_desk;

  // Shelf alignment.
  ShelfAlignment shelf_alignment;

  // Indicates if there are any saved desks.
  bool has_saved_desks;

  // The expected bar widget bounds for desk button desk bar.
  gfx::Rect desk_button_bar_widget_bounds;

  // The expected bar view bounds for desk button desk bar.
  gfx::Rect desk_button_bar_view_bounds;

  // The expected bar widget bounds for overview desk bar.
  gfx::Rect overview_bar_widget_bounds;

  // The expected bar view bounds for overview desk bar.
  gfx::Rect overview_bar_view_bounds;
};

// Tests that desk button desk bar can show outside of overview with expected
// bounds for different shelf alignments.
TEST_P(DeskBarTest, Basic) {
  UpdateDisplay("800x600");

  const int expected_expanded_overview_height =
      features::IsForestFeatureEnabled() ? 114 : 98;

  const DeskBarTestBasicCase tests[] = {
      {.test_name = "single desk + bottom shelf + saved desks",
       .desks = {0},
       .active_desk = 0,
       .shelf_alignment = ShelfAlignment::kBottom,
       .has_saved_desks = true,
       .desk_button_bar_widget_bounds = {299, 446, 202, 98},
       .desk_button_bar_view_bounds = {0, 0, 202, 98},
       .overview_bar_widget_bounds = {0, 0, 800, 40},
       .overview_bar_view_bounds = {0, 0, 800, 40}},
      {.test_name = "single desk + bottom shelf",
       .desks = {0},
       .active_desk = 0,
       .shelf_alignment = ShelfAlignment::kBottom,
       .has_saved_desks = false,
       .desk_button_bar_widget_bounds = {323, 446, 154, 98},
       .desk_button_bar_view_bounds = {0, 0, 154, 98},
       .overview_bar_widget_bounds = {0, 0, 800, 40},
       .overview_bar_view_bounds = {0, 0, 800, 40}},
      {.test_name = "single desk + left shelf + saved desks",
       .desks = {0},
       .active_desk = 0,
       .shelf_alignment = ShelfAlignment::kLeft,
       .has_saved_desks = true,
       .desk_button_bar_widget_bounds = {56, 247, 202, 98},
       .desk_button_bar_view_bounds = {0, 0, 202, 98},
       .overview_bar_widget_bounds = {48, 0, 752, 40},
       .overview_bar_view_bounds = {0, 0, 752, 40}},
      {.test_name = "single desk + right shelf + saved desks",
       .desks = {0},
       .active_desk = 0,
       .shelf_alignment = ShelfAlignment::kRight,
       .has_saved_desks = true,
       .desk_button_bar_widget_bounds = {542, 247, 202, 98},
       .desk_button_bar_view_bounds = {0, 0, 202, 98},
       .overview_bar_widget_bounds = {0, 0, 752, 40},
       .overview_bar_view_bounds = {0, 0, 752, 40}},
      {.test_name = "multiple desks + bottom shelf + saved desks",
       .desks = {0, 1, 2},
       .active_desk = 0,
       .shelf_alignment = ShelfAlignment::kBottom,
       .has_saved_desks = true,
       .desk_button_bar_widget_bounds = {221, 446, 358, 98},
       .desk_button_bar_view_bounds = {0, 0, 358, 98},
       .overview_bar_widget_bounds = {0, 0, 800,
                                      expected_expanded_overview_height},
       .overview_bar_view_bounds = {0, 0, 800,
                                    expected_expanded_overview_height}},
  };

  auto* desks_controller = DesksController::Get();
  Shelf* shelf = GetPrimaryShelf();

  for (const auto& test : tests) {
    SCOPED_TRACE(test.test_name);

    // Set up desks.
    while (desks_controller->GetNumberOfDesks() < (int)test.desks.size()) {
      NewDesk();
    }
    if (!desks_controller->GetDeskAtIndex(test.active_desk)->is_active()) {
      ActivateDesk(desks_controller->GetDeskAtIndex(test.active_desk));
    }

    // Set up shelf.
    shelf->SetAlignment(test.shelf_alignment);

    // Set up saved desk.
    if (test.has_saved_desks) {
      AddSavedDeskEntry(desk_model(), base::Uuid::GenerateRandomV4(),
                        "saved_desk_1", base::Time::Now(),
                        DeskTemplateType::kSaveAndRecall);
    }

    // Create the desk bar then verify the bar and its child UI have expected
    // appearance.
    OpenDeskBar();
    auto* desk_bar_view = GetDeskBarView();
    ASSERT_TRUE(desk_bar_view);
    ASSERT_TRUE(desk_bar_view->GetVisible());
    auto* desk_bar_widget = desk_bar_view->GetWidget();
    ASSERT_TRUE(desk_bar_widget);
    if (bar_type_ == DeskBarViewBase::Type::kOverview) {
      EXPECT_THAT(desk_bar_widget->GetWindowBoundsInScreen(),
                  test.overview_bar_widget_bounds);
      EXPECT_THAT(desk_bar_view->bounds(), test.overview_bar_view_bounds);
      EXPECT_THAT(desk_bar_view->IsZeroState(), test.desks.size() == 1);
    } else {
      EXPECT_THAT(desk_bar_widget->GetWindowBoundsInScreen(),
                  test.desk_button_bar_widget_bounds);
      EXPECT_THAT(desk_bar_view->bounds(), test.desk_button_bar_view_bounds);
      EXPECT_FALSE(desk_bar_view->IsZeroState());
    }
    auto* new_desk_button = desk_bar_view->new_desk_button();
    const auto expected_button_state =
        bar_type_ == DeskBarViewBase::Type::kOverview && test.desks.size() == 1
            ? DeskIconButton::State::kZero
            : DeskIconButton::State::kExpanded;
    EXPECT_THAT(new_desk_button->state(), expected_button_state);
    EXPECT_TRUE(new_desk_button->GetVisible());
    EXPECT_THAT(new_desk_button->GetEnabled(),
                desks_controller->CanCreateDesks());
    auto* library_button = desk_bar_view->library_button();
    EXPECT_THAT(IsLazyInitViewVisible(library_button), test.has_saved_desks);
    if (library_button) {
      EXPECT_THAT(library_button->state(), expected_button_state);
      EXPECT_TRUE(library_button->GetEnabled());
    }

    CloseDeskBar();

    // Reset to clean state, i.e. only 1 desk and no saved desks.
    while (desks_controller->CanRemoveDesks()) {
      RemoveDesk(desks_controller->desks().back().get());
    }
    DeleteAllSavedDesks();
  }
}

// Tests that desk button desk bar shows with the correct bounds in secondary
// display.
TEST_P(DeskBarTest, BasicSecondaryDisplay) {
  UpdateDisplay("800x600,800x600");

  NewDesk();

  aura::Window* root = Shell::Get()->GetAllRootWindows().back();
  OpenDeskBar(root);

  auto* desk_bar_view = GetDeskBarView(root);
  ASSERT_TRUE(desk_bar_view);
  ASSERT_TRUE(desk_bar_view->GetVisible());
  auto* desk_bar_widget = desk_bar_view->GetWidget();
  ASSERT_TRUE(desk_bar_widget);

  const int expected_expanded_overview_height =
      features::IsForestFeatureEnabled() ? 114 : 98;

  if (bar_type_ == DeskBarViewBase::Type::kOverview) {
    EXPECT_THAT(desk_bar_widget->GetWindowBoundsInScreen(),
                gfx::Rect(800, 0, 800, expected_expanded_overview_height));
    EXPECT_THAT(desk_bar_view->bounds(),
                gfx::Rect(0, 0, 800, expected_expanded_overview_height));
    EXPECT_FALSE(desk_bar_view->IsZeroState());
  } else {
    EXPECT_THAT(desk_bar_widget->GetWindowBoundsInScreen(),
                gfx::Rect(1084, 446, 232, 98));
    EXPECT_THAT(desk_bar_view->bounds(), gfx::Rect(0, 0, 232, 98));
    EXPECT_FALSE(desk_bar_view->IsZeroState());
  }

  CloseDeskBar(root);
}

// Tests that desk button desk bar shows the scroll arrow buttons when overflow
// happens.
TEST_P(DeskBarTest, ScrollLayout) {
  UpdateDisplay("600x400");

  auto* desks_controller = DesksController::Get();
  while (desks_controller->CanCreateDesks()) {
    NewDesk();
  }

  OpenDeskBar();

  auto* left_scroll_button =
      DesksTestApi::GetDeskBarLeftScrollButton(bar_type_);
  ASSERT_TRUE(left_scroll_button);
  auto* right_scroll_button =
      DesksTestApi::GetDeskBarRightScrollButton(bar_type_);
  ASSERT_TRUE(right_scroll_button);
  EXPECT_FALSE(left_scroll_button->GetVisible());
  EXPECT_TRUE(right_scroll_button->GetVisible());

  while (right_scroll_button->GetVisible()) {
    ClickOrPressOnView(right_scroll_button);
    EXPECT_TRUE(left_scroll_button->GetVisible());
  }

  while (left_scroll_button->GetVisible()) {
    ClickOrPressOnView(left_scroll_button);
    EXPECT_TRUE(right_scroll_button->GetVisible());
  }

  CloseDeskBar();
}

TEST_P(DeskBarTest, HoverBasic) {
  auto window_1 = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto window_2 = CreateAppWindow(gfx::Rect(0, 0, 100, 100));

  NewDesk();

  auto* desks_controller = DesksController::Get();
  desks_controller->SendToDeskAtIndex(window_1.get(), 0);
  desks_controller->SendToDeskAtIndex(window_2.get(), 1);

  OpenDeskBar();

  for (int i = 0; i < desks_controller->GetNumberOfDesks(); i++) {
    auto* event_generator = GetEventGenerator();
    auto* mini_view = GetDeskBarView()->mini_views()[i].get();

    EXPECT_FALSE(DesksTestApi::IsDeskShortcutViewVisible(mini_view));
    event_generator->MoveMouseTo(
        mini_view->desk_preview()->GetBoundsInScreen().CenterPoint());

    if (features::IsSavedDeskUiRevampEnabled()) {
      EXPECT_TRUE(mini_view->desk_action_view()->context_menu_button());
      EXPECT_TRUE(
          mini_view->desk_action_view()->context_menu_button()->GetVisible());
    } else {
      EXPECT_TRUE(mini_view->desk_action_view()->combine_desks_button());
      EXPECT_TRUE(
          mini_view->desk_action_view()->combine_desks_button()->GetVisible());
    }

    EXPECT_TRUE(
        mini_view->desk_action_view()->close_all_button()->GetVisible());

    // The shortcut view only appears on the first 8 desks in the desk button
    // desk bar.
    const bool expected_visibility =
        i <= 7 && bar_type_ == DeskBarViewBase::Type::kDeskButton;
    EXPECT_EQ(DesksTestApi::IsDeskShortcutViewVisible(mini_view),
              expected_visibility);
  }

  CloseDeskBar();
}

// Tests that clicking on new desk button does the expected thing.
TEST_P(DeskBarTest, NewDeskButton) {
  OpenDeskBar();

  auto* desks_controller = DesksController::Get();
  auto* desk_bar_view = GetDeskBarView();
  auto* new_desk_button = desk_bar_view->new_desk_button();

  auto verify_disabled_new_desk_button = [&]() {
    EXPECT_THAT(desks_controller->GetNumberOfDesks(),
                desks_util::GetMaxNumberOfDesks());
    EXPECT_FALSE(desks_controller->CanCreateDesks());
    EXPECT_FALSE(new_desk_button->GetEnabled());
    EXPECT_THAT(new_desk_button->GetState(), views::Button::STATE_DISABLED);
    EXPECT_THAT(desk_bar_view->mini_views().size(),
                desks_controller->GetNumberOfDesks());
  };
  auto verify_enabled_new_desk_button = [&](bool zero_state_bar) {
    EXPECT_LE(desks_controller->GetNumberOfDesks(),
              (int)desks_util::GetMaxNumberOfDesks());
    EXPECT_TRUE(desks_controller->CanCreateDesks());
    EXPECT_THAT(desk_bar_view->IsZeroState(), zero_state_bar);
    EXPECT_TRUE(new_desk_button->GetEnabled());
    EXPECT_NE(new_desk_button->GetState(), views::Button::STATE_DISABLED);
    EXPECT_THAT(desk_bar_view->mini_views().size(),
                zero_state_bar ? 0 : desks_controller->GetNumberOfDesks());
  };

  // Create max number of desks.
  for (int i = 2; i <= (int)desks_util::GetMaxNumberOfDesks(); i++) {
    const bool zero_state_bar =
        bar_type_ == DeskBarViewBase::Type::kOverview && i == 2;
    verify_enabled_new_desk_button(zero_state_bar);
    ClickOrPressOnView(new_desk_button);

    EXPECT_TRUE(desks_controller->CanRemoveDesks());
    EXPECT_THAT(desks_controller->GetNumberOfDesks(), i);
  }

  // The new desk button should be disabled.
  verify_disabled_new_desk_button();

  // Clicking on the new desk button again does nothing now.
  ClickOrPressOnView(new_desk_button);
  verify_disabled_new_desk_button();

  CloseDeskBar();
}

// Tests that we can go to saved desk library directly via desk button desk bar.
TEST_P(DeskBarTest, LibraryButton) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Add a saved desk, so that the library button can show up.
  AddSavedDeskEntry(desk_model(), base::Uuid::GenerateRandomV4(),
                    "saved_desk_1", base::Time::Now(),
                    DeskTemplateType::kSaveAndRecall);

  auto test_library_button = [&](const std::string& trace_message) {
    SCOPED_TRACE(trace_message);

    OpenDeskBar();
    if (bar_type_ == DeskBarViewBase::Type::kOverview) {
      WaitForOverviewEnterAnimation();
    }

    EnterLibrary();
    auto* root = Shell::GetPrimaryRootWindow();
    if (bar_type_ == DeskBarViewBase::Type::kOverview) {
      // Clicking on library button in zero state overview desk bar would
      // trigger a bar bounds animation.
      DesksTestApi::WaitForDeskBarUiUpdate(
          GetDeskBarView(root, DeskBarViewBase::Type::kOverview));
    }
    const views::FocusRing* library_button_focus_ring =
        views::FocusRing::Get(GetPrimaryRootDesksBarView()->library_button());
    ASSERT_TRUE(library_button_focus_ring);
    ASSERT_TRUE(library_button_focus_ring->GetVisible());
    EXPECT_FALSE(library_button_focus_ring->GetVisibleBounds().IsEmpty());

    CloseDeskBar(root, DeskBarViewBase::Type::kOverview);
  };

  test_library_button("no app window");

  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  test_library_button("one app window");
}

// Tests that the desk button desk bar support dragging to reorder desks.
TEST_P(DeskBarTest, ReorderDesk) {
  OpenDeskBar();

  auto* desks_controller = DesksController::Get();
  auto* desk_bar_view = GetDeskBarView();
  auto* event_generator = GetEventGenerator();

  // Create two more desks.
  std::vector<std::string> desk_names = {"1", "2", "3"};
  for (auto i = 0; auto desk_name : desk_names) {
    if (desks_controller->GetNumberOfDesks() - 1 < i) {
      NewDesk();
    }
    desks_controller->GetDeskAtIndex(i)->SetName(base::UTF8ToUTF16(desk_name),
                                                 /*set_by_user=*/true);
    EXPECT_THAT(desk_bar_view->mini_views().size(),
                GetDeskBarView()->IsZeroState()
                    ? 0
                    : desks_controller->GetNumberOfDesks());
    i++;
  }
  views::test::RunScheduledLayout(desk_bar_view);
  desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();

  auto verify_desks = [&]() {
    for (int i = 0; i < desks_controller->GetNumberOfDesks(); i++) {
      EXPECT_THAT(desks_controller->GetDeskAtIndex(i)->name(),
                  base::UTF8ToUTF16(desk_names[i]));
    }
    auto* prefs =
        Shell::Get()->session_controller()->GetPrimaryUserPrefService();
    EXPECT_THAT(GetDeskRestoreNames(prefs),
                ::testing::ElementsAreArray(desk_names));
  };

  auto drag_desk = [&](int from, int to) {
    SCOPED_TRACE("Swap desk " + base::NumberToString(from) + " with desk " +
                 base::NumberToString(to));

    // Drag the desk from `from` to `to`.
    StartDragDeskPreview(desk_bar_view->mini_views()[from], event_generator);
    EXPECT_TRUE(desk_bar_view->IsDraggingDesk());
    event_generator->MoveMouseTo(desk_bar_view->mini_views()[to]
                                     ->GetPreviewBoundsInScreen()
                                     .CenterPoint());
    event_generator->ReleaseLeftButton();
    EXPECT_FALSE(desk_bar_view->IsDraggingDesk());

    // Update `desk_names` with the expected order.
    std::string dragged_desk = desk_names[from];
    if (from <= to) {
      for (int i = from + 1; i <= to; i++) {
        desk_names[i - 1] = desk_names[i];
      }
    } else {
      for (int i = from - 1; i >= to; i--) {
        desk_names[i + 1] = desk_names[i];
      }
    }
    desk_names[to] = dragged_desk;

    // Verify desks are with expected order.
    verify_desks();
  };

  verify_desks();
  drag_desk(/*from=*/0, /*to=*/1);
  drag_desk(/*from=*/1, /*to=*/2);
  drag_desk(/*from=*/0, /*to=*/1);
  drag_desk(/*from=*/2, /*to=*/0);

  CloseDeskBar();
}

// Tests that the desk button desk bar supports keyboard reordering desks.
TEST_P(DeskBarTest, KeyboardReorderDesk) {
  OpenDeskBar();
  auto* desks_controller = DesksController::Get();
  auto* desk_bar_view = GetDeskBarView();

  // Create two more desks and store uuids to verify swaps.
  NewDesk();
  NewDesk();

  std::vector<base::Uuid> desk_uuids;
  for (auto& desk : desks_controller->desks()) {
    desk_uuids.push_back(desk->uuid());
  }

  auto verify_desk_uuids = [&]() {
    for (int i = 0; i < desks_controller->GetNumberOfDesks(); i++) {
      EXPECT_THAT(desks_controller->GetDeskAtIndex(i)->uuid(), desk_uuids[i]);
    }
  };

  auto verify_shortcut_label = [&](auto* mini_view, int desk_index) {
    const bool expected_shortcut_visibility =
        bar_type_ == DeskBarViewBase::Type::kDeskButton;

    ASSERT_EQ(DesksTestApi::IsDeskShortcutViewVisible(mini_view),
              expected_shortcut_visibility);
    if (expected_shortcut_visibility) {
      views::Label* label = DesksTestApi::GetDeskShortcutLabel(mini_view);
      ASSERT_TRUE(label);
      EXPECT_EQ(base::NumberToString16(desk_index + 1), label->GetText());
    }
  };

  auto swap_desk = [&](int desk_index, bool left) {
    SCOPED_TRACE("Swap desk " + base::NumberToString(desk_index) + " to the " +
                 (left ? "left" : "right"));

    // Ensure swap is possible.
    ASSERT_FALSE(desk_index == 0 && left);
    ASSERT_FALSE(desk_index == 2 && !left);

    // Perform the swap.
    auto* mini_view = desk_bar_view->mini_views()[desk_index].get();
    mini_view->desk_preview()->RequestFocus();
    verify_shortcut_label(mini_view, desk_index);
    PressAndReleaseKey(left ? ui::VKEY_LEFT : ui::VKEY_RIGHT,
                       ui::EF_CONTROL_DOWN);

    // Verify the swap.
    const int new_index = desk_index + (left ? -1 : 1);
    EXPECT_EQ(mini_view, desk_bar_view->mini_views()[new_index]);
    verify_shortcut_label(mini_view, new_index);

    // Update `desk_uuids` with the expected order and verify desks.
    std::swap(desk_uuids[desk_index], desk_uuids[new_index]);
    verify_desk_uuids();
  };

  verify_desk_uuids();
  swap_desk(/*desk_index=*/0, /*left=*/false);
  swap_desk(/*desk_index=*/1, /*left=*/false);
  swap_desk(/*desk_index=*/0, /*left=*/false);
  swap_desk(/*desk_index=*/1, /*left=*/false);
  swap_desk(/*desk_index=*/1, /*left=*/true);
  swap_desk(/*desk_index=*/2, /*left=*/true);
  swap_desk(/*desk_index=*/1, /*left=*/true);
  swap_desk(/*desk_index=*/0, /*left=*/false);
  swap_desk(/*desk_index=*/2, /*left=*/true);
  swap_desk(/*desk_index=*/1, /*left=*/false);
  swap_desk(/*desk_index=*/1, /*left=*/true);

  CloseDeskBar();
}

TEST_P(DeskBarTest, ActivateDesk) {
  auto* desks_controller = DesksController::Get();

  NewDesk();
  EXPECT_THAT(desks_controller->GetActiveDeskIndex(), 0);

  // Clicking on active desk would *not* switch desk but just hide the bar.
  OpenDeskBar();
  ClickOrPressOnView(GetDeskBarView()->mini_views()[0]);
  EXPECT_THAT(desks_controller->GetActiveDeskIndex(), 0);
  EXPECT_FALSE(GetDeskBarView());

  // Clicking on non-active desk would switch desk and hide the bar.
  OpenDeskBar();
  DeskSwitchAnimationWaiter waiter;
  ClickOrPressOnView(GetDeskBarView()->mini_views()[1]);
  waiter.Wait();
  EXPECT_THAT(desks_controller->GetActiveDeskIndex(), 1);
  EXPECT_FALSE(GetDeskBarView());
}

TEST_P(DeskBarTest, CombineOrCloseDesk) {
  // Setup 3 desks, e.g. "Desk 1", "Desk 2", and "Desk 3". Only "Desk 3" has a
  // window.
  NewDesk();
  NewDesk();
  auto window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto* desks_controller = DesksController::Get();
  desks_controller->SendToDeskAtIndex(window.get(), 2);

  OpenDeskBar();

  for (int desk_index = desks_controller->GetNumberOfDesks() - 1;
       desk_index > 0; desk_index--) {
    CloseDeskFromMiniView(GetDeskBarView()->mini_views()[desk_index],
                          GetEventGenerator());
    EXPECT_THAT(GetDeskBarView()->mini_views().size(),
                GetDeskBarView()->IsZeroState() ? 0 : desk_index);
  }

  CloseDeskBar();
}

TEST_P(DeskBarTest, DeskRenameKeyEsc) {
  NewDesk();
  OpenDeskBar();

  auto* desk_bar_view = GetDeskBarView();
  Desk* desk = DesksController::Get()->GetDeskAtIndex(0);
  auto* mini_view = desk_bar_view->FindMiniViewForDesk(desk);
  auto* desk_name_view = mini_view->desk_name_view();

  ClickOrPressOnView(desk_name_view);
  ASSERT_TRUE(desk_name_view->HasFocus());

  // Undo by pressing esc on keyboard.
  PressAndReleaseKey(ui::VKEY_D, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_1);
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(desk_name_view->HasFocus());
  EXPECT_FALSE(desk->is_name_set_by_user());
  EXPECT_THAT(desk_name_view->GetText(), u"Desk 1");

  CloseDeskBar();
}

TEST_P(DeskBarTest, DeskRenameKeyReturn) {
  NewDesk();
  OpenDeskBar();

  auto* desk_bar_view = GetDeskBarView();
  Desk* desk = DesksController::Get()->GetDeskAtIndex(0);
  auto* mini_view = desk_bar_view->FindMiniViewForDesk(desk);
  auto* desk_name_view = mini_view->desk_name_view();

  ClickOrPressOnView(desk_name_view);
  ASSERT_TRUE(desk_name_view->HasFocus());

  // Commit by pressing return on keyboard.
  PressAndReleaseKey(ui::VKEY_D, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_1);
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_FALSE(desk_name_view->HasFocus());
  EXPECT_TRUE(desk->is_name_set_by_user());
  EXPECT_THAT(desk_name_view->GetText(), u"D1");

  CloseDeskBar();
}

TEST_P(DeskBarTest, DeskRenameKeyTab) {
  NewDesk();
  OpenDeskBar();

  auto* desk_bar_view = GetDeskBarView();
  Desk* desk = DesksController::Get()->desks().front().get();
  auto* mini_view = desk_bar_view->FindMiniViewForDesk(desk);
  auto* desk_name_view = mini_view->desk_name_view();

  ClickOrPressOnView(desk_name_view);
  ASSERT_TRUE(desk_name_view->HasFocus());

  // Commit by pressing tab on keyboard.
  PressAndReleaseKey(ui::VKEY_D, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_1);
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_FALSE(desk_name_view->HasFocus());
  EXPECT_TRUE(desk->is_name_set_by_user());
  EXPECT_THAT(desk_name_view->GetText(), u"D1");
  ASSERT_TRUE(desk_bar_view->mini_views()[1]->desk_preview()->HasFocus());

  CloseDeskBar();
}

TEST_P(DeskBarTest, DeskRenameKeyShiftTab) {
  NewDesk();
  OpenDeskBar();

  auto* desk_bar_view = GetDeskBarView();
  Desk* desk = DesksController::Get()->desks().front().get();
  auto* mini_view = desk_bar_view->FindMiniViewForDesk(desk);
  auto* desk_name_view = mini_view->desk_name_view();

  ClickOrPressOnView(desk_name_view);
  ASSERT_TRUE(desk_name_view->HasFocus());

  // Commit by pressing shift+tab on keyboard.
  PressAndReleaseKey(ui::VKEY_D, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_1);
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(desk_name_view->HasFocus());
  EXPECT_TRUE(desk->is_name_set_by_user());
  EXPECT_THAT(desk_name_view->GetText(), u"D1");
  ASSERT_TRUE(mini_view->desk_action_view()->close_all_button()->HasFocus());

  CloseDeskBar();
}

TEST_P(DeskBarTest, DeskRenameClickOnBar) {
  NewDesk();
  OpenDeskBar();

  auto* desk_bar_view = GetDeskBarView();
  Desk* desk = DesksController::Get()->GetDeskAtIndex(0);
  auto* mini_view = desk_bar_view->FindMiniViewForDesk(desk);
  auto* desk_name_view = mini_view->desk_name_view();

  ClickOrPressOnView(desk_name_view);
  ASSERT_TRUE(desk_name_view->HasFocus());

  // Commit by clicking or pressing empty area on the bar.
  PressAndReleaseKey(ui::VKEY_D, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_1);
  gfx::Point empty_point = desk_bar_view->GetBoundsInScreen().top_center();
  empty_point.Offset(0, 2);
  ClickOrPressOnPoint(empty_point);

  EXPECT_FALSE(desk_name_view->HasFocus());
  EXPECT_TRUE(desk->is_name_set_by_user());
  EXPECT_THAT(desk_name_view->GetText(), u"D1");

  CloseDeskBar();
}

TEST_P(DeskBarTest, DeskRenameClickOffBar) {
  NewDesk();
  OpenDeskBar();

  auto* desk_bar_view = GetDeskBarView();
  Desk* desk = DesksController::Get()->GetDeskAtIndex(0);
  auto* mini_view = desk_bar_view->FindMiniViewForDesk(desk);
  auto* desk_name_view = mini_view->desk_name_view();

  ClickOrPressOnView(desk_name_view);
  ASSERT_TRUE(desk_name_view->HasFocus());
  PressAndReleaseKey(ui::VKEY_D, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_1);

  // Commit by clicking or pressing empty area off the bar.
  gfx::Point empty_point = desk_bar_view->GetBoundsInScreen().bottom_center();
  empty_point.Offset(0, 2);
  ClickOrPressOnPoint(empty_point);
  EXPECT_FALSE(desk_name_view->HasFocus());
  EXPECT_TRUE(desk->is_name_set_by_user());
  EXPECT_THAT(desk_name_view->GetText(), u"D1");
  EXPECT_TRUE(GetDeskBarView());

  CloseDeskBar();
}

TEST_P(DeskBarTest, AutoHideClickOrPress) {
  OpenDeskBar();
  auto* desk_bar_view = GetDeskBarView();

  // Clicking on wallpaper hides the bar.
  gfx::Point empty_point = desk_bar_view->GetBoundsInScreen().bottom_center();
  empty_point.Offset(0, 2);
  ClickOrPressOnPoint(empty_point);
  EXPECT_FALSE(GetDeskBarView());
}

TEST_P(DeskBarTest, AutoHideKeyEsc) {
  OpenDeskBar();
  ASSERT_TRUE(GetDeskBarView());

  // Pressing `Esc` hides the bar.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(GetDeskBarView());
}

TEST_P(DeskBarTest, AutoHideKeyBack) {
  OpenDeskBar();
  ASSERT_TRUE(GetDeskBarView());

  // Pressing `Back` hides the bar.
  PressAndReleaseKey(ui::VKEY_BROWSER_BACK);
  EXPECT_FALSE(GetDeskBarView());
}

TEST_P(DeskBarTest, ForwardTabbing) {
  // Add a saved desk, so that the library button can show up.
  AddSavedDeskEntry(desk_model(), base::Uuid::GenerateRandomV4(),
                    "saved_desk_1", base::Time::Now(),
                    DeskTemplateType::kSaveAndRecall);

  // Create `max - 1` number of desks.
  for (size_t i = 2; i <= desks_util::GetMaxNumberOfDesks() - 1; i++) {
    NewDesk();
  }
  auto* desk_controller = DesksController::Get();
  ASSERT_EQ(desk_controller->GetNumberOfDesks(),
            static_cast<int>(desks_util::GetMaxNumberOfDesks()) - 1);
  const auto& window = CreateAppWindow();
  desk_controller->SendToDeskAtIndex(window.get(), 0);

  OpenDeskBar();
  auto* desk_bar_view = GetDeskBarView();
  ASSERT_TRUE(desk_bar_view);

  // Tab through the first window if we are in overview.
  if (bar_type_ == DeskBarViewBase::Type::kOverview) {
    PressAndReleaseKey(ui::VKEY_TAB);
  }

  // Tab through all desks.
  for (int i = 0; i < desk_controller->GetNumberOfDesks(); i++) {
    auto* mini_view = desk_bar_view->mini_views()[i].get();

    PressAndReleaseKey(ui::VKEY_TAB);
    ASSERT_TRUE(mini_view->desk_preview()->HasFocus());
    // The shortcut view only appears on the first 8 desks in the desk button
    // desk bar.
    const bool expected_visibility =
        i <= 7 && bar_type_ == DeskBarViewBase::Type::kDeskButton;
    EXPECT_EQ(DesksTestApi::IsDeskShortcutViewVisible(mini_view),
              expected_visibility);

    if (use_desk_profiles_) {
      PressAndReleaseKey(ui::VKEY_TAB);
      ASSERT_TRUE(mini_view->desk_profiles_button()->HasFocus());
    }

    if (i == 0) {
      PressAndReleaseKey(ui::VKEY_TAB);

      if (features::IsSavedDeskUiRevampEnabled()) {
        ASSERT_TRUE(mini_view->desk_action_view()->context_menu_button());
        ASSERT_TRUE(
            mini_view->desk_action_view()->context_menu_button()->HasFocus());
      } else {
        ASSERT_TRUE(mini_view->desk_action_view()->combine_desks_button());
        ASSERT_TRUE(
            mini_view->desk_action_view()->combine_desks_button()->HasFocus());
      }
    }

    PressAndReleaseKey(ui::VKEY_TAB);
    ASSERT_TRUE(mini_view->desk_action_view()->close_all_button()->HasFocus());
    // The shortcut view only appears on the first 8 desks in the desk button
    // desk bar.
    EXPECT_EQ(DesksTestApi::IsDeskShortcutViewVisible(mini_view),
              expected_visibility);

    PressAndReleaseKey(ui::VKEY_TAB);
    ASSERT_TRUE(mini_view->desk_name_view()->HasFocus());
    EXPECT_FALSE(DesksTestApi::IsDeskShortcutViewVisible(
        desk_bar_view->mini_views()[i]));
  }

  // Tab through new desk button.
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_TRUE(desk_bar_view->new_desk_button()->HasFocus());

  // Tab through library button.
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_TRUE(desk_bar_view->library_button());
  ASSERT_TRUE(desk_bar_view->library_button()->HasFocus());

  CloseDeskBar();
}

TEST_P(DeskBarTest, ReverseTabbing) {
  // Add a saved desk, so that the library button can show up.
  AddSavedDeskEntry(desk_model(), base::Uuid::GenerateRandomV4(),
                    "saved_desk_1", base::Time::Now(),
                    DeskTemplateType::kSaveAndRecall);

  // Create `max - 1` number of desks.
  for (size_t i = 2; i <= desks_util::GetMaxNumberOfDesks() - 1; i++) {
    NewDesk();
  }
  auto* desk_controller = DesksController::Get();
  ASSERT_EQ(desk_controller->GetNumberOfDesks(),
            static_cast<int>(desks_util::GetMaxNumberOfDesks()) - 1);
  const auto& window = CreateAppWindow();
  desk_controller->SendToDeskAtIndex(window.get(), 0);

  OpenDeskBar();
  auto* desk_bar_view = GetDeskBarView();
  ASSERT_TRUE(desk_bar_view);

  // If it is the bento button desk bar, tab one more time since it starts at
  // the active desk.
  if (bar_type_ == DeskBarViewBase::Type::kDeskButton) {
    PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  }

  // Tab through library button.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ASSERT_TRUE(desk_bar_view->library_button());
  ASSERT_TRUE(desk_bar_view->library_button()->HasFocus());

  // Tab through new desk button.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ASSERT_TRUE(desk_bar_view->new_desk_button()->HasFocus());

  // Tab through all desks.
  for (int i = desk_controller->GetNumberOfDesks() - 1; i >= 0; i--) {
    auto* mini_view = desk_bar_view->mini_views()[i].get();

    PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
    ASSERT_TRUE(mini_view->desk_name_view()->HasFocus());
    EXPECT_FALSE(DesksTestApi::IsDeskShortcutViewVisible(
        desk_bar_view->mini_views()[i]));

    PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
    ASSERT_TRUE(mini_view->desk_action_view()->close_all_button()->HasFocus());

    if (i == 0) {
      PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);

      if (features::IsSavedDeskUiRevampEnabled()) {
        ASSERT_TRUE(mini_view->desk_action_view()->context_menu_button());
        ASSERT_TRUE(
            mini_view->desk_action_view()->context_menu_button()->HasFocus());
      } else {
        ASSERT_TRUE(mini_view->desk_action_view()->combine_desks_button());
        ASSERT_TRUE(
            mini_view->desk_action_view()->combine_desks_button()->HasFocus());
      }
    }

    if (use_desk_profiles_) {
      PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
      ASSERT_TRUE(mini_view->desk_profiles_button()->HasFocus());
    }

    // The shortcut view only appears on the first 8 desks in the desk button
    // desk bar.
    const bool expected_visibility =
        i <= 7 && bar_type_ == DeskBarViewBase::Type::kDeskButton;
    EXPECT_EQ(DesksTestApi::IsDeskShortcutViewVisible(mini_view),
              expected_visibility);
    PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
    ASSERT_TRUE(mini_view->desk_preview()->HasFocus());
    EXPECT_EQ(DesksTestApi::IsDeskShortcutViewVisible(mini_view),
              expected_visibility);
  }

  CloseDeskBar();
}

TEST_P(DeskBarTest, CloseActiveDesk) {
  // Create one app window and two more desks.
  WindowHolder window_holder(CreateAppWindow());
  NewDesk();
  NewDesk();

  // Move window to desk #1 and active the desk.
  auto* desks_controller = DesksController::Get();
  desks_controller->desks()[0]->MoveWindowToDesk(
      window_holder.window(), desks_controller->desks()[1].get(),
      window_holder.window()->GetRootWindow(), /*unminimize=*/true);
  ActivateDesk(desks_controller->desks()[1].get());

  OpenDeskBar();
  auto* desk_bar_view = GetDeskBarView();
  ASSERT_TRUE(desk_bar_view);

  // Focus desk #1.
  ASSERT_TRUE(TabUntil(
      /*reverse=*/false,
      ViewFocused(desk_bar_view->mini_views()[1]->desk_preview()),
      GetEventGenerator()));

  // Close active desk.
  if (bar_type_ == DeskBarViewBase::Type::kOverview) {
    // Closing the active desk in overview should remain in the current overview
    // session.
    PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
    EXPECT_TRUE(GetDeskBarView());
    CloseDeskBar();
  } else {
    // Closing the active desk in desk button desk bar should trigger a desk
    // switch and hide the bar.
    DeskSwitchAnimationWaiter waiter;
    PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
    waiter.Wait();
    EXPECT_FALSE(GetDeskBarView());
  }
  EXPECT_THAT(desks_controller->GetActiveDeskIndex(), 0);
  EXPECT_THAT(desks_controller->desks().size(), 2);
  SimulateWaitForCloseAll();
  EXPECT_FALSE(window_holder.is_valid());
}

TEST_P(DeskBarTest, MergeActiveDesk) {
  // Create one app window and two more desks.
  WindowHolder window_holder(CreateAppWindow());
  NewDesk();
  NewDesk();

  // Move window to desk #1 and active the desk.
  auto* desks_controller = DesksController::Get();
  desks_controller->desks()[0]->MoveWindowToDesk(
      window_holder.window(), desks_controller->desks()[1].get(),
      window_holder.window()->GetRootWindow(), /*unminimize=*/true);
  ActivateDesk(desks_controller->desks()[1].get());

  OpenDeskBar();
  auto* desk_bar_view = GetDeskBarView();
  ASSERT_TRUE(desk_bar_view);

  // Focus desk #1
  ASSERT_TRUE(TabUntil(
      /*reverse=*/false,
      ViewFocused(desk_bar_view->mini_views()[1]->desk_preview()),
      GetEventGenerator()));

  // Merge active desk.
  if (bar_type_ == DeskBarViewBase::Type::kOverview) {
    // Merging the active desk in overview should remain in the current overview
    // session.
    PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
    EXPECT_TRUE(GetDeskBarView());
    CloseDeskBar();
  } else {
    // Merging the active desk in desk button desk bar should trigger a desk
    // switch and hide the bar.
    DeskSwitchAnimationWaiter waiter;
    PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
    waiter.Wait();
    EXPECT_FALSE(GetDeskBarView());
  }
  EXPECT_THAT(desks_controller->GetActiveDeskIndex(), 0);
  EXPECT_THAT(desks_controller->desks().size(), 2);
  EXPECT_TRUE(window_holder.is_valid());
  EXPECT_TRUE(desks_controller->desks()[0]
                  ->GetDeskContainerForRoot(Shell::GetPrimaryRootWindow())
                  ->Contains(window_holder.window()));
}

TEST_P(DeskBarTest, CloseNonActiveDesk) {
  // Create one app window and two more desks.
  WindowHolder window_holder(CreateAppWindow());
  NewDesk();
  NewDesk();

  // Move window to desk #1.
  auto* desks_controller = DesksController::Get();
  desks_controller->desks()[0]->MoveWindowToDesk(
      window_holder.window(), desks_controller->desks()[1].get(),
      window_holder.window()->GetRootWindow(), /*unminimize=*/true);

  OpenDeskBar();
  auto* desk_bar_view = GetDeskBarView();
  ASSERT_TRUE(desk_bar_view);

  // Highlight desk #1.
  ASSERT_TRUE(TabUntil(
      /*reverse=*/false,
      ViewFocused(desk_bar_view->mini_views()[1]->desk_preview()),
      GetEventGenerator()));

  // Close non-active desk.
  PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(GetDeskBarView());
  EXPECT_THAT(desks_controller->GetActiveDeskIndex(), 0);
  EXPECT_THAT(desks_controller->desks().size(), 2);
  SimulateWaitForCloseAll();
  EXPECT_FALSE(window_holder.is_valid());
  CloseDeskBar();
}

TEST_P(DeskBarTest, MergeNonActiveDesk) {
  // Create one app window and two more desks.
  WindowHolder window_holder(CreateAppWindow());
  NewDesk();
  NewDesk();

  // Move window to desk #1.
  auto* desks_controller = DesksController::Get();
  desks_controller->desks()[0]->MoveWindowToDesk(
      window_holder.window(), desks_controller->desks()[1].get(),
      window_holder.window()->GetRootWindow(), /*unminimize=*/true);

  OpenDeskBar();
  auto* desk_bar_view = GetDeskBarView();
  ASSERT_TRUE(desk_bar_view);

  // Highlight desk #1.
  ASSERT_TRUE(TabUntil(
      /*reverse=*/false,
      ViewFocused(desk_bar_view->mini_views()[1]->desk_preview()),
      GetEventGenerator()));

  // Close non-active desk.
  PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(GetDeskBarView());
  EXPECT_THAT(desks_controller->GetActiveDeskIndex(), 0);
  EXPECT_THAT(desks_controller->desks().size(), 2);
  EXPECT_TRUE(window_holder.is_valid());
  EXPECT_TRUE(desks_controller->desks()[0]
                  ->GetDeskContainerForRoot(Shell::GetPrimaryRootWindow())
                  ->Contains(window_holder.window()));

  CloseDeskBar();
}

// Tests that the actions in the desk button and overview desk bars are being
// correctly recorded in their histograms.
TEST_P(DeskBarTest, DeskBarActionMetrics) {
  NewDesk();

  // Add a saved desk so that the library button can show up.
  AddSavedDeskEntry(desk_model(), base::Uuid::GenerateRandomV4(),
                    "saved_desk_1", base::Time::Now(),
                    DeskTemplateType::kSaveAndRecall);

  base::HistogramTester histogram_tester;

  // Windows can only be added to desks when the desk bar is closed. Otherwise
  // the combine desks button will not show.
  WindowHolder window(CreateAppWindow());
  auto* desks_controller = DesksController::Get();
  desks_controller->SendToDeskAtIndex(window.window(), 1);

  OpenDeskBar();
  auto* desk_bar_view = GetDeskBarView();

  // Add new desk.
  ClickOrPressOnView(desk_bar_view->new_desk_button());
  views::test::RunScheduledLayout(desk_bar_view);
  histogram_tester.ExpectTotalCount(
      bar_type_ == DeskBarViewBase::Type::kDeskButton
          ? kDeskButtonDeskBarNewDeskHistogramName
          : kOverviewDeskBarNewDeskHistogramName,
      1);

  // Set desk name.
  PressAndReleaseKey(ui::VKEY_A, ui::EF_NONE);
  PressAndReleaseKey(ui::VKEY_B, ui::EF_NONE);
  PressAndReleaseKey(ui::VKEY_C, ui::EF_NONE);
  PressAndReleaseKey(ui::VKEY_RETURN, ui::EF_NONE);
  views::test::RunScheduledLayout(desk_bar_view);
  histogram_tester.ExpectTotalCount(
      bar_type_ == DeskBarViewBase::Type::kDeskButton
          ? kDeskButtonDeskBarRenameDeskHistogramName
          : kOverviewDeskBarRenameDeskHistogramName,
      1);

  // Activate the third desk.
  DeskSwitchAnimationWaiter waiter;
  ClickOrPressOnView(desk_bar_view->mini_views()[2]->desk_preview());
  waiter.Wait();
  histogram_tester.ExpectTotalCount(
      bar_type_ == DeskBarViewBase::Type::kDeskButton
          ? kDeskButtonDeskBarActivateDeskHistogramName
          : kOverviewDeskBarActivateDeskHistogramName,
      1);

  // Activating the desk should take us out of our desk bar, so we need a new
  // one.
  OpenDeskBar();
  desk_bar_view = GetDeskBarView();

  // Drag desk to reorder.
  const auto& mini_views = desk_bar_view->mini_views();
  auto* event_generator = GetEventGenerator();
  StartDragDeskPreview(mini_views[0], event_generator);
  EXPECT_TRUE(desk_bar_view->IsDraggingDesk());
  event_generator->MoveMouseTo(
      mini_views[2]->GetPreviewBoundsInScreen().CenterPoint());
  event_generator->ReleaseLeftButton();
  views::test::RunScheduledLayout(desk_bar_view);
  EXPECT_FALSE(desk_bar_view->IsDraggingDesk());
  histogram_tester.ExpectTotalCount(
      bar_type_ == DeskBarViewBase::Type::kDeskButton
          ? kDeskButtonDeskBarReorderDeskHistogramName
          : kOverviewDeskBarReorderDeskHistogramName,
      1);

  // Combine desks.
  auto* root_window = Shell::Get()->GetPrimaryRootWindow();
  if (features::IsSavedDeskUiRevampEnabled()) {
    CombineDeskWithMenu(/*index=*/0, root_window, bar_type_);
  } else {
    CloseDeskWithButton(/*index=*/0, /*close_all=*/false, root_window,
                        bar_type_);
  }

  histogram_tester.ExpectTotalCount(
      bar_type_ == DeskBarViewBase::Type::kDeskButton
          ? kDeskButtonDeskBarCombineDesksHistogramName
          : kOverviewDeskBarCombineDesksHistogramName,
      1);

  // Close desk with windows.
  CloseDeskWithButton(/*index=*/1, /*close_all=*/true, root_window, bar_type_);
  histogram_tester.ExpectTotalCount(
      bar_type_ == DeskBarViewBase::Type::kDeskButton
          ? kDeskButtonDeskBarCloseDeskHistogramName
          : kOverviewDeskBarCloseDeskHistogramName,
      1);

  // Open library.
  EnterLibrary();
  histogram_tester.ExpectTotalCount(
      bar_type_ == DeskBarViewBase::Type::kDeskButton
          ? kDeskButtonDeskBarOpenLibraryHistogramName
          : kOverviewDeskBarOpenLibraryHistogramName,
      1);
}

// Tests that metrics are correctly separated for desk switches between the
// overview desk bar and the desk button desk bar.
TEST_P(DeskBarTest, DeskBarSwitchMetrics) {
  NewDesk();
  NewDesk();

  base::HistogramTester histogram_tester;

  // Create an app window on the first desk.
  WindowHolder window(CreateAppWindow());
  auto* desks_controller = DesksController::Get();
  desks_controller->SendToDeskAtIndex(window.window(), 0);

  OpenDeskBar();

  DeskSwitchAnimationWaiter waiter;
  ClickOrPressOnView(GetDeskBarView()->mini_views()[1]->desk_preview());
  waiter.Wait();
  ASSERT_TRUE(desks_controller->desks()[1]->is_active());
  histogram_tester.ExpectBucketCount(
      kDeskSwitchHistogramName,
      bar_type_ == DeskBarViewBase::Type::kDeskButton
          ? DesksSwitchSource::kDeskButtonMiniViewButton
          : DesksSwitchSource::kMiniViewButton,
      1);

  OpenDeskBar();

  auto* root_window = Shell::Get()->GetPrimaryRootWindow();
  CloseDeskWithButton(/*index=*/1, /*close_all=*/true, root_window, bar_type_);
  histogram_tester.ExpectBucketCount(
      kDeskSwitchHistogramName,
      bar_type_ == DeskBarViewBase::Type::kDeskButton
          ? DesksSwitchSource::kDeskButtonDeskRemoved
          : DesksSwitchSource::kDeskRemoved,
      1);

  OpenDeskBar();

  // Combine desks.
  if (features::IsSavedDeskUiRevampEnabled()) {
    CombineDeskWithMenu(/*index=*/0, root_window, bar_type_);
  } else {
    CloseDeskWithButton(/*index=*/0, /*close_all=*/false, root_window,
                        bar_type_);
  }

  histogram_tester.ExpectBucketCount(
      kDeskSwitchHistogramName,
      bar_type_ == DeskBarViewBase::Type::kDeskButton
          ? DesksSwitchSource::kDeskButtonDeskRemoved
          : DesksSwitchSource::kDeskRemoved,
      2);
}

// Tests that metrics are correctly separated for desk creation and removal
// between the overview desk bar and the desk button desk bar.
TEST_P(DeskBarTest, DeskCreationRemovalMetrics) {
  NewDesk();

  base::HistogramTester histogram_tester;

  WindowHolder window(CreateAppWindow());
  auto* desks_controller = DesksController::Get();
  desks_controller->SendToDeskAtIndex(window.window(), 0);

  OpenDeskBar();
  auto* desk_bar = GetDeskBarView();

  ClickOrPressOnView(desk_bar->new_desk_button());
  histogram_tester.ExpectBucketCount(
      kNewDeskHistogramName,
      bar_type_ == DeskBarViewBase::Type::kDeskButton
          ? DesksCreationRemovalSource::kDeskButtonDeskBarButton
          : DesksCreationRemovalSource::kButton,
      1);

  auto* root_window = Shell::Get()->GetPrimaryRootWindow();
  CloseDeskWithButton(/*index=*/1, /*close_all=*/true, root_window, bar_type_);
  histogram_tester.ExpectBucketCount(
      kRemoveDeskHistogramName,
      bar_type_ == DeskBarViewBase::Type::kDeskButton
          ? DesksCreationRemovalSource::kDeskButtonDeskBarButton
          : DesksCreationRemovalSource::kButton,
      1);

  // Long press doesn't register if there have been touch events on the desk bar
  // prior to trying to long press. Close and reopen the desk bar.
  if (features::IsSavedDeskUiRevampEnabled()) {
    CloseDeskBar();
  }
  OpenDeskBar();

  // Combine desks.
  if (features::IsSavedDeskUiRevampEnabled()) {
    CombineDeskWithMenu(/*index=*/0, root_window, bar_type_);
  } else {
    CloseDeskWithButton(/*index=*/0, /*close_all=*/false, root_window,
                        bar_type_);
  }

  histogram_tester.ExpectBucketCount(
      kRemoveDeskHistogramName,
      bar_type_ == DeskBarViewBase::Type::kDeskButton
          ? DesksCreationRemovalSource::kDeskButtonDeskBarButton
          : DesksCreationRemovalSource::kButton,
      2);
}

// Tests that metrics are correctly separated for open desk context menu
// between the overview desk bar and the desk button desk bar.
TEST_P(DeskBarTest, DeskOpenContextMenuMetrics) {
  NewDesk();

  base::HistogramTester histogram_tester;

  WindowHolder window(CreateAppWindow());
  auto* desks_controller = DesksController::Get();
  desks_controller->SendToDeskAtIndex(window.window(), 0);

  OpenDeskBar();
  auto* desk_bar = GetDeskBarView();
  RightClickOn(desk_bar->mini_views()[0]);

  EnterOverview();
  auto* overview_desks_bar =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())->desks_bar_view();
  RightClickOn(overview_desks_bar->mini_views()[0]);
  histogram_tester.ExpectTotalCount(
      bar_type_ == DeskBarViewBase::Type::kDeskButton
          ? kDeskButtonDeskBarOpenContextMenuHistogramName
          : kOverviewDeskBarOpenContextMenuHistogramName,
      1);
}

// Tests that setting to bottom locked shelf should not crash. Please refer to
// b/293625099.
TEST_P(DeskBarTest, BottomLockedShelf) {
  OpenDeskBar();
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottomLocked);
}

// Tests that we can undo close-all solely via keyboard navigation (tabbing to
// the undo toast and pressing enter).
TEST_P(DeskBarTest, CanUndoDeskClosureThroughKeyboardNavigation) {
  // Scenarios in which we can try to undo desk closure. If the active desk is
  // removed, we close the desk bar and immediately focus the undo button.
  enum class DeskRemovalMethod {
    kInactiveDeskRemovedForwardTab,
    kInactiveDeskRemovedReverseTab,
    kActiveDeskRemoved,
  };

  struct {
    const std::string scope_trace;
    const DeskRemovalMethod desk_removal_method;
  } kTestCases[] = {
      {"Forward tabbing to the undo button after removing an inactive desk",
       DeskRemovalMethod::kInactiveDeskRemovedForwardTab},
      {"Reverse tabbing to the undo button after removing an inactive desk",
       DeskRemovalMethod::kInactiveDeskRemovedReverseTab},
      {"Activating the undo button after removing the active desk",
       DeskRemovalMethod::kActiveDeskRemoved},
  };

  NewDesk();
  NewDesk();
  Shell::Get()->accessibility_controller()->spoken_feedback().SetEnabled(true);

  OpenDeskBar();
  auto* desk_bar = GetDeskBarView();
  const auto& mini_views = desk_bar->mini_views();

  DesksController* desks_controller = DesksController::Get();

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.scope_trace);

    // Tab to the first mini view.
    PressAndReleaseKey(ui::VKEY_TAB);

    // If we are not closing the active desk, close the next desk instead.
    if (test_case.desk_removal_method !=
        DeskRemovalMethod::kActiveDeskRemoved) {
      PressAndReleaseKey(ui::VKEY_TAB);
      PressAndReleaseKey(ui::VKEY_TAB);

      // If desk profiles are enabled then we need to tab past the profile
      // avatar button.
      if (use_desk_profiles_) {
        PressAndReleaseKey(ui::VKEY_TAB);
      }

      PressAndReleaseKey(ui::VKEY_TAB);
      ASSERT_TRUE(mini_views[1]->desk_preview()->HasFocus());

      PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
    } else if (bar_type_ == DeskBarViewBase::Type::kDeskButton) {
      ASSERT_EQ(mini_views[0]->desk_preview(),
                desk_bar->GetWidget()->GetFocusManager()->GetFocusedView());

      // In the desk button desk bar, we run the desk switch animation when
      // removing the active desk.
      DeskSwitchAnimationWaiter waiter;
      PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
      waiter.Wait();
    } else {
      ASSERT_EQ(DeskBarViewBase::Type::kOverview, bar_type_);
      ASSERT_TRUE(mini_views[0]->desk_preview()->HasFocus());
      PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
    }

    ASSERT_EQ(2u, desks_controller->desks().size());

    ASSERT_TRUE(TabUntil(
        /*reverse=*/
        test_case.desk_removal_method ==
            DeskRemovalMethod::kInactiveDeskRemovedReverseTab,
        [desks_controller] { return desks_controller->IsUndoToastFocused(); },
        GetEventGenerator()));

    // Pressing undo in the desk button desk bar after closing the active desk
    // will switch us back to the removed desk.
    if (test_case.desk_removal_method ==
            DeskRemovalMethod::kActiveDeskRemoved &&
        bar_type_ == DeskBarViewBase::Type::kDeskButton) {
      DeskSwitchAnimationWaiter waiter;
      PressAndReleaseKey(ui::VKEY_RETURN);
      waiter.Wait();
    } else {
      PressAndReleaseKey(ui::VKEY_RETURN);
    }

    ASSERT_EQ(3u, desks_controller->desks().size());
  }
}

TEST_P(DeskBarTest, DeskProfilesUsageMetrics) {
  base::HistogramTester histogram_tester;

  OpenDeskBar();
  // There's only one desk and regardless of whether the flag feature is enabled
  // or not, conditions are not met.
  histogram_tester.ExpectBucketCount(kDeskProfilesUsageStatusHistogramName,
                                     DeskProfilesUsageStatus::kConditionsNotMet,
                                     1);
  CloseDeskBar();

  NewDesk();
  OpenDeskBar();
  // Now there are two desks and we expect metrics depending on whether the
  // feature is enabled or not.
  if (use_desk_profiles_) {
    histogram_tester.ExpectBucketCount(kDeskProfilesUsageStatusHistogramName,
                                       DeskProfilesUsageStatus::kConditionsMet,
                                       1);
  } else {
    histogram_tester.ExpectBucketCount(
        kDeskProfilesUsageStatusHistogramName,
        DeskProfilesUsageStatus::kConditionsNotMet, 2);
  }
  CloseDeskBar();

  // Explicitly assign a profile to one of the desks.
  if (use_desk_profiles_) {
    DesksController::Get()->GetDeskAtIndex(0)->SetLacrosProfileId(
        GetDummyLacrosDeskProfileId(0), /*source=*/std::nullopt);

    OpenDeskBar();
    histogram_tester.ExpectBucketCount(kDeskProfilesUsageStatusHistogramName,
                                       DeskProfilesUsageStatus::kEnabled, 1);
    CloseDeskBar();
  }
}

TEST_P(DeskBarTest, DeskActionButtonTooltipForNewDesk) {
  OpenDeskBar();

  // Click the new desk button and verify the desk action buttons' tooltip.
  auto* desk_bar_view = GetDeskBarView();
  ClickOrPressOnView(desk_bar_view->new_desk_button());
  views::test::RunScheduledLayout(desk_bar_view);
  auto* desk_action_view = desk_bar_view->mini_views()[1]->desk_action_view();

  if (features::IsSavedDeskUiRevampEnabled()) {
    EXPECT_TRUE(desk_action_view->context_menu_button());
    EXPECT_THAT(desk_action_view->context_menu_button()->GetTooltipText(),
                u"Open context menu");
  } else {
    EXPECT_TRUE(desk_action_view->combine_desks_button());
    EXPECT_THAT(desk_action_view->combine_desks_button()->GetTooltipText(),
                u"Combine with Desk 1");
  }

  EXPECT_THAT(desk_action_view->close_all_button()->GetTooltipText(),
              u"Close Desk 2 and windows");

  // Rename desk 2 to `D2` and verify the desk action buttons' tooltip.
  PressAndReleaseKey(ui::VKEY_D, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_2);
  PressAndReleaseKey(ui::VKEY_RETURN);
  views::test::RunScheduledLayout(desk_bar_view);

  if (features::IsSavedDeskUiRevampEnabled()) {
    EXPECT_TRUE(desk_action_view->context_menu_button());
    EXPECT_THAT(desk_action_view->context_menu_button()->GetTooltipText(),
                u"Open context menu");
  } else {
    EXPECT_TRUE(desk_action_view->combine_desks_button());
    EXPECT_THAT(desk_action_view->combine_desks_button()->GetTooltipText(),
                u"Combine with Desk 1");
  }

  EXPECT_THAT(desk_action_view->close_all_button()->GetTooltipText(),
              u"Close D2 and windows");

  CloseDeskBar();
}

struct DeskButtonTestParams {
  ShelfAlignment alignment = ShelfAlignment::kBottom;
};

class DeskButtonTest
    : public AshTestBase,
      public ::testing::WithParamInterface<DeskButtonTestParams> {
 public:
  DeskButtonTest() = default;
  DeskButtonTest(const DeskButtonTest&) = delete;
  DeskButtonTest& operator=(const DeskButtonTest&) = delete;
  ~DeskButtonTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    shelf_test_api_ = std::make_unique<ShelfViewTestAPI>(
        GetPrimaryShelf()->GetShelfViewForTesting());
    GetPrimaryShelf()->SetAlignment(GetParam().alignment);
    SetShowDeskButtonInShelfPref(GetPrimaryUserPrefService(), true);
    // Set the shelf alignment pref to the desired alignment because if a test
    // updates the display size the shelf alignment will be reset from the pref.
    SetShelfAlignmentPref(GetPrimaryUserPrefService(), GetPrimaryDisplay().id(),
                          GetParam().alignment);
  }

  DeskButtonWidget* GetDeskButtonWidget() {
    return GetPrimaryShelf()->shelf_widget()->desk_button_widget();
  }

  DeskButton* GetDeskButton() {
    return GetDeskButtonWidget()->GetDeskButtonContainer()->desk_button();
  }

  DeskSwitchButton* GetPrevDeskButton() {
    return GetDeskButtonWidget()->GetDeskButtonContainer()->prev_desk_button();
  }

  DeskSwitchButton* GetNextDeskButton() {
    return GetDeskButtonWidget()->GetDeskButtonContainer()->next_desk_button();
  }

  // Switches to adjacent desk by clicking one of the desk switch buttons when
  // it's bottom shelf, or by pressing desk switch shortcut when it's side
  // shelf. When `next` is true, we go to the next desk, otherwise we go to the
  // previous desk.
  void SwitchToAdjacentDesk(bool next) {
    if (GetParam().alignment == ShelfAlignment::kBottom) {
      // The buttons will show if we hover the desk button, so we need to do
      // that first.
      auto* event_generator = GetEventGenerator();
      auto* desk_button = GetDeskButton();
      ASSERT_TRUE(desk_button);
      event_generator->MoveMouseTo(
          desk_button->GetBoundsInScreen().CenterPoint());
      views::ImageButton* target_button =
          next ? GetNextDeskButton() : GetPrevDeskButton();
      ASSERT_TRUE(target_button);
      EXPECT_TRUE(target_button->GetVisible());
      EXPECT_TRUE(target_button->GetEnabled());
      event_generator->MoveMouseTo(
          target_button->GetBoundsInScreen().CenterPoint());
      DeskSwitchAnimationWaiter waiter;
      event_generator->ClickLeftButton();
      waiter.Wait();
    } else {
      // Desk button does not expand for side shelf, thus use shortcut to switch
      // desks.
      DeskSwitchAnimationWaiter waiter;
      PressAndReleaseKey(next ? ui::VKEY_OEM_6 : ui::VKEY_OEM_4,
                         ui::EF_COMMAND_DOWN);
      waiter.Wait();
    }
  }

  // Clicks on the desk button.
  void ClickDeskButton() {
    // In the case of left and right shelf alignments, the button must be
    // hovered before it can be clicked if it is not already activated.
    HoverDeskButton();
    auto* event_generator = GetEventGenerator();
    auto* desk_button = GetDeskButton();
    ASSERT_TRUE(desk_button);
    event_generator->MoveMouseTo(
        desk_button->GetBoundsInScreen().CenterPoint());
    event_generator->ClickLeftButton();
  }

  // Hovers on the desk button.
  void HoverDeskButton() {
    auto* event_generator = GetEventGenerator();
    auto* desk_button = GetDeskButton();
    ASSERT_TRUE(desk_button);
    event_generator->MoveMouseTo(
        desk_button->GetBoundsInScreen().CenterPoint());
    if (!desk_button->zero_state()) {
      ASSERT_EQ(GetParam().alignment, ShelfAlignment::kBottom);
    }
  }

  // Unhovers away from the desk button.
  void UnhoverDeskButton() {
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(0, 0);
  }

  void OpenDeskBar() {
    DesksController::Get()->desk_bar_controller()->OpenDeskBar(
        Shell::Get()->GetPrimaryRootWindow());
  }

  void CloseDeskBar() {
    DesksController::Get()->desk_bar_controller()->CloseDeskBar(
        Shell::Get()->GetPrimaryRootWindow());
  }

  DeskBarViewBase* GetDeskBarView() {
    return DesksController::Get()->desk_bar_controller()->GetDeskBarView(
        Shell::Get()->GetPrimaryRootWindow());
  }

 private:
  std::unique_ptr<ShelfViewTestAPI> shelf_test_api_;
};

// Tests functionalities for `DeskSwitchButton`s.
TEST_P(DeskButtonTest, DeskSwitchButtons) {
  // With only one desk, both switch buttons are hidden.
  views::ImageButton* prev_desk_button = GetPrevDeskButton();
  views::ImageButton* next_desk_button = GetNextDeskButton();
  ASSERT_TRUE(prev_desk_button);
  ASSERT_TRUE(next_desk_button);
  EXPECT_FALSE(prev_desk_button->GetVisible());
  EXPECT_FALSE(next_desk_button->GetVisible());

  // Create a new desk. The previous desk button should be hidden and
  // the next desk button should be enabled.
  NewDesk();
  views::test::RunScheduledLayout(GetDeskButtonWidget());
  if (GetParam().alignment == ShelfAlignment::kBottom) {
    EXPECT_FALSE(prev_desk_button->GetVisible());
    EXPECT_TRUE(next_desk_button->GetVisible());
    EXPECT_TRUE(next_desk_button->GetEnabled());
  } else {
    EXPECT_FALSE(prev_desk_button->GetVisible());
    EXPECT_FALSE(next_desk_button->GetVisible());
  }

  // Switch to desk 2. The previous desk button should be visible and enabled.
  // The next desk button should be visible but disabled.
  SwitchToAdjacentDesk(/*next=*/true);
  if (GetParam().alignment == ShelfAlignment::kBottom) {
    EXPECT_TRUE(prev_desk_button->GetVisible());
    EXPECT_TRUE(prev_desk_button->GetEnabled());
    EXPECT_TRUE(next_desk_button->GetVisible());
    EXPECT_FALSE(next_desk_button->GetEnabled());
  } else {
    EXPECT_FALSE(prev_desk_button->GetVisible());
    EXPECT_FALSE(next_desk_button->GetVisible());
  }

  // Create a new desk. Both buttons should be visible and enabled.
  NewDesk();
  views::test::RunScheduledLayout(GetDeskButtonWidget());
  if (GetParam().alignment == ShelfAlignment::kBottom) {
    EXPECT_TRUE(prev_desk_button->GetVisible());
    EXPECT_TRUE(prev_desk_button->GetEnabled());
    EXPECT_TRUE(next_desk_button->GetVisible());
    EXPECT_TRUE(next_desk_button->GetEnabled());
  } else {
    EXPECT_FALSE(prev_desk_button->GetVisible());
    EXPECT_FALSE(next_desk_button->GetVisible());
  }

  // Switch to desk 3. The previous desk button should be visible and enabled.
  // The next desk button should be visible but disabled.
  SwitchToAdjacentDesk(/*next=*/true);
  if (GetParam().alignment == ShelfAlignment::kBottom) {
    EXPECT_TRUE(prev_desk_button->GetVisible());
    EXPECT_TRUE(prev_desk_button->GetEnabled());
    EXPECT_TRUE(next_desk_button->GetVisible());
    EXPECT_FALSE(next_desk_button->GetEnabled());
  } else {
    EXPECT_FALSE(prev_desk_button->GetVisible());
    EXPECT_FALSE(next_desk_button->GetVisible());
  }

  // Switch back to desk 2. Both buttons should be visible and enabled.
  SwitchToAdjacentDesk(/*next=*/false);
  if (GetParam().alignment == ShelfAlignment::kBottom) {
    EXPECT_TRUE(prev_desk_button->GetVisible());
    EXPECT_TRUE(prev_desk_button->GetEnabled());
    EXPECT_TRUE(next_desk_button->GetVisible());
    EXPECT_TRUE(next_desk_button->GetEnabled());
  } else {
    EXPECT_FALSE(prev_desk_button->GetVisible());
    EXPECT_FALSE(next_desk_button->GetVisible());
  }
}

// Tests that button text updates when desk is changed.
TEST_P(DeskButtonTest, DeskButtonTextReflectsDeskChange) {
  // Desk button will be forced to be zero state for display that is narrower
  // than 1280.
  UpdateDisplay("1280x720");

  NewDesk();

  auto* controller = DesksController::Get();
  auto* desk_1 = controller->GetDeskAtIndex(0);
  auto* desk_2 = controller->GetDeskAtIndex(1);

  // Check that renaming the active desk changes the text in the button.
  ASSERT_TRUE(desk_1->is_active());

  desk_1->SetName(u"Work", /*set_by_user=*/true);
  auto* desk_button = GetDeskButton();
  ASSERT_TRUE(desk_button);
  EXPECT_EQ(GetParam().alignment == ShelfAlignment::kBottom ? u"Work" : u"W",
            desk_button->desk_name_label()->GetText());
  views::test::RunScheduledLayout(desk_button);

  desk_2->SetName(u"Fun", /*set_by_user=*/true);
  EXPECT_EQ(GetParam().alignment == ShelfAlignment::kBottom ? u"Work" : u"W",
            desk_button->desk_name_label()->GetText());

  SwitchToAdjacentDesk(/*next=*/true);
  EXPECT_EQ(GetParam().alignment == ShelfAlignment::kBottom ? u"Fun" : u"F",
            desk_button->desk_name_label()->GetText());

  // Add a third desk and don't name it to check how default desk names are
  // handled.
  NewDesk();
  SwitchToAdjacentDesk(/*next=*/true);
  EXPECT_EQ(GetParam().alignment == ShelfAlignment::kBottom ? u"Desk 3" : u"#3",
            desk_button->desk_name_label()->GetText());
}

// Tests that emojis show up correctly for desk names.
TEST_P(DeskButtonTest, DeskButtonTextWorksWithEmojis) {
  // Desk button will be forced to be zero state for display that is narrower
  // than 1280.
  UpdateDisplay("1280x720");

  NewDesk();

  auto* controller = DesksController::Get();
  controller->GetDeskAtIndex(0)->SetName(u"😃emoji", /*set_by_user=*/true);

  auto* desk_button = GetDeskButton();
  ASSERT_TRUE(desk_button);
  EXPECT_EQ(
      GetParam().alignment == ShelfAlignment::kBottom ? u"😃emoji" : u"😃",
      desk_button->desk_name_label()->GetText());
}

// Tests that the desk button is visible after changing desks from overview.
TEST_P(DeskButtonTest, OverviewDeskSwitch) {
  NewDesk();
  EXPECT_TRUE(GetDeskButtonWidget()->GetLayer()->GetTargetVisibility());

  EnterOverview();
  EXPECT_FALSE(GetDeskButtonWidget()->GetLayer()->GetTargetVisibility());
  auto* desks_controller = DesksController::Get();
  ActivateDesk(desks_controller->GetDeskAtIndex(1));
  EXPECT_TRUE(GetDeskButtonWidget()->GetLayer()->GetTargetVisibility());
}

// Tests that switching the shelf alignment correctly repositions the desk
// button and updates their colors.
TEST_P(DeskButtonTest, UpdateShelfAlignmentDuringTest) {
  // Desk button will be forced to be zero state for display that is narrower
  // than 1280.
  UpdateDisplay("1280x720");

  NewDesk();
  DesksController::Get()->GetDeskAtIndex(0)->SetName(u"school",
                                                     /*set_by_user=*/true);

  const bool bottom_at_start = GetParam().alignment == ShelfAlignment::kBottom;
  auto* desk_button = GetDeskButton();
  ASSERT_TRUE(desk_button);
  // Verify desk names and color changes.
  ASSERT_EQ(bottom_at_start ? u"school" : u"s",
            desk_button->desk_name_label()->GetText());
  auto* color_provider = desk_button->GetColorProvider();
  ASSERT_EQ(color_provider->GetColor(bottom_at_start
                                         ? cros_tokens::kCrosSysSystemOnBase1
                                         : cros_tokens::kCrosSysSystemOnBase),
            desk_button->GetBackground()->get_color());

  // Activate/Deactivate the desk button and verify color changes.
  ClickDeskButton();
  ASSERT_EQ(
      color_provider->GetColor(cros_tokens::kCrosSysSystemPrimaryContainer),
      desk_button->GetBackground()->get_color());
  ClickDeskButton();
  ASSERT_EQ(color_provider->GetColor(bottom_at_start
                                         ? cros_tokens::kCrosSysSystemOnBase1
                                         : cros_tokens::kCrosSysSystemOnBase),
            desk_button->GetBackground()->get_color());

  // Update shelf alignment and verify desk names and color changes.
  GetPrimaryShelf()->SetAlignment(bottom_at_start ? ShelfAlignment::kLeft
                                                  : ShelfAlignment::kBottom);
  ASSERT_EQ(color_provider->GetColor(bottom_at_start
                                         ? cros_tokens::kCrosSysSystemOnBase
                                         : cros_tokens::kCrosSysSystemOnBase1),
            desk_button->GetBackground()->get_color());
  EXPECT_EQ(bottom_at_start ? u"s" : u"school",
            desk_button->desk_name_label()->GetText());
}

// Tests that when the desk button is activated, shelf auto-hide should be
// suspended.
TEST_P(DeskButtonTest, SuspendShelfAutoHideWhenActivated) {
  auto* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ASSERT_FALSE(shelf->disable_auto_hide());

  auto* desk_button = GetDeskButton();
  ASSERT_TRUE(desk_button);

  ClickDeskButton();
  UnhoverDeskButton();
  EXPECT_TRUE(desk_button->is_activated());
  EXPECT_NE(desk_button->GetState(), views::Button::ButtonState::STATE_HOVERED);
  EXPECT_TRUE(shelf->disable_auto_hide());

  ClickDeskButton();
  UnhoverDeskButton();
  EXPECT_FALSE(desk_button->is_activated());
  EXPECT_NE(desk_button->GetState(), views::Button::ButtonState::STATE_HOVERED);
  ASSERT_FALSE(shelf->disable_auto_hide());
}

// Tests that the desk button and its child components are correctly positioned
// in each phase of the desk button's desk switch process.
TEST_P(DeskButtonTest, ValidateDeskButtonPosition) {
  // Desk button will be forced to be zero state for display that is narrower
  // than 1280.
  UpdateDisplay("1280x720");

  NewDesk();
  NewDesk();

  auto* desk_button = GetDeskButton();
  auto* prev_desk_button = GetPrevDeskButton();
  auto* next_desk_button = GetNextDeskButton();
  auto* desk_name_label = desk_button->desk_name_label();
  auto* desk_controller = DesksController::Get();
  const int desk_count = desk_controller->GetNumberOfDesks();

  for (int i = desk_count - 1; i >= 0; i--) {
    ActivateDesk(desk_controller->desks()[i].get());

    const bool should_show_prev_desk_button =
        GetParam().alignment == ShelfAlignment::kBottom && i > 0;
    const bool should_show_next_desk_button =
        GetParam().alignment == ShelfAlignment::kBottom;
    const bool should_enable_next_desk_button =
        GetParam().alignment == ShelfAlignment::kBottom && i < desk_count - 1;

    // Check the desk button and both desk switch buttons.
    if (GetParam().alignment == ShelfAlignment::kBottom) {
      EXPECT_TRUE(gfx::Rect(4, 4, 128, 28).Contains(desk_button->bounds()));
      EXPECT_TRUE(
          gfx::Rect(10, 0, 102, 28).Contains(desk_name_label->bounds()));
    } else {
      EXPECT_EQ(desk_button->bounds(), gfx::Rect(0, 0, 36, 36));
      EXPECT_EQ(desk_name_label->bounds(), gfx::Rect(4, 4, 28, 28));
    }
    EXPECT_EQ(prev_desk_button->GetVisible(), should_show_prev_desk_button);
    if (prev_desk_button->GetVisible()) {
      EXPECT_TRUE(prev_desk_button->GetEnabled());
    }
    EXPECT_EQ(next_desk_button->GetVisible(), should_show_next_desk_button);
    if (next_desk_button->GetVisible()) {
      EXPECT_EQ(next_desk_button->GetEnabled(), should_enable_next_desk_button);
    }
  }
}

// Tests that desk button press metrics are correctly recorded.
TEST_P(DeskButtonTest, DeskButtonPressMetrics) {
  NewDesk();
  base::HistogramTester histogram_tester;
  ClickDeskButton();
  histogram_tester.ExpectTotalCount(kDeskButtonPressesHistogramName, 1);
  ClickDeskButton();
  histogram_tester.ExpectTotalCount(kDeskButtonPressesHistogramName, 2);
}

// Tests that the desk button shows up in the correct position and layout
// in RTL. The desk bar is laid out LTR in RTL mode so the desk button should be
// too, ensuring that pressing the left chevron button moves activation to the
// desk to the left in the desk bar.
TEST_P(DeskButtonTest, LayoutInRTL) {
  // Turn on RTL mode.
  const bool default_rtl = base::i18n::IsRTL();
  base::i18n::SetRTLForTesting(true);
  EXPECT_TRUE(base::i18n::IsRTL());

  // The test doesn't start in RTL so we need to tell the widget to swap the
  // desk switch buttons because RTL was disabled in the constructor.
  GetDeskButtonWidget()->HandleLocaleChange();
  GetPrimaryShelf()->shelf_layout_manager()->LayoutShelf();

  // Desk button will be forced to be zero state for display that is narrower
  // than 1280.
  UpdateDisplay("1280x720");

  // Add an app icon to the shelf.
  SkBitmap app_bitmap;
  app_bitmap.allocN32Pixels(1, 1);
  app_bitmap.eraseColor(SK_ColorRED);
  ShelfTestUtil::AddAppShortcutWithIcon(
      "0", TYPE_PINNED_APP, gfx::ImageSkia::CreateFrom1xBitmap(app_bitmap));

  // Navigate to the 2nd of 3 desks so both buttons should be visible.
  NewDesk();
  NewDesk();
  views::test::RunScheduledLayout(GetDeskButtonWidget());
  SwitchToAdjacentDesk(/*next=*/true);

  // The desk button should show up to the right of the shelf apps in horizontal
  // and on top in vertical.
  const gfx::Rect desk_button_bounds = GetDeskButton()->GetBoundsInScreen();
  const gfx::Rect app_icon_bounds = GetPrimaryShelf()
                                        ->hotseat_widget()
                                        ->GetShelfView()
                                        ->first_visible_button_for_testing()
                                        ->GetBoundsInScreen();

  switch (GetParam().alignment) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      EXPECT_TRUE(gfx::Rect(684, 682, 128, 28).Contains(desk_button_bounds));
      EXPECT_LT(app_icon_bounds.x(), desk_button_bounds.x());
      break;
    case ShelfAlignment::kLeft:
      EXPECT_EQ(gfx::Rect(6, 286, 36, 36), desk_button_bounds);
      EXPECT_LT(desk_button_bounds.y(), app_icon_bounds.y());
      break;
    case ShelfAlignment::kRight:
      EXPECT_EQ(gfx::Rect(1238, 286, 36, 36), desk_button_bounds);
      EXPECT_LT(desk_button_bounds.y(), app_icon_bounds.y());
      break;
  }

  // Hover over the button to show the desk switch buttons.
  auto* prev_desk_button = GetPrevDeskButton();
  auto* next_desk_button = GetNextDeskButton();
  if (GetParam().alignment == ShelfAlignment::kBottom) {
    ASSERT_TRUE(prev_desk_button->GetVisible());
    ASSERT_TRUE(prev_desk_button->GetEnabled());
    ASSERT_TRUE(next_desk_button->GetVisible());
    ASSERT_TRUE(next_desk_button->GetEnabled());

    // The previous desk button should be to the right of the desk button, and
    // the next desk button should be to the right of the previous desk button.
    EXPECT_GT(prev_desk_button->GetBoundsInScreen().x(),
              desk_button_bounds.right());
    EXPECT_GT(next_desk_button->GetBoundsInScreen().x(),
              prev_desk_button->GetBoundsInScreen().right());
  } else {
    ASSERT_FALSE(prev_desk_button->GetVisible());
    ASSERT_FALSE(next_desk_button->GetVisible());
  }

  // Clicking the previous desk button should take us to desk 1, and clicking
  // the next desk button twice should take us to desk 3.
  auto* desks_controller = DesksController::Get();
  SwitchToAdjacentDesk(/*next=*/false);
  EXPECT_EQ(0, desks_controller->GetActiveDeskIndex());
  SwitchToAdjacentDesk(/*next=*/true);
  SwitchToAdjacentDesk(/*next=*/true);
  EXPECT_EQ(2, desks_controller->GetActiveDeskIndex());

  // Recover to default RTL mode.
  base::i18n::SetRTLForTesting(default_rtl);
}

TEST_P(DeskButtonTest, BarBoundsWithDeviceSacleFactorChange) {
  // Test both upscaling and downscaling.
  for (auto key : {ui::VKEY_OEM_MINUS, ui::VKEY_OEM_PLUS}) {
    OpenDeskBar();
    PressAndReleaseKey(key, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
    ASSERT_TRUE(GetDeskBarView());
    gfx::Rect bounds = GetDeskBarView()->GetWidget()->GetWindowBoundsInScreen();
    CloseDeskBar();
    OpenDeskBar();
    gfx::Rect expected_bounds =
        GetDeskBarView()->GetWidget()->GetWindowBoundsInScreen();
    CloseDeskBar();
    EXPECT_EQ(bounds, expected_bounds);
  }
}

TEST_P(DeskButtonTest, BarBoundsWithScreenRotationChange) {
  const int control_and_shift = ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN;

  // Accept the dialog once so later on it does not pop up.
  auto* accessibility_controller = Shell::Get()->accessibility_controller();
  PressAndReleaseKey(ui::VKEY_BROWSER_REFRESH, control_and_shift);
  if (auto* confirm_dialog =
          accessibility_controller->GetConfirmationDialogForTest()) {
    confirm_dialog->AcceptDialog();
    base::RunLoop().RunUntilIdle();
  }

  // Test with all four screen rotations.
  for (int i = 0; i < 4; i++) {
    OpenDeskBar();
    PressAndReleaseKey(ui::VKEY_BROWSER_REFRESH, control_and_shift);
    ASSERT_TRUE(GetDeskBarView());
    gfx::Rect bounds = GetDeskBarView()->GetWidget()->GetWindowBoundsInScreen();
    CloseDeskBar();
    OpenDeskBar();
    gfx::Rect expected_bounds =
        GetDeskBarView()->GetWidget()->GetWindowBoundsInScreen();
    CloseDeskBar();
    EXPECT_EQ(bounds, expected_bounds);
  }
}

TEST_P(DeskButtonTest, BarBoundsWithWorkAreaChangeChromevox) {
  const int control_and_alt = ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN;

  // Test with chromevox on and off.
  OpenDeskBar();
  PressAndReleaseKey(ui::VKEY_Z, control_and_alt);
  ASSERT_TRUE(GetDeskBarView());
  gfx::Rect bounds = GetDeskBarView()->GetWidget()->GetWindowBoundsInScreen();
  CloseDeskBar();
  OpenDeskBar();
  gfx::Rect expected_bounds =
      GetDeskBarView()->GetWidget()->GetWindowBoundsInScreen();
  CloseDeskBar();
  PressAndReleaseKey(ui::VKEY_Z, control_and_alt);
  EXPECT_EQ(bounds, expected_bounds);
}

TEST_P(DeskButtonTest, BarBoundsWithWorkAreaChangeDockedMagnifier) {
  // Set the display large enough for the docked magnifier and the desk bar.
  UpdateDisplay("1024x768");

  const int control_and_search = ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN;

  // Accept the dialog once so later on it does not pop up.
  auto* accessibility_controller = Shell::Get()->accessibility_controller();
  PressAndReleaseKey(ui::VKEY_D, control_and_search);
  if (auto* confirm_dialog =
          accessibility_controller->GetConfirmationDialogForTest()) {
    confirm_dialog->AcceptDialog();
    base::RunLoop().RunUntilIdle();
  }
  PressAndReleaseKey(ui::VKEY_D, control_and_search);

  // Test with docked magnifier on and off.
  OpenDeskBar();
  PressAndReleaseKey(ui::VKEY_D, control_and_search);
  ASSERT_TRUE(GetDeskBarView());
  gfx::Rect bounds = GetDeskBarView()->GetWidget()->GetWindowBoundsInScreen();
  CloseDeskBar();
  OpenDeskBar();
  gfx::Rect expected_bounds =
      GetDeskBarView()->GetWidget()->GetWindowBoundsInScreen();
  CloseDeskBar();
  PressAndReleaseKey(ui::VKEY_D, control_and_search);
  EXPECT_EQ(bounds, expected_bounds);
}

TEST_P(DeskButtonTest, BarBoundsWithRTL) {
  UpdateDisplay("800x600");

  // Turn on RTL mode.
  const bool default_rtl = base::i18n::IsRTL();
  base::i18n::SetRTLForTesting(true);
  ASSERT_TRUE(base::i18n::IsRTL());

  OpenDeskBar();
  EXPECT_EQ(GetDeskBarView()->bounds(), gfx::Rect(0, 0, 154, 98));

  CloseDeskBar();

  // Recover to default RTL mode.
  base::i18n::SetRTLForTesting(default_rtl);
}

// Tests that desk button tab order is correct in the shelf.
TEST_P(DeskButtonTest, TabOrder) {
  NewDesk();
  NewDesk();
  auto* desks_controller = DesksController::Get();
  ASSERT_THAT(desks_controller->desks().size(), 3);
  ASSERT_THAT(desks_controller->GetActiveDeskIndex(), 0);

  // Tabbing once should focus the shelf navigation widget.
  PressAndReleaseKey(ui::VKEY_TAB);

  if (GetParam().alignment == ShelfAlignment::kBottom) {
    // One more tab should focus the desk button, then the next desk button.
    // There should be no previous desk button since we're on the first desk.
    PressAndReleaseKey(ui::VKEY_TAB);
    ASSERT_TRUE(GetDeskButton()->HasFocus());
    PressAndReleaseKey(ui::VKEY_TAB);
    ASSERT_TRUE(GetNextDeskButton()->HasFocus());

    // Tabbing in the other direction should work too.
    PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
    ASSERT_TRUE(GetDeskButton()->HasFocus());
    PressAndReleaseKey(ui::VKEY_TAB);
    ASSERT_TRUE(GetNextDeskButton()->HasFocus());

    // Pressing the next desk button should take us to the next desk, and
    // immediately pressing enter again should take us to the last desk.
    DeskSwitchAnimationWaiter waiter;
    PressAndReleaseKey(ui::VKEY_RETURN);
    waiter.Wait();
    ASSERT_THAT(desks_controller->GetActiveDeskIndex(), 1);
    ASSERT_TRUE(GetPrevDeskButton()->GetEnabled());
    ASSERT_TRUE(GetNextDeskButton()->GetEnabled());
    DeskSwitchAnimationWaiter waiter2;
    PressAndReleaseKey(ui::VKEY_RETURN);
    waiter2.Wait();
    ASSERT_THAT(desks_controller->GetActiveDeskIndex(), 2);

    // Focus should have been passed to the hotseat widget now that the next
    // desk button isn't visible.
    ASSERT_FALSE(GetNextDeskButton()->GetEnabled());
  } else {
    // Desk button does not expand for side shelf.
    PressAndReleaseKey(ui::VKEY_TAB);
    ASSERT_TRUE(GetDeskButton()->HasFocus());
    ASSERT_TRUE(GetDeskButton()->zero_state());
    PressAndReleaseKey(ui::VKEY_TAB);
    ASSERT_FALSE(GetDeskButton()->HasFocus());
    ASSERT_TRUE(GetDeskButton()->zero_state());
  }
}

// Tests that desk bar is on top of floated window.
TEST_P(DeskButtonTest, BarAboveFloatWindow) {
  // Create a floated window.
  std::unique_ptr<aura::Window> floated_window = CreateAppWindow();
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(floated_window.get())->IsFloated());

  // Open desk bar, set the floated window to be the same bounds, then check if
  // the bar is above the floated window.
  OpenDeskBar();
  auto* desk_bar_view = GetDeskBarView();
  floated_window->SetBounds(desk_bar_view->GetBoundsInScreen());
  EXPECT_EQ(
      GetTopmostWindowAtPoint(desk_bar_view->GetBoundsInScreen().CenterPoint(),
                              /*ignore=*/{}),
      desk_bar_view->GetWidget()->GetNativeWindow());
}

// Tests that desk bar sends back focus to desk button when hitting esc.
TEST_P(DeskButtonTest, SendFocusBackToDeskButton) {
  // Focus the desk button.
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_TRUE(GetDeskButton()->HasFocus());

  // Open desk bar.
  PressAndReleaseKey(ui::VKEY_RETURN);
  ASSERT_TRUE(GetDeskBarView());

  // Hit esc and verify focus is sent back to the desk button.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  ASSERT_TRUE(GetDeskButton()->HasFocus());
}

// Tests that desk button shows context menu when right clicked and long tapped,
// and does not show the menu when the desk bar is active.
TEST_P(DeskButtonTest, DeskButtonContextMenu) {
  for (bool use_touch_gestures : {false, true}) {
    auto* event_generator = GetEventGenerator();
    auto* desk_button = GetDeskButton();

    // The context menu should show when the desk bar is *not* active.
    if (use_touch_gestures) {
      LongGestureTap(desk_button->GetBoundsInScreen().CenterPoint(),
                     event_generator);
    } else {
      RightClickOn(desk_button);
    }
    EXPECT_NE(desk_button->GetState(),
              views::Button::ButtonState::STATE_HOVERED);
    EXPECT_TRUE(GetPrimaryShelf()->GetShelfViewForTesting()->IsShowingMenu());
    PressAndReleaseKey(ui::VKEY_ESCAPE);
    EXPECT_FALSE(GetPrimaryShelf()->GetShelfViewForTesting()->IsShowingMenu());

    // The context menu should *not* show when the desk bar is active.
    ClickDeskButton();
    if (use_touch_gestures) {
      LongGestureTap(desk_button->GetBoundsInScreen().CenterPoint(),
                     event_generator);
    } else {
      RightClickOn(desk_button);
    }
    EXPECT_FALSE(GetPrimaryShelf()->GetShelfViewForTesting()->IsShowingMenu());
    CloseDeskBar();
  }
}

// Tests that desk switch buttons show context menu when right clicked and long
// tapped, and do not show the menu when the desk bar is active.
TEST_P(DeskButtonTest, DeskSwitchButtonContextMenu) {
  if (GetParam().alignment != ShelfAlignment::kBottom) {
    SUCCEED();
    return;
  }

  NewDesk();
  NewDesk();

  struct DeskSwitchButtonTestCase {
    bool visible;
    bool enabled;
    bool show_context_menu;
  };
  const DeskSwitchButtonTestCase prev_test_cases[] = {
      {.visible = false, .enabled = false, .show_context_menu = false},
      {.visible = true, .enabled = true, .show_context_menu = true},
      {.visible = true, .enabled = true, .show_context_menu = true},
  };
  const DeskSwitchButtonTestCase next_test_cases[] = {
      {.visible = true, .enabled = true, .show_context_menu = true},
      {.visible = true, .enabled = true, .show_context_menu = true},
      {.visible = true, .enabled = false, .show_context_menu = false},
  };

  auto* event_generator = GetEventGenerator();
  auto* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
  auto verify_button = [&](DeskSwitchButton* button, bool button_visible,
                           bool button_enabled, bool button_show_context_menu,
                           bool use_touch_gestures, bool desk_bar_active) {
    SCOPED_TRACE(
        "Testing " +
        (button->type() == DeskSwitchButton::Type::kPrev
             ? std::string("previous desk button")
             : std::string("next desk button")) +
        " in " +
        base::UTF16ToUTF8(DesksController::Get()->active_desk()->name()) +
        " using " +
        (use_touch_gestures ? std::string("gestures") : std::string("mouse")) +
        " when desk bar " +
        (desk_bar_active ? std::string("active") : std::string("not active")));

    ASSERT_EQ(button->GetVisible(), button_visible);
    if (!button->GetVisible()) {
      return;
    }
    EXPECT_EQ(button->GetEnabled(), button_enabled);
    if (use_touch_gestures) {
      LongGestureTap(button->GetBoundsInScreen().CenterPoint(),
                     event_generator);
    } else {
      RightClickOn(button);
    }
    views::test::RunScheduledLayout(button);
    EXPECT_EQ(shelf_view->IsShowingMenu(), button_show_context_menu);
    if (!desk_bar_active) {
      EXPECT_NE(button->GetState(), views::Button::ButtonState::STATE_HOVERED);
    }
    if (shelf_view->IsShowingMenu()) {
      PressAndReleaseKey(ui::VKEY_ESCAPE);
      EXPECT_FALSE(shelf_view->IsShowingMenu());
    }
  };

  for (bool use_touch_gestures : {false, true}) {
    auto* desk_controller = DesksController::Get();
    for (int index = 0; index < desk_controller->GetNumberOfDesks(); index++) {
      // Switch to the desired desk if needed.
      if (desk_controller->GetActiveDeskIndex() != index) {
        ActivateDesk(desk_controller->GetDeskAtIndex(index));
      }

      // The context menu could show when the desk bar is *not* active.
      verify_button(GetPrevDeskButton(), prev_test_cases[index].visible,
                    prev_test_cases[index].enabled,
                    prev_test_cases[index].show_context_menu,
                    use_touch_gestures, false);
      verify_button(GetNextDeskButton(), next_test_cases[index].visible,
                    next_test_cases[index].enabled,
                    next_test_cases[index].show_context_menu,
                    use_touch_gestures, false);

      // The context menu could *not* show when the desk bar is active.
      ClickDeskButton();
      verify_button(GetPrevDeskButton(), prev_test_cases[index].visible,
                    prev_test_cases[index].enabled, false, use_touch_gestures,
                    true);
      verify_button(GetNextDeskButton(), next_test_cases[index].visible,
                    next_test_cases[index].enabled, false, use_touch_gestures,
                    true);
      CloseDeskBar();
    }
  }
}

// Tests that metrics are being recorded when a desk animation screenshot is
// taken.
TEST_P(DesksAcceleratorsTest, DeskSwitchScreenshotMetricsRecording) {
  NewDesk();
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kDeskSwitchScreenshotResultHistogramName,
                                    0);

  // Switching desks should result in 2 successful screenshots: one for the
  // starting desk and one for the ending desk.
  DeskSwitchAnimationWaiter waiter;
  SendAccelerator(ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN);
  waiter.Wait();
  histogram_tester.ExpectTotalCount(kDeskSwitchScreenshotResultHistogramName,
                                    2);
  histogram_tester.ExpectBucketCount(kDeskSwitchScreenshotResultHistogramName,
                                     true, 2);
}

class DeskProfilesTest : public AshTestBase {
 public:
  DeskProfilesTest() = default;
  ~DeskProfilesTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      chromeos::features::kDeskProfiles};
};

TEST_F(DeskProfilesTest, RemoveProfile) {
  // This test creates three dummy lacros profiles and sets up three desks with
  // each associated with a different user. The last user is then removed and we
  // verify that the desk has reverted to the default lacros profile.
  AddDummyLacrosDeskProfiles(3);

  const uint64_t lacros_profile_id1 = GetDummyLacrosDeskProfileId(0);
  const uint64_t lacros_profile_id2 = GetDummyLacrosDeskProfileId(1);
  const uint64_t lacros_profile_id3 = GetDummyLacrosDeskProfileId(2);

  GetDeskProfilesTestDelegate().SetPrimaryProfileByProfileId(
      lacros_profile_id1);

  NewDesk();
  NewDesk();

  // Assign different lacros profiles to all three desks.
  auto* controller = DesksController::Get();
  auto* desk1 = controller->GetDeskAtIndex(0);
  auto* desk2 = controller->GetDeskAtIndex(1);
  auto* desk3 = controller->GetDeskAtIndex(2);
  desk1->SetLacrosProfileId(lacros_profile_id1, /*source=*/std::nullopt);
  desk2->SetLacrosProfileId(lacros_profile_id2, /*source=*/std::nullopt);
  desk3->SetLacrosProfileId(lacros_profile_id3, /*source=*/std::nullopt);

  EXPECT_EQ(desk1->lacros_profile_id(), lacros_profile_id1);
  EXPECT_EQ(desk2->lacros_profile_id(), lacros_profile_id2);
  EXPECT_EQ(desk3->lacros_profile_id(), lacros_profile_id3);

  // Remove the last profile. We now expect the desk to be updated to the
  // default profile (lacros_profile_id1).
  GetDeskProfilesTestDelegate().RemoveTestProfile(lacros_profile_id3);

  EXPECT_EQ(desk1->lacros_profile_id(), lacros_profile_id1);
  EXPECT_EQ(desk2->lacros_profile_id(), lacros_profile_id2);
  EXPECT_EQ(desk3->lacros_profile_id(), lacros_profile_id1);
}

TEST_F(DeskProfilesTest, DeskProfilesButtonClickMetrics) {
  // The desk profile button is visible when there are two or more profiles.
  AddDummyLacrosDeskProfiles(2);

  base::HistogramTester histogram_tester;
  auto* desk_bar_controller = DesksController::Get()->desk_bar_controller();
  desk_bar_controller->OpenDeskBar(Shell::Get()->GetPrimaryRootWindow());
  auto* desk_bar_view =
      desk_bar_controller->GetDeskBarView(Shell::Get()->GetPrimaryRootWindow());
  views::test::RunScheduledLayout(desk_bar_view);
  ASSERT_EQ(1u, desk_bar_view->mini_views().size());

  DeskProfilesButton* desk_profile_button =
      DesksTestApi::GetDeskProfileButton(desk_bar_view->mini_views()[0]);
  ASSERT_NE(desk_profile_button, nullptr);

  // Test desk profile button click metrics.
  LeftClickOn(desk_profile_button);
  histogram_tester.ExpectTotalCount(kDeskProfilesPressesHistogramName, 1);

  // Test context menu profile manager click metrics.
  DeskActionContextMenu* menu = desk_profile_button->menu();
  ASSERT_NE(menu, nullptr);

  views::MenuItemView* menu_item = DesksTestApi::GetDeskActionContextMenuItem(
      menu, DeskActionContextMenu::kShowProfileManager);
  ASSERT_NE(menu_item, nullptr);

  LeftClickOn(menu_item);
  histogram_tester.ExpectTotalCount(
      kDeskProfilesOpenProfileManagerHistogramName, 1);
}

TEST_F(DeskProfilesTest, SelectProfile) {
  AddDummyLacrosDeskProfiles(2);

  base::HistogramTester histogram_tester;
  auto* desk_bar_controller = DesksController::Get()->desk_bar_controller();

  NewDesk();
  auto* desk1 = DesksController::Get()->GetDeskAtIndex(0);
  auto* desk2 = DesksController::Get()->GetDeskAtIndex(1);

  // By default, neither desk has a lacros profile ID assigned.
  EXPECT_EQ(0u, desk1->lacros_profile_id());
  EXPECT_EQ(0u, desk2->lacros_profile_id());

  desk_bar_controller->OpenDeskBar(Shell::Get()->GetPrimaryRootWindow());
  auto* desk_bar_view =
      desk_bar_controller->GetDeskBarView(Shell::Get()->GetPrimaryRootWindow());
  views::test::RunScheduledLayout(desk_bar_view);
  ASSERT_EQ(2u, desk_bar_view->mini_views().size());

  // Assign a profile to the second desk using the desk profile button.
  {
    DeskProfilesButton* desk_profile_button =
        DesksTestApi::GetDeskProfileButton(desk_bar_view->mini_views()[1]);
    ASSERT_TRUE(desk_profile_button);

    LeftClickOn(desk_profile_button);
    DeskActionContextMenu* menu = desk_profile_button->menu();
    ASSERT_TRUE(menu);

    // Get the menu item for the seecond profile and click it.
    views::MenuItemView* menu_item = DesksTestApi::GetDeskActionContextMenuItem(
        menu, DeskActionContextMenu::kDynamicProfileStart + 1);
    ASSERT_TRUE(menu_item);
    LeftClickOn(menu_item);

    histogram_tester.ExpectBucketCount(
        kDeskProfilesSelectProfileHistogramName,
        DeskProfilesSelectProfileSource::kDeskProfileButton, 1);

    // Verify that the profile has indeed been set on desk 2.
    EXPECT_EQ(desk2->lacros_profile_id(), GetDummyLacrosDeskProfileId(1));
  }

  // Assign a profile to the second desk using the desk action context menu.
  {
    DeskActionContextMenu* menu = DesksTestApi::GetContextMenuForDesk(
        DeskBarViewBase::Type::kDeskButton, 1);
    ASSERT_TRUE(menu);

    // Get the menu item for the first profile and click it.
    views::MenuItemView* menu_item = DesksTestApi::GetDeskActionContextMenuItem(
        menu, DeskActionContextMenu::kDynamicProfileStart);
    ASSERT_TRUE(menu_item);
    LeftClickOn(menu_item);

    histogram_tester.ExpectBucketCount(
        kDeskProfilesSelectProfileHistogramName,
        DeskProfilesSelectProfileSource::kDeskActionContextMenu, 1);

    // Verify that the profile has been updated on desk 2.
    EXPECT_EQ(desk2->lacros_profile_id(), GetDummyLacrosDeskProfileId(0));
  }
}

// TODO(afakhry): Add more tests:
// - Always on top windows are not tracked by any desk.
// - Reusing containers when desks are removed and created.

// Instantiate the parametrized tests.

// This is used for tests that test all combinations of 8/16 desks, and
// clicks/touch. Additionally, some combinations are tested with per-desk-shelf
// enabled.
constexpr DesksTestParams kTestCombinations[] = {
    {.use_touch_gestures = false, .use_16_desks = false},
    {.use_touch_gestures = false, .use_16_desks = true},
    {.use_touch_gestures = true, .use_16_desks = false},
    {.use_touch_gestures = true, .use_16_desks = true},
    // Per-desk shelf enabled combinations.
    {.per_desk_shelf = true},
};

// This is used for tests that only want to test 8/16 desks.
constexpr DesksTestParams kDeskCountOnly[] = {
    {.use_16_desks = false},
    {.use_16_desks = true},
};

std::string GetDeskCountSuffix(bool use_16_desks) {
  return use_16_desks ? "16Desks" : "8Desks";
}

std::string GetTestSuffix(const DesksTestParams& params) {
  std::string use_touch = params.use_touch_gestures ? "Touch" : "Mouse";
  std::string use_16 = GetDeskCountSuffix(params.use_16_desks);
  std::string use_per_desk_shelf =
      params.per_desk_shelf ? "PerDeskShelf" : "NoPerDeskShelf";
  return base::StringPrintf("%s_%s_%s", use_touch.c_str(), use_16.c_str(),
                            use_per_desk_shelf.c_str());
}

std::string GetDeskCountOnlyTestSuffix(
    const testing::TestParamInfo<DesksTestParams>& info) {
  return GetDeskCountSuffix(info.param.use_16_desks);
}

constexpr DeskButtonTestParams kDeskButtonTestParamCombinations[] = {
    {.alignment = ShelfAlignment::kBottom},
    {.alignment = ShelfAlignment::kLeft},
    {.alignment = ShelfAlignment::kRight}};

INSTANTIATE_TEST_SUITE_P(
    All,
    DesksTest,
    ValuesIn(kTestCombinations),
    [](const testing::TestParamInfo<DesksTestParams>& info) {
      return GetTestSuffix(info.param);
    });

INSTANTIATE_TEST_SUITE_P(All,
                         DesksEditableNamesTest,
                         ValuesIn(kDeskCountOnly),
                         GetDeskCountOnlyTestSuffix);
INSTANTIATE_TEST_SUITE_P(All,
                         TabletModeDesksTest,
                         ValuesIn(kDeskCountOnly),
                         GetDeskCountOnlyTestSuffix);
INSTANTIATE_TEST_SUITE_P(All,
                         DesksAcceleratorsTest,
                         ValuesIn(kDeskCountOnly),
                         GetDeskCountOnlyTestSuffix);
INSTANTIATE_TEST_SUITE_P(All,
                         DesksMockTimeTest,
                         ValuesIn(kDeskCountOnly),
                         GetDeskCountOnlyTestSuffix);
INSTANTIATE_TEST_SUITE_P(All,
                         DesksCloseAllTest,
                         ValuesIn(kDeskCountOnly),
                         GetDeskCountOnlyTestSuffix);
INSTANTIATE_TEST_SUITE_P(All,
                         PerDeskShelfTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "PerDeskShelfOn"
                                             : "PerDeskShelfOff";
                         });
INSTANTIATE_TEST_SUITE_P(
    All,
    DeskBarTest,
    testing::Combine(testing::Bool(),  // use touch gestures
                     testing::Bool(),  // use 16 desks
                     testing::Bool(),  // use desk profiles
                     testing::Values(DeskBarViewBase::Type::kDeskButton,
                                     DeskBarViewBase::Type::kOverview)),
    [](const testing::TestParamInfo<DeskBarTest::ParamType>& info) {
      DesksTestParams params;
      bool use_desk_profiles;
      DeskBarViewBase::Type bar_type;
      std::tie(params.use_touch_gestures, params.use_16_desks,
               use_desk_profiles, bar_type) = info.param;
      std::string result = GetTestSuffix(params);
      std::string bar_type_str;
      switch (bar_type) {
        case DeskBarViewBase::Type::kDeskButton:
          bar_type_str = "DeskButtonBar";
          break;
        case DeskBarViewBase::Type::kOverview:
          bar_type_str = "OverviewBar";
          break;
      }

      std::string desk_profiles_str =
          use_desk_profiles ? "DeskProfiles" : "NoDeskProfiles";
      return base::StringPrintf("%s_%s_%s", result.c_str(),
                                bar_type_str.c_str(),
                                desk_profiles_str.c_str());
    });

INSTANTIATE_TEST_SUITE_P(
    All,
    DeskButtonTest,
    ValuesIn(kDeskButtonTestParamCombinations),
    [](const testing::TestParamInfo<DeskButtonTestParams>& info) {
      std::stringstream sstream;
      sstream << "Shelf" << info.param.alignment;
      return sstream.str();
    });

}  // namespace

}  // namespace ash
