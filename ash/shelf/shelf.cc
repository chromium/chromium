// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf.h"

#include <memory>

#include "ash/animation/animation_change_type.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/metrics/login_unlock_throughput_recorder.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/desk_button_widget.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/login_shelf_widget.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/work_area_insets.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

bool IsAppListBackground(ShelfBackgroundType background_type) {
  switch (background_type) {
    case ShelfBackgroundType::kHomeLauncher:
      return true;
    case ShelfBackgroundType::kDefaultBg:
    case ShelfBackgroundType::kMaximized:
    case ShelfBackgroundType::kOobe:
    case ShelfBackgroundType::kLogin:
    case ShelfBackgroundType::kLoginNonBlurredWallpaper:
    case ShelfBackgroundType::kOverview:
    case ShelfBackgroundType::kInApp:
      return false;
  }
}

bool IsBottomAlignment(ShelfAlignment alignment) {
  return alignment == ShelfAlignment::kBottom ||
         alignment == ShelfAlignment::kBottomLocked;
}

}  // namespace

// Records smoothness of bounds animations for the HotseatWidget.
class HotseatWidgetAnimationMetricsReporter {
 public:
  // The different kinds of hotseat elements.
  enum class HotseatElementType {
    // The Hotseat Widget.
    kWidget,
    // The Hotseat Widget's translucent background.
    kTranslucentBackground
  };

  explicit HotseatWidgetAnimationMetricsReporter(
      HotseatElementType hotseat_element)
      : hotseat_element_(hotseat_element) {}
  ~HotseatWidgetAnimationMetricsReporter() = default;

  void ReportSmoothness(HotseatState target_state, int smoothness) {
    switch (target_state) {
      case HotseatState::kShownClamshell:
      case HotseatState::kShownHomeLauncher:
        if (hotseat_element_ == HotseatElementType::kWidget) {
          UMA_HISTOGRAM_PERCENTAGE(
              "Ash.HotseatWidgetAnimation.Widget.AnimationSmoothness."
              "TransitionToShownHotseat",
              smoothness);
        } else {
          UMA_HISTOGRAM_PERCENTAGE(
              "Ash.HotseatWidgetAnimation.TranslucentBackground."
              "AnimationSmoothness.TransitionToShownHotseat",
              smoothness);
        }
        break;
      case HotseatState::kExtended:
        if (hotseat_element_ == HotseatElementType::kWidget) {
          UMA_HISTOGRAM_PERCENTAGE(
              "Ash.HotseatWidgetAnimation.Widget.AnimationSmoothness."
              "TransitionToExtendedHotseat",
              smoothness);
        } else {
          UMA_HISTOGRAM_PERCENTAGE(
              "Ash.HotseatWidgetAnimation.TranslucentBackground."
              "AnimationSmoothness.TransitionToExtendedHotseat",
              smoothness);
        }
        break;
      case HotseatState::kHidden:
        if (hotseat_element_ == HotseatElementType::kWidget) {
          UMA_HISTOGRAM_PERCENTAGE(
              "Ash.HotseatWidgetAnimation.Widget.AnimationSmoothness."
              "TransitionToHiddenHotseat",
              smoothness);
        } else {
          UMA_HISTOGRAM_PERCENTAGE(
              "Ash.HotseatWidgetAnimation.TranslucentBackground."
              "AnimationSmoothness.TransitionToHiddenHotseat",
              smoothness);
        }
        break;
      case HotseatState::kNone:
        NOTREACHED();
    }
  }

  metrics_util::ReportCallback GetReportCallback(HotseatState target_state) {
    DCHECK_NE(target_state, HotseatState::kNone);
    return metrics_util::ForSmoothnessV3(base::BindRepeating(
        &HotseatWidgetAnimationMetricsReporter::ReportSmoothness,
        weak_ptr_factory_.GetWeakPtr(), target_state));
  }

 private:
  // The element that is reporting an animation.
  const HotseatElementType hotseat_element_;

  base::WeakPtrFactory<HotseatWidgetAnimationMetricsReporter> weak_ptr_factory_{
      this};
};

// An animation metrics reporter for the shelf navigation widget.
class ASH_EXPORT NavigationWidgetAnimationMetricsReporter {
 public:
  NavigationWidgetAnimationMetricsReporter() = default;

  ~NavigationWidgetAnimationMetricsReporter() = default;

  NavigationWidgetAnimationMetricsReporter(
      const NavigationWidgetAnimationMetricsReporter&) = delete;
  NavigationWidgetAnimationMetricsReporter& operator=(
      const NavigationWidgetAnimationMetricsReporter&) = delete;

  void ReportSmoothness(HotseatState target_hotseat_state, int smoothness) {
    switch (target_hotseat_state) {
      case HotseatState::kShownClamshell:
      case HotseatState::kShownHomeLauncher:
        UMA_HISTOGRAM_PERCENTAGE(
            "Ash.NavigationWidget.Widget.AnimationSmoothness."
            "TransitionToShownHotseat",
            smoothness);
        break;
      case HotseatState::kExtended:
        UMA_HISTOGRAM_PERCENTAGE(
            "Ash.NavigationWidget.Widget.AnimationSmoothness."
            "TransitionToExtendedHotseat",
            smoothness);
        break;
      case HotseatState::kHidden:
        UMA_HISTOGRAM_PERCENTAGE(
            "Ash.NavigationWidget.Widget.AnimationSmoothness."
            "TransitionToHiddenHotseat",
            smoothness);
        break;
      case HotseatState::kNone:
        NOTREACHED();
    }
  }

  metrics_util::ReportCallback GetReportCallback(
      HotseatState target_hotseat_state) {
    DCHECK_NE(target_hotseat_state, HotseatState::kNone);
    return metrics_util::ForSmoothnessV3(base::BindRepeating(
        &NavigationWidgetAnimationMetricsReporter::ReportSmoothness,
        weak_ptr_factory_.GetWeakPtr(), target_hotseat_state));
  }

 private:
  base::WeakPtrFactory<NavigationWidgetAnimationMetricsReporter>
      weak_ptr_factory_{this};
};

// Shelf::AutoHideEventHandler -----------------------------------------------

// Forwards mouse and gesture events to ShelfLayoutManager for auto-hide.
class Shelf::AutoHideEventHandler : public ui::EventHandler {
 public:
  explicit AutoHideEventHandler(Shelf* shelf) : shelf_(shelf) {
    Shell::Get()->AddPreTargetHandler(this);
  }

  AutoHideEventHandler(const AutoHideEventHandler&) = delete;
  AutoHideEventHandler& operator=(const AutoHideEventHandler&) = delete;

  ~AutoHideEventHandler() override {
    Shell::Get()->RemovePreTargetHandler(this);
  }

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    shelf_->shelf_layout_manager()->UpdateAutoHideForMouseEvent(
        event, static_cast<aura::Window*>(event->target()));
  }
  void OnGestureEvent(ui::GestureEvent* event) override {
    shelf_->shelf_layout_manager()->ProcessGestureEventOfAutoHideShelf(
        event, static_cast<aura::Window*>(event->target()));
  }
  void OnTouchEvent(ui::TouchEvent* event) override {
    if (shelf_->auto_hide_behavior() != ShelfAutoHideBehavior::kAlways)
      return;

    // The event target should be the shelf widget or the hotseat widget.
    if (!shelf_->shelf_layout_manager()->IsShelfWindow(
            static_cast<aura::Window*>(event->target()))) {
      return;
    }

    // The touch-pressing event may hide the shelf. Lock the shelf's auto hide
    // state to give the shelf a chance to handle the touch event before it
    // being hidden.
    ShelfLayoutManager* shelf_layout_manager = shelf_->shelf_layout_manager();
    if (event->type() == ui::EventType::kTouchPressed && shelf_->IsVisible()) {
      shelf_layout_manager->LockAutoHideState(true);
    } else if (event->type() == ui::EventType::kTouchReleased ||
               event->type() == ui::EventType::kTouchCancelled) {
      // Unlock auto hide (and eventually recompute auto hide state).
      shelf_layout_manager->LockAutoHideState(false);
    }
  }

 private:
  raw_ptr<Shelf> shelf_;
};

// Shelf::AutoDimEventHandler -----------------------------------------------

// Handles mouse and touch events and determines whether ShelfLayoutManager
// should update shelf opacity for auto-dimming.
class Shelf::AutoDimEventHandler : public ui::EventHandler,
                                   public ShelfObserver {
 public:
  explicit AutoDimEventHandler(Shelf* shelf) : shelf_(shelf) {
    Shell::Get()->AddPreTargetHandler(this);
    shelf_observation_.Observe(shelf_.get());
    UndimShelf();
  }

  AutoDimEventHandler(const AutoDimEventHandler&) = delete;
  AutoDimEventHandler& operator=(const AutoDimEventHandler&) = delete;

  ~AutoDimEventHandler() override {
    Shell::Get()->RemovePreTargetHandler(this);
  }

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (shelf_->shelf_layout_manager()->IsShelfWindow(
            static_cast<aura::Window*>(event->target()))) {
      UndimShelf();
    }
  }

  void OnTouchEvent(ui::TouchEvent* event) override {
    if (shelf_->shelf_layout_manager()->IsShelfWindow(
            static_cast<aura::Window*>(event->target()))) {
      UndimShelf();
    }
  }

  void StartDimShelfTimer() {
    dim_shelf_timer_.Start(
        FROM_HERE, kDimDelay,
        base::BindOnce(&AutoDimEventHandler::DimShelf, base::Unretained(this)));
  }

  void DimShelf() {
    // Attempt to dim the shelf. Stop the |dim_shelf_timer_| if successful.
    if (shelf_->shelf_layout_manager()->SetDimmed(true))
      dim_shelf_timer_.Stop();
  }

  // Sets shelf as active and sets timer to mark shelf as inactive.
  void UndimShelf() {
    shelf_->shelf_layout_manager()->SetDimmed(false);
    StartDimShelfTimer();
  }

  bool HasDimShelfTimer() { return dim_shelf_timer_.IsRunning(); }

  // ShelfObserver:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override {
    // Shelf should be undimmed when it is shown.
    if (new_state == ShelfAutoHideState::SHELF_AUTO_HIDE_SHOWN)
      UndimShelf();
  }

  // ShelfObserver:
  void OnShelfVisibilityStateChanged(ShelfVisibilityState new_state) override {
    // Shelf should be undimmed when it is shown.
    if (new_state != ShelfVisibilityState::SHELF_HIDDEN)
      UndimShelf();
  }

 private:
  // Unowned pointer to the shelf that owns this event handler.
  raw_ptr<Shelf> shelf_;
  // OneShotTimer that dims shelf due to inactivity.
  base::OneShotTimer dim_shelf_timer_;
  // An observer that notifies the AutoDimHandler that shelf visibility has
  // changed.
  base::ScopedObservation<Shelf, ShelfObserver> shelf_observation_{this};

  // Delay before dimming the shelf.
  const base::TimeDelta kDimDelay = base::Seconds(5);
};

// Shelf::ScopedDisableAutoHide ----------------------------------------------

Shelf::ScopedDisableAutoHide::ScopedDisableAutoHide(Shelf* shelf)
    : shelf_(shelf->GetWeakPtr()) {
  CHECK(shelf);

  ++shelf_->disable_auto_hide_;
  if (shelf_->disable_auto_hide_ == 1) {
    shelf_->UpdateVisibilityState();
  }
}

Shelf::ScopedDisableAutoHide::~ScopedDisableAutoHide() {
  if (!shelf_) {
    return;
  }

  --shelf_->disable_auto_hide_;
  CHECK_GE(shelf_->disable_auto_hide_, 0);
  if (shelf_->disable_auto_hide_ == 0) {
    shelf_->UpdateVisibilityState();
  }
}

// Shelf ---------------------------------------------------------------------

Shelf::Shelf()
    : shelf_locking_manager_(this),
      shelf_focus_cycler_(std::make_unique<ShelfFocusCycler>(this)),
      tooltip_(std::make_unique<ShelfTooltipManager>(this)) {}

Shelf::~Shelf() = default;

// static
Shelf* Shelf::ForWindow(aura::Window* window) {
  return RootWindowController::ForWindow(window)->shelf();
}

// static
void Shelf::LaunchShelfItem(int item_index) {
  const int item_count = ShelfModel::Get()->item_count();

  // A negative argument will launch the last app. A positive argument will
  // launch the app at the corresponding index, unless it's higher than the
  // total number of apps, in which case we do nothing.
  if (item_index >= item_count)
    return;

  const int found_index = item_index >= 0 ? item_index : item_count - 1;

  // Set this one as active (or advance to the next item of its kind).
  ActivateShelfItem(found_index);
}

// static
void Shelf::ActivateShelfItem(int item_index) {
  ActivateShelfItemOnDisplay(item_index, display::kInvalidDisplayId);
}

// static
void Shelf::ActivateShelfItemOnDisplay(int item_index, int64_t display_id) {
  const ShelfModel* shelf_model = ShelfModel::Get();
  const ShelfItem& item = shelf_model->items()[item_index];
  ShelfItemDelegate* item_delegate = shelf_model->GetShelfItemDelegate(item.id);
  std::unique_ptr<ui::Event> event = std::make_unique<ui::KeyEvent>(
      ui::EventType::kKeyReleased, ui::VKEY_UNKNOWN, ui::EF_NONE);
  item_delegate->ItemSelected(std::move(event), display_id, LAUNCH_FROM_SHELF,
                              base::DoNothing(), base::NullCallback());
}

// static
void Shelf::UpdateShelfVisibility() {
  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    Shelf::ForWindow(root)->UpdateVisibilityState();
  }
}

void Shelf::CreateNavigationWidget(aura::Window* container) {
  DCHECK(container);
  DCHECK(!navigation_widget_);
  navigation_widget_ = std::make_unique<ShelfNavigationWidget>(
      this, hotseat_widget()->GetShelfView());
  navigation_widget_->Initialize(container);
  navigation_widget_metrics_reporter_ =
      std::make_unique<NavigationWidgetAnimationMetricsReporter>();
}

void Shelf::CreateDeskButtonWidget(aura::Window* container) {
  CHECK(container);
  CHECK(!desk_button_widget_);
  CHECK(ash::features::IsDeskButtonEnabled());

  desk_button_widget_ = std::make_unique<DeskButtonWidget>(this);
  desk_button_widget_->Initialize(container);
}

void Shelf::CreateHotseatWidget(aura::Window* container) {
  DCHECK(container);
  DCHECK(!hotseat_widget_);
  hotseat_widget_ = std::make_unique<HotseatWidget>();
  translucent_background_metrics_reporter_ =
      std::make_unique<HotseatWidgetAnimationMetricsReporter>(
          HotseatWidgetAnimationMetricsReporter::HotseatElementType::
              kTranslucentBackground);
  hotseat_widget_->Initialize(container, this);
  shelf_widget_->RegisterHotseatWidget(hotseat_widget());
  hotseat_transition_metrics_reporter_ =
      std::make_unique<HotseatWidgetAnimationMetricsReporter>(
          HotseatWidgetAnimationMetricsReporter::HotseatElementType::kWidget);
}

void Shelf::CreateStatusAreaWidget(aura::Window* shelf_container) {
  DCHECK(shelf_container);
  DCHECK(!status_area_widget_);
  status_area_widget_ =
      std::make_unique<StatusAreaWidget>(shelf_container, this);
  status_area_widget_->Initialize();
}

void Shelf::CreateShelfWidget(aura::Window* root) {
  DCHECK(!shelf_widget_);
  aura::Window* shelf_container =
      root->GetChildById(kShellWindowId_ShelfContainer);
  shelf_widget_ = std::make_unique<ShelfWidget>(this);

  DCHECK(!shelf_layout_manager_);
  shelf_layout_manager_ = shelf_widget_->shelf_layout_manager();
  shelf_layout_manager_->AddObserver(this);

  // Create the various shelf components.
  CreateHotseatWidget(shelf_container);
  CreateNavigationWidget(shelf_container);
  if (ash::features::IsDeskButtonEnabled()) {
    CreateDeskButtonWidget(shelf_container);
  }
  login_shelf_widget_ =
      std::make_unique<LoginShelfWidget>(/*shelf=*/this, shelf_container);

  // Must occur after |shelf_widget_| is constructed because the system tray
  // constructors call back into Shelf::shelf_widget().
  CreateStatusAreaWidget(shelf_container);
  shelf_widget_->Initialize(shelf_container);
  shelf_widget_->GetNativeWindow()->parent()->StackChildAtBottom(
      shelf_widget_->GetNativeWindow());

  // The Hotseat should be above everything in the shelf.
  hotseat_widget()->StackAtTop();
}

void Shelf::ShutdownShelfWidget() {
  for (auto& observer : observers_)
    observer.OnShelfShuttingDown();

  // Remove observers prior to destroying child widgets, this prevents
  // activation changes from triggering during shutdown, see
  // https://crbug.com/1307898.
  shelf_widget_->Shutdown();

  // The contents view of the hotseat widget may rely on the status area widget.
  // So do explicit destruction here.
  hotseat_widget_.reset();
  status_area_widget_.reset();
  navigation_widget_.reset();
  login_shelf_widget_.reset();
}

void Shelf::DestroyShelfWidget() {
  DCHECK(shelf_widget_);
  shelf_widget_.reset();
}

bool Shelf::IsVisible() const {
  return shelf_layout_manager_->IsVisible();
}

const aura::Window* Shelf::GetWindow() const {
  return shelf_widget_ ? shelf_widget_->GetNativeWindow() : nullptr;
}

aura::Window* Shelf::GetWindow() {
  return const_cast<aura::Window*>(const_cast<const Shelf*>(this)->GetWindow());
}

void Shelf::SetAlignment(ShelfAlignment alignment) {
  if (!shelf_widget_)
    return;

  if (alignment_ == alignment)
    return;

  if (shelf_locking_manager_.is_locked() &&
      alignment != ShelfAlignment::kBottomLocked) {
    shelf_locking_manager_.set_in_session_alignment(alignment);
    return;
  }

  bool needs_relayout =
      !IsBottomAlignment(alignment_) || !IsBottomAlignment(alignment);

  ShelfAlignment old_alignment = alignment_;
  alignment_ = alignment;
  tooltip_->Close();
  if (needs_relayout) {
    shelf_layout_manager_->HandleShelfAlignmentChange();
    Shell::Get()->NotifyShelfAlignmentChanged(GetWindow()->GetRootWindow(),
                                              old_alignment);
  }
}

bool IsHorizontalAlignment(ShelfAlignment alignment) {
  switch (alignment) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return true;
    case ShelfAlignment::kLeft:
    case ShelfAlignment::kRight:
      return false;
  }
  NOTREACHED();
}

bool Shelf::IsHorizontalAlignment() const {
  return ash::IsHorizontalAlignment(alignment_);
}

void Shelf::SetAutoHideBehavior(ShelfAutoHideBehavior auto_hide_behavior) {
  DCHECK(shelf_layout_manager_);

  if (auto_hide_behavior_ == auto_hide_behavior)
    return;

  auto_hide_behavior_ = auto_hide_behavior;

  for (auto& observer : observers_)
    observer.OnShelfAutoHideBehaviorChanged();
}

ShelfAutoHideState Shelf::GetAutoHideState() const {
  return shelf_layout_manager_->auto_hide_state();
}

void Shelf::UpdateAutoHideState() {
  shelf_layout_manager_->UpdateAutoHideState();
}

ShelfBackgroundType Shelf::GetBackgroundType() const {
  return shelf_layout_manager_ ? shelf_layout_manager_->shelf_background_type()
                               : ShelfBackgroundType::kDefaultBg;
}

void Shelf::UpdateVisibilityState() {
  if (shelf_layout_manager_)
    shelf_layout_manager_->UpdateVisibilityState(/*force_layout=*/false);
}

void Shelf::MaybeUpdateShelfBackground() {
  if (!shelf_layout_manager_)
    return;

  shelf_layout_manager_->MaybeUpdateShelfBackground(
      AnimationChangeType::ANIMATE);
}

ShelfVisibilityState Shelf::GetVisibilityState() const {
  return shelf_layout_manager_ ? shelf_layout_manager_->visibility_state()
                               : SHELF_HIDDEN;
}

gfx::Rect Shelf::GetShelfBoundsInScreen() const {
  return shelf_widget()->GetTargetBounds();
}

gfx::Rect Shelf::GetIdealBounds() const {
  return shelf_layout_manager_->GetIdealBounds();
}

gfx::Rect Shelf::GetIdealBoundsForWorkAreaCalculation() {
  return shelf_layout_manager_->GetIdealBoundsForWorkAreaCalculation();
}

gfx::Rect Shelf::GetScreenBoundsOfItemIconForWindow(aura::Window* window) {
  if (!shelf_widget_)
    return gfx::Rect();
  return shelf_widget_->GetScreenBoundsOfItemIconForWindow(window);
}

bool Shelf::ProcessGestureEvent(const ui::GestureEvent& event) {
  // Can be called at login screen.
  if (!shelf_layout_manager_)
    return false;
  return shelf_layout_manager_->ProcessGestureEvent(event);
}

void Shelf::ProcessMouseEvent(const ui::MouseEvent& event) {
  if (shelf_layout_manager_)
    shelf_layout_manager_->ProcessMouseEventFromShelf(event);
}

void Shelf::ProcessScrollEvent(ui::ScrollEvent* event) {
  if (event->finger_count() != 2 || event->type() != ui::EventType::kScroll) {
    return;
  }

  if (!shelf_layout_manager_->is_active_session_state())
    return;

  // Introduce the swipe up gesture behind a flag over certain conditions.
  if (!shelf_layout_manager_->IsBubbleLauncherShowOnGestureScrollAvailable())
    return;

  auto* app_list_controller = Shell::Get()->app_list_controller();
  DCHECK(app_list_controller);

  shelf_layout_manager_->ProcessScrollEventFromShelf(event);
  event->SetHandled();
}

void Shelf::ProcessMouseWheelEvent(ui::MouseWheelEvent* event) {
  if (!shelf_layout_manager_->is_active_session_state() ||
      !IsHorizontalAlignment())
    return;

  // Introduce the swipe up gesture behind a flag over certain conditions.
  if (!shelf_layout_manager_->IsBubbleLauncherShowOnGestureScrollAvailable())
    return;

  auto* app_list_controller = Shell::Get()->app_list_controller();
  DCHECK(app_list_controller);

  shelf_layout_manager_->ProcessMouseWheelEventFromShelf(event);
  event->SetHandled();
}

void Shelf::AddObserver(ShelfObserver* observer) {
  observers_.AddObserver(observer);
}

void Shelf::RemoveObserver(ShelfObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Shelf::NotifyShelfIconPositionsChanged() {
  for (auto& observer : observers_)
    observer.OnShelfIconPositionsChanged();
}

StatusAreaWidget* Shelf::GetStatusAreaWidget() const {
  return shelf_widget_ ? shelf_widget_->status_area_widget() : nullptr;
}

gfx::Rect Shelf::GetSystemTrayAnchorRect() const {
  gfx::Rect work_area = GetWorkAreaInsets()->user_work_area_bounds();
  switch (alignment_) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return gfx::Rect(base::i18n::IsRTL()
                           ? work_area.x()
                           : work_area.right() - kShelfDisplayOffset,
                       work_area.bottom() - kShelfDisplayOffset, 0, 0);
    case ShelfAlignment::kLeft:
      return gfx::Rect(work_area.x(), work_area.bottom() - kShelfDisplayOffset,
                       0, 0);
    case ShelfAlignment::kRight:
      return gfx::Rect(work_area.right() - kShelfDisplayOffset,
                       work_area.bottom() - kShelfDisplayOffset, 0, 0);
  }
  NOTREACHED();
}

bool Shelf::ShouldHideOnSecondaryDisplay(session_manager::SessionState state) {
  if (Shell::GetPrimaryRootWindowController()->shelf() == this)
    return false;

  return state != session_manager::SessionState::ACTIVE;
}

void Shelf::SetVirtualKeyboardBoundsForTesting(const gfx::Rect& bounds) {
  KeyboardStateDescriptor state;
  state.is_visible = !bounds.IsEmpty();
  state.visual_bounds = bounds;
  state.occluded_bounds_in_screen = bounds;
  state.displaced_bounds_in_screen = gfx::Rect();
  WorkAreaInsets* work_area_insets = GetWorkAreaInsets();
  work_area_insets->OnKeyboardVisibilityChanged(state.is_visible);
  work_area_insets->OnKeyboardVisibleBoundsChanged(state.visual_bounds);
  work_area_insets->OnKeyboardOccludedBoundsChanged(
      state.occluded_bounds_in_screen);
  work_area_insets->OnKeyboardDisplacingBoundsChanged(
      state.displaced_bounds_in_screen);
  work_area_insets->OnKeyboardAppearanceChanged(state);
}

ShelfLockingManager* Shelf::GetShelfLockingManagerForTesting() {
  return &shelf_locking_manager_;
}

ShelfView* Shelf::GetShelfViewForTesting() {
  return shelf_widget_->shelf_view_for_testing();
}

metrics_util::ReportCallback Shelf::GetHotseatTransitionReportCallback(
    HotseatState target_state) {
  return hotseat_transition_metrics_reporter_->GetReportCallback(target_state);
}

metrics_util::ReportCallback Shelf::GetTranslucentBackgroundReportCallback(
    HotseatState target_state) {
  return translucent_background_metrics_reporter_->GetReportCallback(
      target_state);
}

metrics_util::ReportCallback Shelf::GetNavigationWidgetAnimationReportCallback(
    HotseatState target_hotseat_state) {
  return navigation_widget_metrics_reporter_->GetReportCallback(
      target_hotseat_state);
}

void Shelf::WillDeleteShelfLayoutManager() {
  // Clear event handlers that might forward events to the destroyed instance.
  auto_hide_event_handler_.reset();
  auto_dim_event_handler_.reset();
  navigation_widget_metrics_reporter_.reset();

  DCHECK(shelf_layout_manager_);
  shelf_layout_manager_->RemoveObserver(this);
  shelf_layout_manager_ = nullptr;
}

void Shelf::OnShelfVisibilityStateChanged(ShelfVisibilityState new_state) {
  if (!auto_dim_event_handler_ && switches::IsUsingShelfAutoDim()) {
    auto_dim_event_handler_ = std::make_unique<AutoDimEventHandler>(this);
  }

  if (new_state != SHELF_AUTO_HIDE) {
    auto_hide_event_handler_.reset();
  } else if (!auto_hide_event_handler_) {
    auto_hide_event_handler_ = std::make_unique<AutoHideEventHandler>(this);
  }

  for (auto& observer : observers_) {
    observer.OnShelfVisibilityStateChanged(new_state);
  }
}

void Shelf::OnAutoHideStateChanged(ShelfAutoHideState new_state) {
  for (auto& observer : observers_)
    observer.OnAutoHideStateChanged(new_state);
}

void Shelf::OnBackgroundUpdated(ShelfBackgroundType background_type,
                                AnimationChangeType change_type) {
  // Shelf should undim when transitioning to show app list.
  if (auto_dim_event_handler_ && IsAppListBackground(background_type))
    UndimShelf();

  for (auto& observer : observers_)
    observer.OnBackgroundTypeChanged(background_type, change_type);
}

void Shelf::OnHotseatStateChanged(HotseatState old_state,
                                  HotseatState new_state) {
  for (auto& observer : observers_)
    observer.OnHotseatStateChanged(old_state, new_state);
}

void Shelf::OnWorkAreaInsetsChanged() {
  for (auto& observer : observers_)
    observer.OnShelfWorkAreaInsetsChanged();
}

void Shelf::DimShelf() {
  auto_dim_event_handler_->DimShelf();
}

void Shelf::UndimShelf() {
  auto_dim_event_handler_->UndimShelf();
}

bool Shelf::HasDimShelfTimer() {
  return auto_dim_event_handler_->HasDimShelfTimer();
}

WorkAreaInsets* Shelf::GetWorkAreaInsets() const {
  const aura::Window* window = GetWindow();
  DCHECK(window);
  return WorkAreaInsets::ForWindow(window->GetRootWindow());
}

base::WeakPtr<Shelf> Shelf::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
