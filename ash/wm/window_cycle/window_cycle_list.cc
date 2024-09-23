// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle/window_cycle_list.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_cycle/window_cycle_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

constexpr char kSameAppWindowCycleSkippedWindowsHistogramName[] =
    "Ash.WindowCycleController.SameApp.SkippedWindows";

constexpr char kEnterWindowCyclePresentationHistogramName[] =
    "Ash.WindowCycleController.Enter.PresentationTime";

constexpr base::TimeDelta kEnterPresentationMaxLatency = base::Seconds(2);

bool g_disable_initial_delay = false;

// Delay before the UI fade in animation starts. This is so users can switch
// quickly between windows without bringing up the UI.
constexpr base::TimeDelta kShowDelayDuration = base::Milliseconds(150);

// The alt-tab cycler widget is not activatable (except when ChromeVox is on),
// so we use WindowTargeter to send input events to the widget.
class CustomWindowTargeter : public aura::WindowTargeter {
 public:
  explicit CustomWindowTargeter(aura::Window* tab_cycler)
      : tab_cycler_(tab_cycler) {}
  CustomWindowTargeter(const CustomWindowTargeter&) = delete;
  CustomWindowTargeter& operator=(const CustomWindowTargeter&) = delete;
  ~CustomWindowTargeter() override = default;

  // aura::WindowTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    if (event->IsLocatedEvent())
      return aura::WindowTargeter::FindTargetForEvent(root, event);
    return tab_cycler_;
  }

 private:
  raw_ptr<aura::Window> tab_cycler_;
};

gfx::Point ConvertEventToScreen(const ui::LocatedEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* event_root = target->GetRootWindow();
  gfx::Point event_screen_point = event->root_location();
  wm::ConvertPointToScreen(event_root, &event_screen_point);
  return event_screen_point;
}

bool IsWindowInSnapGroup(aura::Window* window) {
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  return snap_group_controller &&
         snap_group_controller->GetSnapGroupForGivenWindow(window);
}

// Returns the mru window with the existence of snap groups. If a snap group is
// at the beginning of the window cycle list, we need to check the activation
// order of the two windows in the snap group since the window list has been
// reordered to reflect the actual window layout with the primarily snapped
// window comes before the secondarily snapped window, which makes the front
// window in the window lists not guaranteed to be the mru window.
aura::Window* GetMruWindow(
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows) {
  aura::Window* front_window = windows.front();
  if (IsWindowInSnapGroup(front_window)) {
    SnapGroup* snap_group =
        SnapGroupController::Get()->GetSnapGroupForGivenWindow(front_window);
    aura::Window* window1 = snap_group->window1();
    aura::Window* window2 = snap_group->window2();
    CHECK_EQ(front_window, window1);
    if (window_util::IsStackedBelow(window1, window2)) {
      return window2;
    }
  }

  return front_window;
}

}  // namespace

WindowCycleList::WindowCycleList(const WindowList& windows, bool same_app_only)
    : windows_(windows), same_app_only_(same_app_only) {
  if (!ShouldShowUi())
    Shell::Get()->mru_window_tracker()->SetIgnoreActivations(true);

  active_window_before_window_cycle_ = window_util::GetActiveWindow();

  if (same_app_only) {
    MakeSameAppOnly();
  }

  for (aura::Window* window : windows_) {
    window->AddObserver(this);
  }

  if (ShouldShowUi()) {
    // Disable the tab scrubber so three finger scrolling doesn't scrub tabs as
    // well.
    Shell::Get()->shell_delegate()->SetTabScrubberChromeOSEnabled(false);

    if (g_disable_initial_delay) {
      InitWindowCycleView();
    } else {
      show_ui_timer_.Start(FROM_HERE, kShowDelayDuration, this,
                           &WindowCycleList::InitWindowCycleView);
    }
  }
}

WindowCycleList::~WindowCycleList() {
  if (!ShouldShowUi())
    Shell::Get()->mru_window_tracker()->SetIgnoreActivations(false);

  Shell::Get()->shell_delegate()->SetTabScrubberChromeOSEnabled(true);

  for (aura::Window* window : windows_) {
    window->RemoveObserver(this);
  }

  if (cycle_ui_widget_)
    cycle_ui_widget_->Close();

  // Store the target window before |cycle_view_| is destroyed.
  aura::Window* target_window = nullptr;

  // |this| is responsible for notifying |cycle_view_| when windows are
  // destroyed. Since |this| is going away, clobber |cycle_view_|. Otherwise
  // there will be a race where a window closes after now but before the
  // Widget::Close() call above actually destroys |cycle_view_|. See
  // crbug.com/681207
  if (cycle_view_) {
    target_window = GetTargetWindow();
    cycle_view_->DestroyContents();
  }

  // While the cycler widget is shown, the windows listed in the cycler is
  // marked as force-visible and don't contribute to occlusion. In order to
  // work occlusion calculation properly, we need to activate a window after
  // the widget has been destroyed. See b/138914552.
  if (!windows_.empty() && user_did_accept_) {
    if (!target_window)
      target_window = windows_[current_index_];
    MaybeReportNonSameAppSkippedWindows(target_window);
    SelectWindow(target_window);
  }
  Shell::Get()->frame_throttling_controller()->EndThrottling();
}

aura::Window* WindowCycleList::GetTargetWindow() {
  return cycle_view_->target_window();
}

void WindowCycleList::ReplaceWindows(const WindowList& windows) {
  RemoveAllWindows();
  windows_ = windows;

  if (same_app_only_) {
    MakeSameAppOnly();
  }

  for (aura::Window* new_window : windows_) {
    new_window->AddObserver(this);
  }

  if (cycle_view_)
    cycle_view_->UpdateWindows(windows_);
}

void WindowCycleList::Step(WindowCyclingDirection direction,
                           bool starting_alt_tab_or_switching_mode) {
  if (windows_.empty())
    return;

  last_cycling_direction_ = direction;

  // If the position of the window cycle list is out-of-sync with the currently
  // selected item, scroll to the selected item and then step.
  if (cycle_view_) {
    aura::Window* selected_window = GetTargetWindow();
    if (selected_window)
      Scroll(GetIndexOfWindow(selected_window) - current_index_);
  }

  int offset = direction == WindowCyclingDirection::kForward ? 1 : -1;
  // When the window focus should be reset and the first window in the MRU
  // cycle list is not the latest active one before entering alt-tab, focus
  // it instead of the second window. This occurs when the user is in overview
  // mode, all windows are minimized, or all windows are in other desks.
  //
  // Note:
  // Simply checking the active status of the first window won't work
  // because when the ChromeVox is enabled, the widget is activatable, so the
  // first window in MRU becomes inactive.
  if (starting_alt_tab_or_switching_mode &&
      direction == WindowCyclingDirection::kForward &&
      (active_window_before_window_cycle_ != windows_[0])) {
    offset = 0;
    current_index_ = 0;
  }

  SetFocusedWindow(windows_[GetOffsettedWindowIndex(offset)]);
  Scroll(offset);
}

void WindowCycleList::Drag(float delta_x) {
  DCHECK(cycle_view_);
  cycle_view_->Drag(delta_x);
}

void WindowCycleList::StartFling(float velocity_x) {
  DCHECK(cycle_view_);
  cycle_view_->StartFling(velocity_x);
}

void WindowCycleList::SetFocusedWindow(aura::Window* window) {
  if (windows_.empty())
    return;

  if (ShouldShowUi() && cycle_view_)
    cycle_view_->SetTargetWindow(windows_[GetIndexOfWindow(window)]);
}

void WindowCycleList::SetFocusTabSlider(bool focus) {
  DCHECK(cycle_view_);
  cycle_view_->SetFocusTabSlider(focus);
}

bool WindowCycleList::IsTabSliderFocused() const {
  DCHECK(cycle_view_);
  return cycle_view_->IsTabSliderFocused();
}

bool WindowCycleList::IsEventInCycleView(const ui::LocatedEvent* event) const {
  return cycle_view_ &&
         cycle_view_->GetBoundsInScreen().Contains(ConvertEventToScreen(event));
}

aura::Window* WindowCycleList::GetWindowAtPoint(const ui::LocatedEvent* event) {
  return cycle_view_
             ? cycle_view_->GetWindowAtPoint(ConvertEventToScreen(event))
             : nullptr;
}

bool WindowCycleList::IsEventInTabSliderContainer(
    const ui::LocatedEvent* event) const {
  return cycle_view_ &&
         cycle_view_->IsEventInTabSliderContainer(ConvertEventToScreen(event));
}

bool WindowCycleList::ShouldShowUi() const {
  // Show alt-tab when there are at least two windows to pick from alt-tab, or
  // when there is at least a window to switch to by switching to the different
  // mode.
  if (!Shell::Get()
           ->window_cycle_controller()
           ->IsInteractiveAltTabModeAllowed()) {
    return windows_.size() > 1u;
  }

  int total_window_in_all_desks = GetNumberOfWindowsAllDesks();
  return windows_.size() > 1u ||
         (windows_.size() <= 1u &&
          static_cast<size_t>(total_window_in_all_desks) > windows_.size());
}

void WindowCycleList::OnModePrefsChanged() {
  if (cycle_view_)
    cycle_view_->OnModePrefsChanged();
}

// static
void WindowCycleList::SetDisableInitialDelayForTesting(bool disabled) {
  g_disable_initial_delay = disabled;
}

void WindowCycleList::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);

  WindowList::iterator i = base::ranges::find(windows_, window);
  CHECK(i != windows_.end());
  int removed_index = static_cast<int>(i - windows_.begin());
  windows_.erase(i);
  if (current_index_ > removed_index ||
      current_index_ == static_cast<int>(windows_.size())) {
    current_index_--;
  }

  // Reset |active_window_before_window_cycle_| to avoid a dangling pointer.
  if (window == active_window_before_window_cycle_)
    active_window_before_window_cycle_ = nullptr;

  if (cycle_view_) {
    auto* new_target_window =
        windows_.empty() ? nullptr : windows_[current_index_].get();
    cycle_view_->HandleWindowDestruction(window, new_target_window);

    if (windows_.empty()) {
      // This deletes us.
      Shell::Get()->window_cycle_controller()->CancelCycling();
      return;
    }
  }
}

void WindowCycleList::OnDisplayMetricsChanged(const display::Display& display,
                                              uint32_t changed_metrics) {
  if (cycle_ui_widget_ &&
      display.id() ==
          display::Screen::GetScreen()
              ->GetDisplayNearestWindow(cycle_ui_widget_->GetNativeWindow())
              .id() &&
      (changed_metrics & (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION))) {
    Shell::Get()->window_cycle_controller()->CancelCycling();
    // |this| is deleted.
    return;
  }
}

void WindowCycleList::RemoveAllWindows() {
  for (aura::Window* window : windows_) {
    window->RemoveObserver(this);

    if (cycle_view_)
      cycle_view_->HandleWindowDestruction(window, nullptr);
  }

  windows_.clear();
  current_index_ = 0;
  window_selected_ = false;
}

void WindowCycleList::InitWindowCycleView() {
  if (cycle_view_)
    return;

  TRACE_EVENT0("ui", "WindowCycleList::InitWindowCycleView");

  aura::Window* root_window = Shell::GetRootWindowForNewWindows();

  auto presentation_time_recorder = CreatePresentationTimeHistogramRecorder(
      root_window->layer()->GetCompositor(),
      kEnterWindowCyclePresentationHistogramName, "",
      ui::PresentationTimeRecorder::BucketParams::CreateWithMaximum(
          kEnterPresentationMaxLatency));
  presentation_time_recorder->RequestNext();

  // Close any tray bubbles that are opened before creating the cycle view.
  StatusAreaWidget* status_area_widget =
      RootWindowController::ForWindow(root_window)->GetStatusAreaWidget();
  for (TrayBackgroundView* tray_button : status_area_widget->tray_buttons()) {
    if (tray_button->is_active())
      tray_button->CloseBubble();
  }

  cycle_view_ = new WindowCycleView(root_window, windows_, same_app_only_);
  const bool is_interactive_alt_tab_mode_allowed =
      Shell::Get()->window_cycle_controller()->IsInteractiveAltTabModeAllowed();
  DCHECK(!windows_.empty() || is_interactive_alt_tab_mode_allowed);

  // Only set target window and scroll to the window when alt-tab is not empty.
  if (!windows_.empty()) {
    DCHECK(static_cast<int>(windows_.size()) > current_index_);
    cycle_view_->SetTargetWindow(windows_[current_index_]);
    cycle_view_->ScrollToWindow(windows_[current_index_]);
  }

  // We need to activate the widget if ChromeVox is enabled as ChromeVox
  // relies on activation.
  const bool spoken_feedback_enabled =
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled();

  views::Widget* widget = new views::Widget();
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = cycle_view_.get();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.layer_type = ui::LAYER_NOT_DRAWN;

  // Don't let the alt-tab cycler be activatable. This lets the currently
  // activated window continue to be in the foreground. This may affect
  // things such as video automatically pausing/playing.
  if (!spoken_feedback_enabled)
    params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = true;
  params.name = "WindowCycleList (Alt+Tab)";
  // TODO(estade): make sure nothing untoward happens when the lock screen
  // or a system modal dialog is shown.
  params.parent = root_window->GetChildById(kShellWindowId_OverlayContainer);
  params.bounds = cycle_view_->GetTargetBounds();

  widget->Init(std::move(params));
  widget->Show();
  cycle_view_->FadeInLayer();
  cycle_ui_widget_ = widget;

  // Since this window is not activated, grab events.
  if (!spoken_feedback_enabled) {
    window_targeter_ = std::make_unique<aura::ScopedWindowTargeter>(
        widget->GetNativeWindow()->GetRootWindow(),
        std::make_unique<CustomWindowTargeter>(widget->GetNativeWindow()));
  }
  // Close the app list, if it's open in clamshell mode.
  if (!display::Screen::GetScreen()->InTabletMode()) {
    Shell::Get()->app_list_controller()->DismissAppList();
  }

  Shell::Get()->frame_throttling_controller()->StartThrottling(windows_);
}

void WindowCycleList::SelectWindow(aura::Window* window) {
  // If the list has only one window, the window can be selected twice (in
  // Scroll() and the destructor). This causes ARC PIP windows to be restored
  // twice, which leads to a wrong window state.
  if (window_selected_)
    return;

  if (window->GetProperty(kPipOriginalWindowKey)) {
    window_util::ExpandArcPipWindow();
  } else {
    window->Show();
    WindowState::Get(window)->Activate();
  }

  window_selected_ = true;
}

void WindowCycleList::Scroll(int offset) {
  if (windows_.size() == 1)
    SelectWindow(windows_[0]);

  if (!ShouldShowUi()) {
    // When there is only one window, we should give feedback to the user. If
    // the window is minimized, we should also show it.
    if (windows_.size() == 1)
      wm::AnimateWindow(windows_[0], wm::WINDOW_ANIMATION_TYPE_BOUNCE);
    return;
  }

  DCHECK(static_cast<size_t>(current_index_) < windows_.size());
  current_index_ = GetOffsettedWindowIndex(offset);

  if (current_index_ > 1)
    InitWindowCycleView();

  // The windows should not shift position when selecting when there's enough
  // room to display all windows.
  if (cycle_view_ && cycle_view_->CalculatePreferredSize({}).width() ==
                         cycle_view_->CalculateMaxWidth()) {
    cycle_view_->ScrollToWindow(windows_[current_index_]);
  }
}

void WindowCycleList::MakeSameAppOnly() {
  CHECK(same_app_only_);
  if (windows_.size() < 2) {
    return;
  }

  const std::string* const mru_window_app_id =
      GetMruWindow(windows_)->GetProperty(kAppIDKey);
  if (!mru_window_app_id) {
    return;
  }
  windows_.erase(
      base::ranges::remove_if(windows_.begin(), windows_.end(),
                              [&mru_window_app_id](aura::Window* window) {
                                const auto* const app_id =
                                    window->GetProperty(kAppIDKey);
                                return !app_id || *app_id != *mru_window_app_id;
                              }),
      windows_.end());
}

int WindowCycleList::GetOffsettedWindowIndex(int offset) const {
  DCHECK(!windows_.empty());

  const int offsetted_index =
      (current_index_ + offset + windows_.size()) % windows_.size();
  DCHECK(windows_[offsetted_index]);

  return offsetted_index;
}

int WindowCycleList::GetIndexOfWindow(aura::Window* window) const {
  auto target_window = base::ranges::find(windows_, window);
  DCHECK(target_window != windows_.end());
  return std::distance(windows_.begin(), target_window);
}

int WindowCycleList::GetNumberOfWindowsAllDesks() const {
  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();

  // If alt-tab mode is not available, the alt-tab defaults to all-desks mode
  // and can obtain the number of all windows easily from `windows_.size()`.
  CHECK(window_cycle_controller->IsInteractiveAltTabModeAllowed());
  return window_cycle_controller->BuildWindowListForWindowCycling(kAllDesks)
      .size();
}

void WindowCycleList::MaybeReportNonSameAppSkippedWindows(
    aura::Window* target_window) const {
  if (!same_app_only_ || windows_.size() < 2 || current_index_ == 0) {
    return;
  }

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  const bool per_active_desk = window_cycle_controller->IsAltTabPerActiveDesk()
                                   ? kActiveDesk
                                   : kAllDesks;
  const WindowList original_windows =
      window_cycle_controller->BuildWindowListForWindowCycling(
          per_active_desk ? kActiveDesk : kAllDesks);

  const std::string* const mru_window_app_id =
      target_window->GetProperty(kAppIDKey);
  if (!mru_window_app_id) {
    return;
  }

  // The window at index 0 is the window cycling started on. It can't be a
  // skipped window, so start at index 1.
  int start = 1;
  int increment = 1;

  // If we're cycling backwards, start from the end and work backwards.
  if (last_cycling_direction_ == WindowCyclingDirection::kBackward) {
    start = original_windows.size() - 1;
    increment = -1;
  }

  // Count up the skipped windows between the starting window and the chosen
  // window.
  int skipped_windows = 0;
  aura::Window* current_window = nullptr;
  for (int i = start; i >= 0 && i < static_cast<int>(original_windows.size()) &&
                      current_window != target_window;
       i += increment) {
    current_window = original_windows[i];
    const auto* const app_id = current_window->GetProperty(kAppIDKey);
    if (!app_id || *app_id != *mru_window_app_id) {
      skipped_windows++;
    }
  }
  // Make sure looping stopped because we found the window.
  DCHECK_EQ(current_window, target_window);

  base::UmaHistogramCounts100(kSameAppWindowCycleSkippedWindowsHistogramName,
                              skipped_windows);
}

}  // namespace ash
