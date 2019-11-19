// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf.h"

#include <memory>

#include "ash/animation/animation_change_type.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/work_area_insets.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// Shelf::AutoHideEventHandler -----------------------------------------------

// Forwards mouse and gesture events to ShelfLayoutManager for auto-hide.
class Shelf::AutoHideEventHandler : public ui::EventHandler {
 public:
  explicit AutoHideEventHandler(Shelf* shelf) : shelf_(shelf) {
    Shell::Get()->AddPreTargetHandler(this);
  }

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
    if (shelf_->auto_hide_behavior() != SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS)
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
    if (event->type() == ui::ET_TOUCH_PRESSED && shelf_->IsVisible()) {
      shelf_layout_manager->LockAutoHideState(true);
    } else if (event->type() == ui::ET_TOUCH_RELEASED ||
               event->type() == ui::ET_TOUCH_CANCELLED) {
      shelf_layout_manager->LockAutoHideState(false);
    }
  }

 private:
  Shelf* shelf_;
  DISALLOW_COPY_AND_ASSIGN(AutoHideEventHandler);
};

// Shelf::AutoDimEventHandler -----------------------------------------------

// Handles mouse and touch events and determines whether ShelfLayoutManager
// should update shelf opacity for auto-dimming.
class Shelf::AutoDimEventHandler : public ui::EventHandler {
 public:
  explicit AutoDimEventHandler(Shelf* shelf) : shelf_(shelf) {
    Shell::Get()->AddPreTargetHandler(this);
    UndimShelf();
  }

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

 private:
  void DimShelf() { shelf_->shelf_layout_manager()->SetDimmed(true); }

  // Sets shelf as active and sets timer to mark shelf as inactive.
  void UndimShelf() {
    shelf_->shelf_layout_manager()->SetDimmed(false);
    update_shelf_dim_state_timer_.Start(
        FROM_HERE, kDimDelay,
        base::BindOnce(&AutoDimEventHandler::DimShelf, base::Unretained(this)));
  }

  // Unowned pointer to the shelf that owns this event handler.
  Shelf* shelf_;
  // OneShotTimer that dims shelf due to inactivity.
  base::OneShotTimer update_shelf_dim_state_timer_;

  // Delay before dimming the shelf.
  const base::TimeDelta kDimDelay = base::TimeDelta::FromSeconds(5);

  DISALLOW_COPY_AND_ASSIGN(AutoDimEventHandler);
};

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
      ui::ET_KEY_RELEASED, ui::VKEY_UNKNOWN, ui::EF_NONE);
  item_delegate->ItemSelected(std::move(event), display_id, LAUNCH_FROM_SHELF,
                              base::DoNothing());
}

void Shelf::CreateShelfWidget(aura::Window* root) {
  DCHECK(!shelf_widget_);
  aura::Window* shelf_container =
      root->GetChildById(kShellWindowId_ShelfContainer);
  shelf_widget_.reset(new ShelfWidget(this));

  DCHECK(!shelf_layout_manager_);
  shelf_layout_manager_ = shelf_widget_->shelf_layout_manager();
  shelf_layout_manager_->AddObserver(this);

  DCHECK(!shelf_widget_->hotseat_widget());
  aura::Window* control_container =
      root->GetChildById(kShellWindowId_ShelfControlContainer);
  shelf_widget_->CreateHotseatWidget(control_container);

  DCHECK(!shelf_widget_->navigation_widget());
  shelf_widget_->CreateNavigationWidget(control_container);

  // Must occur after |shelf_widget_| is constructed because the system tray
  // constructors call back into Shelf::shelf_widget().
  DCHECK(!shelf_widget_->status_area_widget());
  aura::Window* status_container =
      root->GetChildById(kShellWindowId_StatusContainer);
  shelf_widget_->CreateStatusAreaWidget(status_container);
  shelf_widget_->Initialize(shelf_container);

  // The Hotseat should be above everything in the shelf.
  shelf_widget_->hotseat_widget()->StackAtTop();
}

void Shelf::ShutdownShelfWidget() {
  shelf_widget_->Shutdown();
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
      alignment != SHELF_ALIGNMENT_BOTTOM_LOCKED) {
    shelf_locking_manager_.set_stored_alignment(alignment);
    return;
  }

  alignment_ = alignment;
  // The ShelfWidget notifies the ShelfView of the alignment change.
  shelf_widget_->OnShelfAlignmentChanged();
  tooltip_->Close();
  shelf_layout_manager_->LayoutShelf();
  Shell::Get()->NotifyShelfAlignmentChanged(GetWindow()->GetRootWindow());
}

bool Shelf::IsHorizontalAlignment() const {
  switch (alignment_) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      return true;
    case SHELF_ALIGNMENT_LEFT:
    case SHELF_ALIGNMENT_RIGHT:
      return false;
  }
  NOTREACHED();
  return true;
}

int Shelf::SelectValueForShelfAlignment(int bottom, int left, int right) const {
  switch (alignment_) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      return bottom;
    case SHELF_ALIGNMENT_LEFT:
      return left;
    case SHELF_ALIGNMENT_RIGHT:
      return right;
  }
  NOTREACHED();
  return bottom;
}

int Shelf::PrimaryAxisValue(int horizontal, int vertical) const {
  return IsHorizontalAlignment() ? horizontal : vertical;
}

void Shelf::SetAutoHideBehavior(ShelfAutoHideBehavior auto_hide_behavior) {
  DCHECK(shelf_layout_manager_);

  if (auto_hide_behavior_ == auto_hide_behavior)
    return;

  auto_hide_behavior_ = auto_hide_behavior;
  Shell::Get()->NotifyShelfAutoHideBehaviorChanged(
      GetWindow()->GetRootWindow());
}

ShelfAutoHideState Shelf::GetAutoHideState() const {
  return shelf_layout_manager_->auto_hide_state();
}

void Shelf::UpdateAutoHideState() {
  shelf_layout_manager_->UpdateAutoHideState();
}

ShelfBackgroundType Shelf::GetBackgroundType() const {
  return shelf_widget_ ? shelf_widget_->GetBackgroundType()
                       : SHELF_BACKGROUND_DEFAULT;
}

void Shelf::UpdateVisibilityState() {
  if (shelf_layout_manager_)
    shelf_layout_manager_->UpdateVisibilityState();
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

void Shelf::ProcessMouseWheelEvent(ui::MouseWheelEvent* event) {
  event->SetHandled();
  if (!IsHorizontalAlignment())
    return;
  auto* app_list_controller = Shell::Get()->app_list_controller();
  DCHECK(app_list_controller);
  // If the App List is not visible, send MouseWheel events to the
  // |shelf_layout_manager_| because these events are used to show the App List.
  if (app_list_controller->IsVisible())
    app_list_controller->ProcessMouseWheelEvent(*event);
  else
    shelf_layout_manager_->ProcessMouseWheelEventFromShelf(event);
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

TrayBackgroundView* Shelf::GetSystemTrayAnchorView() const {
  return GetStatusAreaWidget()->GetSystemTrayAnchor();
}

gfx::Rect Shelf::GetSystemTrayAnchorRect() const {
  gfx::Rect work_area = GetWorkAreaInsets()->user_work_area_bounds();
  switch (alignment_) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      return gfx::Rect(
          base::i18n::IsRTL() ? work_area.x() : work_area.right() - 1,
          work_area.bottom() - 1, 0, 0);
    case SHELF_ALIGNMENT_LEFT:
      return gfx::Rect(work_area.x(), work_area.bottom() - 1, 0, 0);
    case SHELF_ALIGNMENT_RIGHT:
      return gfx::Rect(work_area.right() - 1, work_area.bottom() - 1, 0, 0);
  }
  NOTREACHED();
  return gfx::Rect();
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

void Shelf::WillDeleteShelfLayoutManager() {
  // Clear event handlers that might forward events to the destroyed instance.
  auto_hide_event_handler_.reset();
  auto_dim_event_handler_.reset();

  DCHECK(shelf_layout_manager_);
  shelf_layout_manager_->RemoveObserver(this);
  shelf_layout_manager_ = nullptr;
}

void Shelf::WillChangeVisibilityState(ShelfVisibilityState new_state) {
  for (auto& observer : observers_)
    observer.WillChangeVisibilityState(new_state);
  if (new_state != SHELF_AUTO_HIDE) {
    auto_hide_event_handler_.reset();
  } else if (!auto_hide_event_handler_) {
    auto_hide_event_handler_ = std::make_unique<AutoHideEventHandler>(this);
  }

  if (!auto_dim_event_handler_ && ash::switches::IsUsingShelfAutoDim()) {
    auto_dim_event_handler_ = std::make_unique<AutoDimEventHandler>(this);
  }
}

void Shelf::OnAutoHideStateChanged(ShelfAutoHideState new_state) {
  for (auto& observer : observers_)
    observer.OnAutoHideStateChanged(new_state);
}

void Shelf::OnBackgroundUpdated(ShelfBackgroundType background_type,
                                AnimationChangeType change_type) {
  if (background_type == GetBackgroundType())
    return;
  for (auto& observer : observers_)
    observer.OnBackgroundTypeChanged(background_type, change_type);
}

void Shelf::OnWorkAreaInsetsChanged() {
  for (auto& observer : observers_)
    observer.OnShelfWorkAreaInsetsChanged();
}

WorkAreaInsets* Shelf::GetWorkAreaInsets() const {
  const aura::Window* window = GetWindow();
  DCHECK(window);
  return WorkAreaInsets::ForWindow(window->GetRootWindow());
}

}  // namespace ash
