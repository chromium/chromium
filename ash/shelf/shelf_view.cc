// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_view.h"

#include <algorithm>
#include <memory>

#include "ash/drag_drop/drag_image_view.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/scoped_root_window_for_new_windows.h"
#include "ash/screen_util.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/back_button.h"
#include "ash/shelf/overflow_bubble.h"
#include "ash/shelf/overflow_bubble_view.h"
#include "ash/shelf/overflow_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_application_menu_model.h"
#include "ash/shelf/shelf_button.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_context_menu_model.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_menu_model_adapter.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/chromeos_switches.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/separator.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

using gfx::Animation;
using views::View;

namespace ash {

// Indices of the start-aligned system buttons (the "back" button in tablet
// mode, and the "app list" button).
constexpr int kBackButtonIndex = 0;
constexpr int kAppListButtonIndex = 1;

// The distance of the cursor from the outer rim of the shelf before it
// separates.
constexpr int kRipOffDistance = 48;

// The dimensions, in pixels, of the separator between pinned and unpinned
// items.
constexpr int kSeparatorSize = 20;
constexpr int kSeparatorThickness = 1;

// The margin between the app list button and the first shelf item.
constexpr int kAppListButtonMargin = 32;

// White with ~20% opacity.
constexpr SkColor kSeparatorColor = SkColorSetARGB(0x32, 0xFF, 0xFF, 0xFF);

// The rip off drag and drop proxy image should get scaled by this factor.
constexpr float kDragAndDropProxyScale = 1.2f;

// The opacity represents that this partially disappeared item will get removed.
constexpr float kDraggedImageOpacity = 0.5f;

namespace {

// Helper to check if tablet mode is enabled.
bool IsTabletModeEnabled() {
  return Shell::Get()->tablet_mode_controller() &&
         Shell::Get()
             ->tablet_mode_controller()
             ->IsTabletModeWindowManagerEnabled();
}

// A class to temporarily disable a given bounds animator.
class BoundsAnimatorDisabler {
 public:
  explicit BoundsAnimatorDisabler(views::BoundsAnimator* bounds_animator)
      : old_duration_(bounds_animator->GetAnimationDuration()),
        bounds_animator_(bounds_animator) {
    bounds_animator_->SetAnimationDuration(1);
  }

  ~BoundsAnimatorDisabler() {
    bounds_animator_->SetAnimationDuration(old_duration_);
  }

 private:
  // The previous animation duration.
  int old_duration_;
  // The bounds animator which gets used.
  views::BoundsAnimator* bounds_animator_;

  DISALLOW_COPY_AND_ASSIGN(BoundsAnimatorDisabler);
};

// Custom FocusSearch used to navigate the shelf in the order items are in
// the ViewModel.
class ShelfFocusSearch : public views::FocusSearch {
 public:
  explicit ShelfFocusSearch(ShelfView* shelf_view)
      : FocusSearch(nullptr, true, true), shelf_view_(shelf_view) {}
  ~ShelfFocusSearch() override = default;

  // views::FocusSearch:
  View* FindNextFocusableView(
      View* starting_view,
      FocusSearch::SearchDirection search_direction,
      FocusSearch::TraversalDirection traversal_direction,
      FocusSearch::StartingViewPolicy check_starting_view,
      FocusSearch::AnchoredDialogPolicy can_go_into_anchored_dialog,
      views::FocusTraversable** focus_traversable,
      View** focus_traversable_view) override {
    views::ViewModel* view_model = shelf_view_->view_model();
    int index = view_model->GetIndexOfView(starting_view);
    // The back button (item with index 0 on the model) only exists in tablet
    // mode, so punt focus to the app list button (item with index 1 on the
    // model).
    const bool tablet_mode = IsTabletModeEnabled();
    if (!tablet_mode && index == 0)
      ++index;

    // Increment or decrement index based on the cycle, unless we are at either
    // edge, then we loop to the back or front.
    const bool is_overflow_shelf = shelf_view_->is_overflow_mode();
    const bool overflow_shown =
        is_overflow_shelf ? shelf_view_->main_shelf()->IsShowingOverflowBubble()
                          : shelf_view_->IsShowingOverflowBubble();
    if (search_direction == FocusSearch::SearchDirection::kBackwards) {
      --index;
      if (index < 0 || (index == 0 && !tablet_mode)) {
        index = overflow_shown ? view_model->view_size() - 1
                               : shelf_view_->last_visible_index();
      }
    } else {
      ++index;
      if (overflow_shown && index >= view_model->view_size()) {
        index = 0;
      } else if (!overflow_shown && index > shelf_view_->last_visible_index()) {
        index = is_overflow_shelf ? shelf_view_->first_visible_index() : 0;
      }

      // Skip the back button (item with index 0) when not in tablet mode.
      if (!tablet_mode && index == 0)
        index = 1;
    }

    if (overflow_shown) {
      // Switch to the other shelf if |index| is not visible on this shelf.
      if (index < shelf_view_->first_visible_index() ||
          index > shelf_view_->last_visible_index()) {
        ShelfView* new_shelf_view =
            is_overflow_shelf
                ? shelf_view_->main_shelf()
                : shelf_view_->overflow_bubble()->bubble_view()->shelf_view();
        if (is_overflow_shelf)
          shelf_view_->shelf_widget()->set_activated_from_overflow_bubble(true);
        return new_shelf_view->view_model()->view_at(index);
      }
    }
    return view_model->view_at(index);
  }

 private:
  ShelfView* shelf_view_;

  DISALLOW_COPY_AND_ASSIGN(ShelfFocusSearch);
};

// AnimationDelegate used when inserting a new item. This steadily increases the
// opacity of the layer as the animation progress.
class FadeInAnimationDelegate : public gfx::AnimationDelegate {
 public:
  explicit FadeInAnimationDelegate(views::View* view) : view_(view) {}
  ~FadeInAnimationDelegate() override = default;

  // AnimationDelegate overrides:
  void AnimationProgressed(const Animation* animation) override {
    view_->layer()->SetOpacity(animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();
  }
  void AnimationEnded(const Animation* animation) override {
    view_->layer()->SetOpacity(1.0f);
    view_->layer()->ScheduleDraw();
  }
  void AnimationCanceled(const Animation* animation) override {
    view_->layer()->SetOpacity(1.0f);
    view_->layer()->ScheduleDraw();
  }

 private:
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(FadeInAnimationDelegate);
};

void ReflectItemStatus(const ShelfItem& item, ShelfButton* button) {
  ShelfID active_id = Shell::Get()->shelf_model()->active_shelf_id();
  if (!active_id.IsNull() && item.id == active_id) {
    // The active status trumps all other statuses.
    button->AddState(ShelfButton::STATE_ACTIVE);
    button->ClearState(ShelfButton::STATE_RUNNING);
    button->ClearState(ShelfButton::STATE_ATTENTION);
    return;
  }

  button->ClearState(ShelfButton::STATE_ACTIVE);

  switch (item.status) {
    case STATUS_CLOSED:
      button->ClearState(ShelfButton::STATE_RUNNING);
      button->ClearState(ShelfButton::STATE_ATTENTION);
      break;
    case STATUS_RUNNING:
      button->AddState(ShelfButton::STATE_RUNNING);
      button->ClearState(ShelfButton::STATE_ATTENTION);
      break;
    case STATUS_ATTENTION:
      button->ClearState(ShelfButton::STATE_RUNNING);
      button->AddState(ShelfButton::STATE_ATTENTION);
      break;
  }

  if (features::IsNotificationIndicatorEnabled()) {
    if (item.has_notification)
      button->AddState(ShelfButton::STATE_NOTIFICATION);
    else
      button->ClearState(ShelfButton::STATE_NOTIFICATION);
  }
}

// Returns the id of the display on which |view| is shown.
int64_t GetDisplayIdForView(View* view) {
  aura::Window* window = view->GetWidget()->GetNativeWindow();
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
}

// Whether |item_view| is a ShelfButton and its state is STATE_DRAGGING.
bool ShelfButtonIsInDrag(const ShelfItemType item_type,
                         const views::View* item_view) {
  switch (item_type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP:
      return static_cast<const ShelfButton*>(item_view)->state() &
             ShelfButton::STATE_DRAGGING;
    case TYPE_DIALOG:
    case TYPE_BACK_BUTTON:
    case TYPE_APP_LIST:
    case TYPE_UNDEFINED:
      return false;
  }
}

}  // namespace

// AnimationDelegate used when deleting an item. This steadily decreased the
// opacity of the layer as the animation progress.
class ShelfView::FadeOutAnimationDelegate : public gfx::AnimationDelegate {
 public:
  FadeOutAnimationDelegate(ShelfView* host, views::View* view)
      : shelf_view_(host), view_(view) {}
  ~FadeOutAnimationDelegate() override = default;

  // AnimationDelegate overrides:
  void AnimationProgressed(const Animation* animation) override {
    view_->layer()->SetOpacity(1 - animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();
  }
  void AnimationEnded(const Animation* animation) override {
    shelf_view_->OnFadeOutAnimationEnded();
  }
  void AnimationCanceled(const Animation* animation) override {}

 private:
  ShelfView* shelf_view_;
  std::unique_ptr<views::View> view_;

  DISALLOW_COPY_AND_ASSIGN(FadeOutAnimationDelegate);
};

// AnimationDelegate used to trigger fading an element in. When an item is
// inserted this delegate is attached to the animation that expands the size of
// the item.  When done it kicks off another animation to fade the item in.
class ShelfView::StartFadeAnimationDelegate : public gfx::AnimationDelegate {
 public:
  StartFadeAnimationDelegate(ShelfView* host, views::View* view)
      : shelf_view_(host), view_(view) {}
  ~StartFadeAnimationDelegate() override = default;

  // AnimationDelegate overrides:
  void AnimationEnded(const Animation* animation) override {
    shelf_view_->FadeIn(view_);
  }
  void AnimationCanceled(const Animation* animation) override {
    view_->layer()->SetOpacity(1.0f);
  }

 private:
  ShelfView* shelf_view_;
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(StartFadeAnimationDelegate);
};

// static
const int ShelfView::kMinimumDragDistance = 8;

ShelfView::ShelfView(ShelfModel* model, Shelf* shelf, ShelfWidget* shelf_widget)
    : model_(model),
      shelf_(shelf),
      shelf_widget_(shelf_widget),
      view_model_(new views::ViewModel),
      bounds_animator_(std::make_unique<views::BoundsAnimator>(this)),
      tooltip_(this),
      focus_search_(std::make_unique<ShelfFocusSearch>(this)),
      shelf_item_background_color_(kShelfDefaultBaseColor),
      weak_factory_(this) {
  DCHECK(model_);
  DCHECK(shelf_);
  DCHECK(shelf_widget_);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->system_tray_model()->virtual_keyboard()->AddObserver(this);
  bounds_animator_->AddObserver(this);
  set_context_menu_controller(this);
}

ShelfView::~ShelfView() {
  // Shell destroys the TabletModeController before destroying all root windows.
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->system_tray_model()->virtual_keyboard()->RemoveObserver(this);
  bounds_animator_->RemoveObserver(this);
  model_->RemoveObserver(this);
}

void ShelfView::Init() {
  model_->AddObserver(this);

  // Add the background view behind the app list and back buttons first, so
  // that other views will appear above it.
  back_and_app_list_background_ = new views::View();
  back_and_app_list_background_->SetBackground(
      CreateBackgroundFromPainter(views::Painter::CreateSolidRoundRectPainter(
          kShelfControlPermanentHighlightBackground,
          ShelfConstants::control_border_radius())));
  ConfigureChildView(back_and_app_list_background_);
  AddChildView(back_and_app_list_background_);

  const ShelfItems& items(model_->items());
  for (ShelfItems::const_iterator i = items.begin(); i != items.end(); ++i) {
    views::View* child = CreateViewForItem(*i);
    child->SetPaintToLayer();
    view_model_->Add(child, static_cast<int>(i - items.begin()));
    AddChildView(child);
  }
  overflow_button_ = new OverflowButton(this, shelf_);
  ConfigureChildView(overflow_button_);
  AddChildView(overflow_button_);
  UpdateBackButton();

  separator_ = new views::Separator();
  separator_->SetColor(kSeparatorColor);
  separator_->SetPreferredHeight(kSeparatorSize);
  separator_->SetVisible(false);
  ConfigureChildView(separator_);
  AddChildView(separator_);

  // We'll layout when our bounds change.
}

void ShelfView::OnShelfAlignmentChanged() {
  LayoutToIdealBounds();
  for (int i = 0; i < view_model_->view_size(); ++i) {
    if (i >= first_visible_index_ && i <= last_visible_index_)
      view_model_->view_at(i)->Layout();
  }
  tooltip_.Close();
  if (overflow_bubble_)
    overflow_bubble_->Hide();
  // For crbug.com/587931, because AppListButton layout logic is in OnPaint.
  AppListButton* app_list_button = GetAppListButton();
  if (app_list_button)
    app_list_button->SchedulePaint();
}

gfx::Rect ShelfView::GetIdealBoundsOfItemIcon(const ShelfID& id) {
  int index = model_->ItemIndexByID(id);
  if (index < 0 || last_visible_index_ < 0 || index >= view_model_->view_size())
    return gfx::Rect();

  // Map items in the overflow bubble to the overflow button.
  if (index > last_visible_index_)
    return GetMirroredRect(overflow_button_->bounds());

  const gfx::Rect& ideal_bounds(view_model_->ideal_bounds(index));
  views::View* view = view_model_->view_at(index);
  // The app list and back button are not ShelfButton subclass instances.
  if (view == GetAppListButton() || view == GetBackButton())
    return GetMirroredRect(ideal_bounds);

  CHECK_EQ(ShelfButton::kViewClassName, view->GetClassName());
  ShelfButton* button = static_cast<ShelfButton*>(view);
  gfx::Rect icon_bounds = button->GetIconBounds();
  return gfx::Rect(GetMirroredXWithWidthInView(
                       ideal_bounds.x() + icon_bounds.x(), icon_bounds.width()),
                   ideal_bounds.y() + icon_bounds.y(), icon_bounds.width(),
                   icon_bounds.height());
}

bool ShelfView::IsShowingMenu() const {
  return shelf_menu_model_adapter_ &&
         shelf_menu_model_adapter_->IsShowingMenu();
}

bool ShelfView::IsShowingMenuForView(const views::View* view) const {
  return IsShowingMenu() &&
         shelf_menu_model_adapter_->IsShowingMenuForView(*view);
}

bool ShelfView::IsShowingOverflowBubble() const {
  return overflow_bubble_.get() && overflow_bubble_->IsShowing();
}

AppListButton* ShelfView::GetAppListButton() const {
  for (int i = 0; i < model_->item_count(); ++i) {
    if (model_->items()[i].type == TYPE_APP_LIST) {
      views::View* view = view_model_->view_at(i);
      CHECK_EQ(AppListButton::kViewClassName, view->GetClassName());
      return static_cast<AppListButton*>(view);
    }
  }

  NOTREACHED() << "Applist button not found";
  return nullptr;
}

BackButton* ShelfView::GetBackButton() const {
  for (int i = 0; i < model_->item_count(); ++i) {
    if (model_->items()[i].type == TYPE_BACK_BUTTON) {
      views::View* view = view_model_->view_at(i);
      CHECK_EQ(BackButton::kViewClassName, view->GetClassName());
      return static_cast<BackButton*>(view);
    }
  }

  NOTREACHED() << "Back button not found";
  return nullptr;
}

bool ShelfView::ShouldHideTooltip(const gfx::Point& cursor_location) const {
  gfx::Rect tooltip_bounds;
  for (int i = 0; i < child_count(); ++i) {
    const views::View* child = child_at(i);
    if (!IsTabletModeEnabled() && child == GetBackButton())
      continue;
    if (child != overflow_button_ && ShouldShowTooltipForView(child))
      tooltip_bounds.Union(child->GetMirroredBounds());
  }
  return !tooltip_bounds.Contains(cursor_location);
}

bool ShelfView::ShouldShowTooltipForView(const views::View* view) const {
  // TODO(msw): Push this app list state into ShelfItem::shows_tooltip.
  if (view == GetAppListButton() && GetAppListButton()->is_showing_app_list())
    return false;
  // Don't show a tooltip for a view that's currently being dragged.
  if (view == drag_view_)
    return false;

  return ShelfItemForView(view) && !IsShowingMenuForView(view);
}

base::string16 ShelfView::GetTitleForView(const views::View* view) const {
  const ShelfItem* item = ShelfItemForView(view);
  return item ? item->title : base::string16();
}

gfx::Rect ShelfView::GetVisibleItemsBoundsInScreen() {
  gfx::Size preferred_size = GetPreferredSize();
  gfx::Point origin(GetMirroredXWithWidthInView(0, preferred_size.width()), 0);
  ConvertPointToScreen(this, &origin);
  return gfx::Rect(origin, preferred_size);
}

void ShelfView::ButtonPressed(views::Button* sender,
                              const ui::Event& event,
                              views::InkDrop* ink_drop) {
  if (sender == overflow_button_) {
    ToggleOverflowBubble();
    shelf_button_pressed_metric_tracker_.ButtonPressed(event, sender,
                                                       SHELF_ACTION_NONE);
    return;
  }

  // None of the checks in ShouldEventActivateButton() affects overflow button.
  // So, it is safe to be checked after handling overflow button.
  if (!ShouldEventActivateButton(sender, event))
    return;

  // Ensure the keyboard is hidden and stays hidden (as long as it isn't locked)
  if (keyboard::KeyboardController::Get()->IsEnabled())
    keyboard::KeyboardController::Get()->HideKeyboardExplicitlyBySystem();

  // Close the overflow bubble if an item on either shelf is clicked. Press
  // events elsewhere will close the overflow shelf via OverflowBubble's
  // EventHandler functionality.
  ShelfView* shelf_view = main_shelf_ ? main_shelf_ : this;
  if (shelf_view->IsShowingOverflowBubble())
    shelf_view->ToggleOverflowBubble();

  // Record the index for the last pressed shelf item.
  last_pressed_index_ = view_model_->GetIndexOfView(sender);
  DCHECK_LT(-1, last_pressed_index_);

  // Place new windows on the same display as the button.
  aura::Window* window = sender->GetWidget()->GetNativeWindow();
  scoped_root_window_for_new_windows_ =
      std::make_unique<ScopedRootWindowForNewWindows>(window->GetRootWindow());

  // Slow down activation animations if shift key is pressed.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> slowing_animations;
  if (event.IsShiftDown()) {
    slowing_animations.reset(new ui::ScopedAnimationDurationScaleMode(
        ui::ScopedAnimationDurationScaleMode::SLOW_DURATION));
  }

  // Collect usage statistics before we decide what to do with the click.
  switch (model_->items()[last_pressed_index_].type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP:
      Shell::Get()->metrics()->RecordUserMetricsAction(
          UMA_LAUNCHER_CLICK_ON_APP);
      break;

    case TYPE_APP_LIST:
      Shell::Get()->metrics()->RecordUserMetricsAction(
          UMA_LAUNCHER_CLICK_ON_APPLIST_BUTTON);
      break;

    case TYPE_BACK_BUTTON:
    case TYPE_DIALOG:
      break;

    case TYPE_UNDEFINED:
      NOTREACHED() << "ShelfItemType must be set.";
      break;
  }

  // Run AfterItemSelected directly if the item has no delegate (ie. in tests).
  const ShelfItem& item = model_->items()[last_pressed_index_];
  if (!model_->GetShelfItemDelegate(item.id)) {
    AfterItemSelected(item, sender, ui::Event::Clone(event), ink_drop,
                      SHELF_ACTION_NONE, base::nullopt);
    return;
  }

  // Notify the item of its selection; handle the result in AfterItemSelected.
  model_->GetShelfItemDelegate(item.id)->ItemSelected(
      ui::Event::Clone(event), GetDisplayIdForView(this), LAUNCH_FROM_UNKNOWN,
      base::BindOnce(&ShelfView::AfterItemSelected, weak_factory_.GetWeakPtr(),
                     item, sender, base::Passed(ui::Event::Clone(event)),
                     ink_drop));
}

////////////////////////////////////////////////////////////////////////////////
// ShelfView, FocusTraversable implementation:

views::FocusSearch* ShelfView::GetFocusSearch() {
  return focus_search_.get();
}

views::FocusTraversable* ShelfView::GetFocusTraversableParent() {
  return parent()->GetFocusTraversable();
}

View* ShelfView::GetFocusTraversableParentView() {
  return this;
}

void ShelfView::CreateDragIconProxy(
    const gfx::Point& location_in_screen_coordinates,
    const gfx::ImageSkia& icon,
    views::View* replaced_view,
    const gfx::Vector2d& cursor_offset_from_center,
    float scale_factor) {
  drag_replaced_view_ = replaced_view;
  aura::Window* root_window =
      drag_replaced_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
  drag_image_ = std::make_unique<DragImageView>(
      root_window, ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  drag_image_->SetImage(icon);
  gfx::Size size = drag_image_->GetPreferredSize();
  size.set_width(size.width() * scale_factor);
  size.set_height(size.height() * scale_factor);
  drag_image_offset_ = gfx::Vector2d(size.width() / 2, size.height() / 2) +
                       cursor_offset_from_center;
  gfx::Rect drag_image_bounds(
      location_in_screen_coordinates - drag_image_offset_, size);
  drag_image_->SetBoundsInScreen(drag_image_bounds);
  drag_image_->SetWidgetVisible(true);
}

void ShelfView::OnTabletModeStarted() {
  // Close all menus when tablet mode starts to ensure that the clamshell only
  // context menu options are not available in tablet mode.
  if (shelf_menu_model_adapter_)
    shelf_menu_model_adapter_->Cancel();
}

void ShelfView::OnTabletModeEnded() {
  // Close all menus when tablet mode ends so that menu options are kept
  // consistent with device state.
  if (shelf_menu_model_adapter_)
    shelf_menu_model_adapter_->Cancel();
}

void ShelfView::OnVirtualKeyboardVisibilityChanged() {
  LayoutToIdealBounds();
}

void ShelfView::CreateDragIconProxyByLocationWithNoAnimation(
    const gfx::Point& origin_in_screen_coordinates,
    const gfx::ImageSkia& icon,
    views::View* replaced_view,
    float scale_factor,
    int blur_radius) {
  drag_replaced_view_ = replaced_view;
  aura::Window* root_window =
      drag_replaced_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
  drag_image_ = std::make_unique<DragImageView>(
      root_window, ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  if (blur_radius > 0)
    SetDragImageBlur(icon.size(), blur_radius);
  drag_image_->SetImage(icon);
  gfx::Size size = drag_image_->GetPreferredSize();
  size.set_width(size.width() * scale_factor);
  size.set_height(size.height() * scale_factor);
  gfx::Rect drag_image_bounds(origin_in_screen_coordinates, size);
  drag_image_->SetBoundsInScreen(drag_image_bounds);

  // Turn off the default visibility animation.
  drag_image_->GetWidget()->SetVisibilityAnimationTransition(
      views::Widget::ANIMATE_NONE);
  drag_image_->SetWidgetVisible(true);
}

void ShelfView::UpdateDragIconProxy(
    const gfx::Point& location_in_screen_coordinates) {
  // TODO(jennyz): Investigate why drag_image_ becomes null at this point per
  // crbug.com/34722, while the app list item is still being dragged around.
  if (drag_image_) {
    drag_image_->SetScreenPosition(location_in_screen_coordinates -
                                   drag_image_offset_);
  }
}

void ShelfView::UpdateDragIconProxyByLocation(
    const gfx::Point& origin_in_screen_coordinates) {
  if (drag_image_)
    drag_image_->SetScreenPosition(origin_in_screen_coordinates);
}

bool ShelfView::IsDraggedView(const ShelfButton* view) const {
  return drag_view_ == view;
}

const std::vector<aura::Window*> ShelfView::GetOpenWindowsForShelfView(
    views::View* view) {
  std::vector<aura::Window*> window_list =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList();
  std::vector<aura::Window*> open_windows;
  const std::string shelf_item_app_id = ShelfItemForView(view)->id.app_id;
  for (auto* window : window_list) {
    const std::string window_app_id =
        ShelfID::Deserialize(window->GetProperty(kShelfIDKey)).app_id;
    if (window_app_id == shelf_item_app_id) {
      // TODO: In the very first version we only show one window. Add the proper
      // UI to show all windows for a given open app.
      open_windows.push_back(window);
    }
  }
  return open_windows;
}

void ShelfView::DestroyDragIconProxy() {
  drag_image_.reset();
  drag_image_offset_ = gfx::Vector2d(0, 0);
}

bool ShelfView::StartDrag(const std::string& app_id,
                          const gfx::Point& location_in_screen_coordinates) {
  // Bail if an operation is already going on - or the cursor is not inside.
  // This could happen if mouse / touch operations overlap.
  if (!drag_and_drop_shelf_id_.IsNull() || app_id.empty() ||
      !GetBoundsInScreen().Contains(location_in_screen_coordinates))
    return false;

  // If the AppsGridView (which was dispatching this event) was opened by our
  // button, ShelfView dragging operations are locked and we have to unlock.
  CancelDrag(-1);
  drag_and_drop_item_pinned_ = false;
  drag_and_drop_shelf_id_ = ShelfID(app_id);
  // Check if the application is pinned - if not, we have to pin it so
  // that we can re-arrange the shelf order accordingly. Note that items have
  // to be pinned to give them the same (order) possibilities as a shortcut.
  // When an item is dragged from overflow to shelf, IsShowingOverflowBubble()
  // returns true. At this time, we don't need to pin the item.
  if (!IsShowingOverflowBubble() && !model_->IsAppPinned(app_id)) {
    model_->PinAppWithID(app_id);
    drag_and_drop_item_pinned_ = true;
  }
  views::View* drag_and_drop_view =
      view_model_->view_at(model_->ItemIndexByID(drag_and_drop_shelf_id_));
  DCHECK(drag_and_drop_view);

  // Since there is already an icon presented by the caller, we hide this item
  // for now. That has to be done by reducing the size since the visibility will
  // change once a regrouping animation is performed.
  pre_drag_and_drop_size_ = drag_and_drop_view->size();
  drag_and_drop_view->SetSize(gfx::Size());

  // First we have to center the mouse cursor over the item.
  gfx::Point pt = drag_and_drop_view->GetBoundsInScreen().CenterPoint();
  views::View::ConvertPointFromScreen(drag_and_drop_view, &pt);
  gfx::Point point_in_root = location_in_screen_coordinates;
  ::wm::ConvertPointFromScreen(
      wm::GetRootWindowAt(location_in_screen_coordinates), &point_in_root);
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, pt, point_in_root,
                       ui::EventTimeForNow(), 0, 0);
  PointerPressedOnButton(drag_and_drop_view, DRAG_AND_DROP, event);

  // Drag the item where it really belongs.
  Drag(location_in_screen_coordinates);
  return true;
}

bool ShelfView::Drag(const gfx::Point& location_in_screen_coordinates) {
  if (drag_and_drop_shelf_id_.IsNull() ||
      !GetBoundsInScreen().Contains(location_in_screen_coordinates))
    return false;

  gfx::Point pt = location_in_screen_coordinates;
  views::View* drag_and_drop_view =
      view_model_->view_at(model_->ItemIndexByID(drag_and_drop_shelf_id_));
  ConvertPointFromScreen(drag_and_drop_view, &pt);
  gfx::Point point_in_root = location_in_screen_coordinates;
  ::wm::ConvertPointFromScreen(
      wm::GetRootWindowAt(location_in_screen_coordinates), &point_in_root);
  ui::MouseEvent event(ui::ET_MOUSE_DRAGGED, pt, point_in_root,
                       ui::EventTimeForNow(), 0, 0);
  PointerDraggedOnButton(drag_and_drop_view, DRAG_AND_DROP, event);
  return true;
}

void ShelfView::EndDrag(bool cancel) {
  if (drag_and_drop_shelf_id_.IsNull())
    return;

  views::View* drag_and_drop_view =
      view_model_->view_at(model_->ItemIndexByID(drag_and_drop_shelf_id_));
  PointerReleasedOnButton(drag_and_drop_view, DRAG_AND_DROP, cancel);

  // Either destroy the temporarily created item - or - make the item visible.
  if (drag_and_drop_item_pinned_ && cancel) {
    model_->UnpinAppWithID(drag_and_drop_shelf_id_.app_id);
  } else if (drag_and_drop_view) {
    if (cancel) {
      // When a hosted drag gets canceled, the item can remain in the same slot
      // and it might have moved within the bounds. In that case the item need
      // to animate back to its correct location.
      AnimateToIdealBounds();
    } else {
      drag_and_drop_view->SetSize(pre_drag_and_drop_size_);
    }
  }

  drag_and_drop_shelf_id_ = ShelfID();
}

bool ShelfView::ShouldEventActivateButton(View* view, const ui::Event& event) {
  if (dragging())
    return false;

  // Ignore if we are already in a pointer event sequence started with a repost
  // event on the same shelf item. See crbug.com/343005 for more detail.
  if (is_repost_event_on_same_item_)
    return false;

  // Don't activate the item twice on double-click. Otherwise the window starts
  // animating open due to the first click, then immediately minimizes due to
  // the second click. The user most likely intended to open or minimize the
  // item once, not do both.
  if (event.flags() & ui::EF_IS_DOUBLE_CLICK)
    return false;

  // Ignore if this is a repost event on the last pressed shelf item.
  int index = view_model_->GetIndexOfView(view);
  if (index == -1)
    return false;
  return !IsRepostEvent(event) || last_pressed_index_ != index;
}

void ShelfView::PointerPressedOnButton(views::View* view,
                                       Pointer pointer,
                                       const ui::LocatedEvent& event) {
  if (drag_view_)
    return;

  if (IsShowingMenu())
    shelf_menu_model_adapter_->Cancel();

  int index = view_model_->GetIndexOfView(view);
  if (index == -1 || view_model_->view_size() <= 1)
    return;  // View is being deleted, ignore request.

  if (view == GetAppListButton() || view == GetBackButton())
    return;  // Views are not draggable, ignore request.

  // Only when the repost event occurs on the same shelf item, we should ignore
  // the call in ShelfView::ButtonPressed(...).
  is_repost_event_on_same_item_ =
      IsRepostEvent(event) && (last_pressed_index_ == index);

  CHECK_EQ(ShelfButton::kViewClassName, view->GetClassName());
  drag_view_ = static_cast<ShelfButton*>(view);
  drag_origin_ = gfx::Point(event.x(), event.y());
  UMA_HISTOGRAM_ENUMERATION("Ash.ShelfAlignmentUsage",
                            static_cast<ShelfAlignmentUmaEnumValue>(
                                shelf_->SelectValueForShelfAlignment(
                                    SHELF_ALIGNMENT_UMA_ENUM_VALUE_BOTTOM,
                                    SHELF_ALIGNMENT_UMA_ENUM_VALUE_LEFT,
                                    SHELF_ALIGNMENT_UMA_ENUM_VALUE_RIGHT)),
                            SHELF_ALIGNMENT_UMA_ENUM_VALUE_COUNT);
}

void ShelfView::PointerDraggedOnButton(views::View* view,
                                       Pointer pointer,
                                       const ui::LocatedEvent& event) {
  if (CanPrepareForDrag(pointer, event))
    PrepareForDrag(pointer, event);

  if (drag_pointer_ == pointer)
    ContinueDrag(event);
}

void ShelfView::PointerReleasedOnButton(views::View* view,
                                        Pointer pointer,
                                        bool canceled) {
  is_repost_event_on_same_item_ = false;

  if (canceled) {
    CancelDrag(-1);
  } else if (drag_pointer_ == pointer) {
    FinalizeRipOffDrag(false);
    drag_pointer_ = NONE;
    AnimateToIdealBounds();
  }
  // If the drag pointer is NONE, no drag operation is going on and the
  // drag_view can be released.
  if (drag_pointer_ == NONE)
    drag_view_ = nullptr;
}

void ShelfView::LayoutToIdealBounds() {
  if (bounds_animator_->IsAnimating()) {
    AnimateToIdealBounds();
    return;
  }

  gfx::Rect overflow_bounds;
  CalculateIdealBounds(&overflow_bounds);
  views::ViewModelUtils::SetViewBoundsToIdealBounds(*view_model_);
  overflow_button_->SetBoundsRect(overflow_bounds);
  UpdateBackButton();
  LayoutAppListAndBackButtonHighlight();
}

bool ShelfView::IsItemPinned(const ShelfItem& item) const {
  return item.type == TYPE_PINNED_APP || item.type == TYPE_BROWSER_SHORTCUT;
}

int ShelfView::GetSeparatorIndex() const {
  for (int i = 0; i < model_->item_count() - 1; ++i) {
    if (IsItemPinned(model_->items()[i]) &&
        model_->items()[i + 1].type == TYPE_APP) {
      return i;
    }
  }
  return -1;
}

int ShelfView::GetDimensionOfCenteredShelfItems() const {
  int size = 0;
  int added_items = 0;
  for (ShelfItem item : model_->items()) {
    if (item.type == TYPE_PINNED_APP || item.type == TYPE_APP ||
        item.type == TYPE_BROWSER_SHORTCUT) {
      size += ShelfConstants::button_size();
      added_items++;
    }
  }
  size += (added_items - 1) * ShelfConstants::button_spacing();
  return size;
}

void ShelfView::UpdateShelfItemBackground(SkColor color) {
  shelf_item_background_color_ = color;
  SchedulePaint();
}

void ShelfView::OnTabletModeChanged() {
  OnBoundsChanged(GetBoundsInScreen());
}

void ShelfView::UpdateAllButtonsVisibilityInOverflowMode() {
  // The overflow button is not shown in overflow mode.
  overflow_button_->SetVisible(false);
  DCHECK_LT(last_visible_index_, view_model_->view_size());
  for (int i = 0; i < view_model_->view_size(); ++i) {
    bool visible = i >= first_visible_index_ && i <= last_visible_index_;
    // To track the dragging of |drag_view_| continuously, its visibility
    // should be always true regardless of its position.
    if (dragged_to_another_shelf_ && view_model_->view_at(i) == drag_view_)
      view_model_->view_at(i)->SetVisible(true);
    else
      view_model_->view_at(i)->SetVisible(visible);
  }
}

void ShelfView::LayoutAppListAndBackButtonHighlight() const {
  // Don't show anything if this is the overflow menu.
  if (is_overflow_mode()) {
    back_and_app_list_background_->SetVisible(false);
    return;
  }
  const int button_spacing = ShelfConstants::button_spacing();
  // "Secondary" as in "orthogonal to the shelf primary axis".
  const int control_secondary_padding =
      (ShelfConstants::shelf_size() - kShelfControlSize) / 2;
  const int back_and_app_list_background_size =
      kShelfControlSize +
      (IsTabletModeEnabled() ? kShelfControlSize + button_spacing : 0);
  back_and_app_list_background_->SetBounds(
      shelf_->PrimaryAxisValue(button_spacing, control_secondary_padding),
      shelf_->PrimaryAxisValue(control_secondary_padding, button_spacing),
      shelf_->PrimaryAxisValue(back_and_app_list_background_size,
                               kShelfControlSize),
      shelf_->PrimaryAxisValue(kShelfControlSize,
                               back_and_app_list_background_size));
}

void ShelfView::CalculateIdealBounds(gfx::Rect* overflow_bounds) const {
  DCHECK(model_->item_count() == view_model_->view_size());

  const int button_spacing = ShelfConstants::button_spacing();
  const int button_size = ShelfConstants::button_size();

  const int available_size = shelf_->PrimaryAxisValue(width(), height());
  const int separator_index = GetSeparatorIndex();
  const bool virtual_keyboard_visible =
      Shell::Get()->system_tray_model()->virtual_keyboard()->visible();
  // Don't show the separator if it isn't needed, or would appear after all
  // visible items.
  separator_->SetVisible(separator_index != -1 &&
                         separator_index < last_visible_index_ &&
                         !virtual_keyboard_visible);
  int app_list_button_position;

  int x = 0;
  int y = 0;

  int w = shelf_->PrimaryAxisValue(button_size, width());
  int h = shelf_->PrimaryAxisValue(height(), button_size);

  for (int i = 0; i < view_model_->view_size(); ++i) {
    if (i < first_visible_index_) {
      // This happens for the overflow view.
      view_model_->set_ideal_bounds(i, gfx::Rect(x, y, 0, 0));
      continue;
    }
    if (i == kAppListButtonIndex + 1) {
      // Start centering after we've laid out the app list button.
      // Center the shelf items on the whole shelf, including the status
      // area widget.
      int centered_shelf_items_size = GetDimensionOfCenteredShelfItems();
      StatusAreaWidget* status_widget = shelf_widget_->status_area_widget();
      int status_widget_size =
          status_widget ? shelf_->PrimaryAxisValue(
                              status_widget->GetWindowBoundsInScreen().width(),
                              status_widget->GetWindowBoundsInScreen().height())
                        : 0;
      int padding_for_centering =
          (available_size + status_widget_size - centered_shelf_items_size) / 2;
      if (padding_for_centering >
          app_list_button_position + kAppListButtonMargin) {
        // Only shift buttons to the right, never let them interfere with the
        // left-aligned system buttons.
        x = shelf_->PrimaryAxisValue(padding_for_centering, 0);
        y = shelf_->PrimaryAxisValue(0, padding_for_centering);
      }
    }

    view_model_->set_ideal_bounds(i, gfx::Rect(x, y, w, h));
    // If not in tablet mode do not increase |x| or |y|. Instead just let the
    // next item (app list button) cover the back button, which will have
    // opacity 0 anyways.
    if (i == kBackButtonIndex && !IsTabletModeEnabled())
      continue;

    // There is no spacing between the first two elements. Do not worry about y
    // since the back button only appears in tablet mode, which forces the shelf
    // to be bottom aligned.
    x = shelf_->PrimaryAxisValue(x + w + (i == 0 ? 0 : button_spacing), x);
    y = shelf_->PrimaryAxisValue(y, y + h + button_spacing);

    // In the new UI, padding between the back & app list buttons is smaller
    // than between all other shelf items.
    if (i == kBackButtonIndex)
      x -= button_spacing;

    if (i == kAppListButtonIndex) {
      app_list_button_position = shelf_->PrimaryAxisValue(x, y);
      // A larger minimum padding after the app list button is required:
      // increment with the necessary extra amount.
      x += shelf_->PrimaryAxisValue(kAppListButtonMargin - button_spacing, 0);
      y += shelf_->PrimaryAxisValue(0, kAppListButtonMargin - button_spacing);
    }

    if (i == separator_index) {
      // Place the separator halfway between the two icons it separates,
      // vertically centered.
      int half_space = button_spacing / 2;
      int secondary_offset =
          (ShelfConstants::shelf_size() - kSeparatorSize) / 2;
      x -= shelf_->PrimaryAxisValue(half_space, 0);
      y -= shelf_->PrimaryAxisValue(0, half_space);
      separator_->SetBounds(
          x + shelf_->PrimaryAxisValue(0, secondary_offset),
          y + shelf_->PrimaryAxisValue(secondary_offset, 0),
          shelf_->PrimaryAxisValue(kSeparatorThickness, kSeparatorSize),
          shelf_->PrimaryAxisValue(kSeparatorSize, kSeparatorThickness));
      x += shelf_->PrimaryAxisValue(half_space, 0);
      y += shelf_->PrimaryAxisValue(0, half_space);
    }
  }

  if (is_overflow_mode()) {
    const_cast<ShelfView*>(this)->UpdateAllButtonsVisibilityInOverflowMode();
    return;
  }

  overflow_bounds->set_size(gfx::Size(shelf_->PrimaryAxisValue(w, width()),
                                      shelf_->PrimaryAxisValue(height(), h)));
  last_visible_index_ =
      IndexOfLastItemThatFitsSize(available_size - button_spacing);
  bool show_overflow = last_visible_index_ < model_->item_count() - 1;

  // Create space for the overflow button and place it in the last visible
  // position.
  if (show_overflow && last_visible_index_ > 0)
    --last_visible_index_;

  for (int i = 0; i < view_model_->view_size(); ++i) {
    // To receive drag event continuously from |drag_view_| during the dragging
    // off from the shelf, don't make |drag_view_| invisible. It will be
    // eventually invisible and removed from the |view_model_| by
    // FinalizeRipOffDrag().
    if (dragged_off_shelf_ && view_model_->view_at(i) == drag_view_)
      continue;
    // If virtual keyboard is visible, only back button and app list button are
    // shown.
    const bool is_visible_item = !virtual_keyboard_visible ||
                                 i == kBackButtonIndex ||
                                 i == kAppListButtonIndex;
    view_model_->view_at(i)->SetVisible(i <= last_visible_index_ &&
                                        is_visible_item);
  }

  overflow_button_->SetVisible(show_overflow);
  if (show_overflow) {
    DCHECK_NE(0, view_model_->view_size());
    if (last_visible_index_ == -1) {
      x = 0;
      y = 0;
    } else {
      x = shelf_->PrimaryAxisValue(
          view_model_->ideal_bounds(last_visible_index_).right(),
          view_model_->ideal_bounds(last_visible_index_).x());
      y = shelf_->PrimaryAxisValue(
          view_model_->ideal_bounds(last_visible_index_).y(),
          view_model_->ideal_bounds(last_visible_index_).bottom());
    }

    if (last_visible_index_ >= 0) {
      // Add more space between last visible item and overflow button.
      // Without this, two buttons look too close compared with other items.
      x = shelf_->PrimaryAxisValue(x + button_spacing, x);
      y = shelf_->PrimaryAxisValue(y, y + button_spacing);
    }

    overflow_bounds->set_x(x);
    overflow_bounds->set_y(y);
    if (overflow_bubble_.get() && overflow_bubble_->IsShowing())
      UpdateOverflowRange(overflow_bubble_->bubble_view()->shelf_view());
  } else {
    if (overflow_bubble_)
      overflow_bubble_->Hide();
  }
}

int ShelfView::IndexOfLastItemThatFitsSize(int max_value) const {
  int index = model_->item_count() - 1;
  while (index >= 0 &&
         shelf_->PrimaryAxisValue(view_model_->ideal_bounds(index).right(),
                                  view_model_->ideal_bounds(index).bottom()) >
             max_value) {
    index--;
  }
  return index;
}

void ShelfView::AnimateToIdealBounds() {
  gfx::Rect overflow_bounds;
  CalculateIdealBounds(&overflow_bounds);
  for (int i = 0; i < view_model_->view_size(); ++i) {
    View* view = view_model_->view_at(i);
    bounds_animator_->AnimateViewTo(view, view_model_->ideal_bounds(i));
    // Now that the item animation starts, we have to make sure that the
    // padding of the first gets properly transferred to the new first item.
    if (i && view->border())
      view->SetBorder(views::NullBorder());
  }
  overflow_button_->SetBoundsRect(overflow_bounds);
  LayoutAppListAndBackButtonHighlight();
}

views::View* ShelfView::CreateViewForItem(const ShelfItem& item) {
  views::View* view = nullptr;
  switch (item.type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP:
    case TYPE_DIALOG: {
      ShelfButton* button = new ShelfButton(this, this);
      button->SetImage(item.image);
      ReflectItemStatus(item, button);
      view = button;
      break;
    }

    case TYPE_APP_LIST: {
      view = new AppListButton(this, this, shelf_);
      break;
    }

    case TYPE_BACK_BUTTON: {
      view = new BackButton();
      break;
    }

    case TYPE_UNDEFINED:
      return nullptr;
  }

  if (item.type != TYPE_BACK_BUTTON)
    view->set_context_menu_controller(this);

  ConfigureChildView(view);
  return view;
}

void ShelfView::FadeIn(views::View* view) {
  view->SetVisible(true);
  view->layer()->SetOpacity(0);
  AnimateToIdealBounds();
  bounds_animator_->SetAnimationDelegate(
      view, std::unique_ptr<gfx::AnimationDelegate>(
                new FadeInAnimationDelegate(view)));
}

void ShelfView::PrepareForDrag(Pointer pointer, const ui::LocatedEvent& event) {
  DCHECK(!dragging());
  DCHECK(drag_view_);
  drag_pointer_ = pointer;
  start_drag_index_ = view_model_->GetIndexOfView(drag_view_);

  if (start_drag_index_ == -1) {
    CancelDrag(-1);
    return;
  }

  // Move the view to the front so that it appears on top of other views.
  ReorderChildView(drag_view_, -1);
  bounds_animator_->StopAnimatingView(drag_view_);

  drag_view_->OnDragStarted(&event);
}

void ShelfView::ContinueDrag(const ui::LocatedEvent& event) {
  DCHECK(dragging());
  DCHECK(drag_view_);
  // Due to a syncing operation the application might have been removed.
  // Bail if it is gone.
  int current_index = view_model_->GetIndexOfView(drag_view_);
  DCHECK_NE(-1, current_index);

  // If this is not a drag and drop host operation and not the app list item,
  // check if the item got ripped off the shelf - if it did we are done.
  if (drag_and_drop_shelf_id_.IsNull() &&
      RemovableByRipOff(current_index) != NOT_REMOVABLE) {
    if (HandleRipOffDrag(event))
      return;
    // The rip off handler could have changed the location of the item.
    current_index = view_model_->GetIndexOfView(drag_view_);
  }

  // TODO: I don't think this works correctly with RTL.
  gfx::Point drag_point(event.location());
  ConvertPointToTarget(drag_view_, this, &drag_point);

  // Constrain the location to the range of valid indices for the type.
  std::pair<int, int> indices(GetDragRange(current_index));
  int first_drag_index = indices.first;
  int last_drag_index = indices.second;
  // If the last index isn't valid, we're overflowing. Constrain to the app list
  // (which is the last visible item).
  if (first_drag_index < model_->item_count() - 1 &&
      last_drag_index > last_visible_index_)
    last_drag_index = last_visible_index_;
  int x = 0, y = 0;
  if (shelf_->IsHorizontalAlignment()) {
    x = std::max(view_model_->ideal_bounds(indices.first).x(),
                 drag_point.x() - drag_origin_.x());
    x = std::min(view_model_->ideal_bounds(last_drag_index).right() -
                     view_model_->ideal_bounds(current_index).width(),
                 x);
    if (drag_view_->x() == x)
      return;
    drag_view_->SetX(x);
  } else {
    y = std::max(view_model_->ideal_bounds(indices.first).y(),
                 drag_point.y() - drag_origin_.y());
    y = std::min(view_model_->ideal_bounds(last_drag_index).bottom() -
                     view_model_->ideal_bounds(current_index).height(),
                 y);
    if (drag_view_->y() == y)
      return;
    drag_view_->SetY(y);
  }

  int target_index = views::ViewModelUtils::DetermineMoveIndex(
      *view_model_, drag_view_,
      shelf_->IsHorizontalAlignment() ? views::ViewModelUtils::HORIZONTAL
                                      : views::ViewModelUtils::VERTICAL,
      x, y);
  target_index =
      std::min(indices.second, std::max(target_index, indices.first));

  // The back button and app list button are always first, and they are the
  // only non-draggable items.
  int first_draggable_item = model_->GetItemIndexForType(TYPE_APP_LIST) + 1;
  DCHECK_EQ(2, first_draggable_item);
  target_index = std::max(target_index, first_draggable_item);
  DCHECK_LT(target_index, model_->item_count());

  if (target_index == current_index)
    return;

  // Change the model, the ShelfItemMoved() callback will handle the
  // |view_model_| update.
  model_->Move(current_index, target_index);
  bounds_animator_->StopAnimatingView(drag_view_);
}

void ShelfView::EndDragOnOtherShelf(bool cancel) {
  if (is_overflow_mode()) {
    main_shelf_->EndDrag(cancel);
  } else {
    DCHECK(overflow_bubble_->IsShowing());
    overflow_bubble_->bubble_view()->shelf_view()->EndDrag(cancel);
  }
}

bool ShelfView::HandleRipOffDrag(const ui::LocatedEvent& event) {
  int current_index = view_model_->GetIndexOfView(drag_view_);
  DCHECK_NE(-1, current_index);
  std::string dragged_app_id = model_->items()[current_index].id.app_id;

  gfx::Point screen_location = event.root_location();
  ::wm::ConvertPointToScreen(GetWidget()->GetNativeWindow()->GetRootWindow(),
                             &screen_location);

  // To avoid ugly forwards and backwards flipping we use different constants
  // for ripping off / re-inserting the items.
  if (dragged_off_shelf_) {
    // If the shelf/overflow bubble bounds contains |screen_location| we insert
    // the item back into the shelf.
    if (GetBoundsForDragInsertInScreen().Contains(screen_location)) {
      if (dragged_to_another_shelf_) {
        // During the dragging an item from Shelf to Overflow, it can enter here
        // directly because both are located very closely.
        EndDragOnOtherShelf(true /* cancel */);

        // Stops the animation of |drag_view_| and sets its bounds explicitly
        // because ContinueDrag() stops its animation. Without this, unexpected
        // bounds will be set.
        bounds_animator_->StopAnimatingView(drag_view_);
        int drag_view_index = view_model_->GetIndexOfView(drag_view_);
        drag_view_->SetBoundsRect(view_model_->ideal_bounds(drag_view_index));
        dragged_to_another_shelf_ = false;
      }
      // Destroy our proxy view item.
      DestroyDragIconProxy();
      // Re-insert the item and return simply false since the caller will handle
      // the move as in any normal case.
      dragged_off_shelf_ = false;
      drag_view_->layer()->SetOpacity(1.0f);
      // The size of Overflow bubble should be updated immediately when an item
      // is re-inserted.
      if (is_overflow_mode())
        PreferredSizeChanged();
      return false;
    } else if (is_overflow_mode() &&
               main_shelf_->GetBoundsForDragInsertInScreen().Contains(
                   screen_location)) {
      // The item was dragged from the overflow shelf to the main shelf.
      if (!dragged_to_another_shelf_) {
        dragged_to_another_shelf_ = true;
        drag_image_->SetOpacity(1.0f);
        main_shelf_->StartDrag(dragged_app_id, screen_location);
      } else {
        main_shelf_->Drag(screen_location);
      }
    } else if (!is_overflow_mode() && overflow_bubble_ &&
               overflow_bubble_->IsShowing() &&
               overflow_bubble_->bubble_view()
                   ->shelf_view()
                   ->GetBoundsForDragInsertInScreen()
                   .Contains(screen_location)) {
      // The item was dragged from the main shelf to the overflow shelf.
      if (!dragged_to_another_shelf_) {
        dragged_to_another_shelf_ = true;
        drag_image_->SetOpacity(1.0f);
        overflow_bubble_->bubble_view()->shelf_view()->StartDrag(
            dragged_app_id, screen_location);
      } else {
        overflow_bubble_->bubble_view()->shelf_view()->Drag(screen_location);
      }
    } else if (dragged_to_another_shelf_) {
      // Makes the |drag_image_| partially disappear again.
      dragged_to_another_shelf_ = false;
      drag_image_->SetOpacity(kDraggedImageOpacity);

      EndDragOnOtherShelf(true /* cancel */);
      if (!is_overflow_mode()) {
        // During dragging, the position of the dragged item is moved to the
        // back. If the overflow bubble is showing, a copy of the dragged item
        // will appear at the end of the overflow shelf. Decrement the last
        // visible index of the overflow shelf to hide this copy.
        overflow_bubble_->bubble_view()->shelf_view()->last_visible_index_--;
      }

      bounds_animator_->StopAnimatingView(drag_view_);
      int drag_view_index = view_model_->GetIndexOfView(drag_view_);
      drag_view_->SetBoundsRect(view_model_->ideal_bounds(drag_view_index));
    }
    // Move our proxy view item.
    UpdateDragIconProxy(screen_location);
    return true;
  }

  // Mark the item as dragged off the shelf if the drag distance exceeds
  // |kRipOffDistance|, or if it's dragged between the main and overflow shelf.
  int delta = CalculateShelfDistance(screen_location);
  bool dragged_off_shelf = delta > kRipOffDistance;
  dragged_off_shelf |=
      (is_overflow_mode() &&
       main_shelf_->GetBoundsForDragInsertInScreen().Contains(screen_location));
  dragged_off_shelf |= (!is_overflow_mode() && overflow_bubble_ &&
                        overflow_bubble_->IsShowing() &&
                        overflow_bubble_->bubble_view()
                            ->shelf_view()
                            ->GetBoundsForDragInsertInScreen()
                            .Contains(screen_location));

  if (dragged_off_shelf) {
    // Create a proxy view item which can be moved anywhere.
    CreateDragIconProxy(event.root_location(), drag_view_->GetImage(),
                        drag_view_, gfx::Vector2d(0, 0),
                        kDragAndDropProxyScale);
    drag_view_->layer()->SetOpacity(0.0f);
    dragged_off_shelf_ = true;
    if (RemovableByRipOff(current_index) == REMOVABLE) {
      // Move the item to the back and hide it. ShelfItemMoved() callback will
      // handle the |view_model_| update and call AnimateToIdealBounds().
      if (current_index != model_->item_count() - 1) {
        model_->Move(current_index, model_->item_count() - 1);
        StartFadeInLastVisibleItem();

        // During dragging, the position of the dragged item is moved to the
        // back. If the overflow bubble is showing, a copy of the dragged item
        // will appear at the end of the overflow shelf. Decrement the last
        // visible index of the overflow shelf to hide this copy.
        if (overflow_bubble_ && overflow_bubble_->IsShowing())
          overflow_bubble_->bubble_view()->shelf_view()->last_visible_index_--;
      } else if (is_overflow_mode()) {
        // Overflow bubble should be shrunk when an item is ripped off.
        PreferredSizeChanged();
      }
      // Make the item partially disappear to show that it will get removed if
      // dropped.
      drag_image_->SetOpacity(kDraggedImageOpacity);
    }
    return true;
  }
  return false;
}

void ShelfView::FinalizeRipOffDrag(bool cancel) {
  if (!dragged_off_shelf_)
    return;
  // Make sure we do not come in here again.
  dragged_off_shelf_ = false;

  // Coming here we should always have a |drag_view_|.
  DCHECK(drag_view_);
  int current_index = view_model_->GetIndexOfView(drag_view_);
  // If the view isn't part of the model anymore (|current_index| == -1), a sync
  // operation must have removed it. In that case we shouldn't change the model
  // and only delete the proxy image.
  if (current_index == -1) {
    DestroyDragIconProxy();
    return;
  }

  // Set to true when the animation should snap back to where it was before.
  bool snap_back = false;
  // Items which cannot be dragged off will be handled as a cancel.
  if (!cancel) {
    if (dragged_to_another_shelf_) {
      dragged_to_another_shelf_ = false;
      EndDragOnOtherShelf(false /* cancel */);
      drag_view_->layer()->SetOpacity(1.0f);
    } else if (RemovableByRipOff(current_index) != REMOVABLE) {
      // Make sure we do not try to remove un-removable items like items which
      // were not pinned or have to be always there.
      cancel = true;
      snap_back = true;
    } else {
      // Make sure the item stays invisible upon removal.
      drag_view_->SetVisible(false);
      model_->UnpinAppWithID(model_->items()[current_index].id.app_id);
    }
  }
  if (cancel || snap_back) {
    if (dragged_to_another_shelf_) {
      dragged_to_another_shelf_ = false;
      // Other shelf handles revert of dragged item.
      EndDragOnOtherShelf(false /* true */);
      drag_view_->layer()->SetOpacity(1.0f);
    } else if (!cancelling_drag_model_changed_) {
      // Only do something if the change did not come through a model change.
      gfx::Rect drag_bounds = drag_image_->GetBoundsInScreen();
      gfx::Point relative_to = GetBoundsInScreen().origin();
      gfx::Rect target(
          gfx::PointAtOffsetFromOrigin(drag_bounds.origin() - relative_to),
          drag_bounds.size());
      drag_view_->SetBoundsRect(target);
      // Hide the status from the active item since we snap it back now. Upon
      // animation end the flag gets cleared if |snap_back_from_rip_off_view_|
      // is set.
      snap_back_from_rip_off_view_ = drag_view_;
      drag_view_->AddState(ShelfButton::STATE_HIDDEN);
      // When a canceling drag model is happening, the view model is diverged
      // from the menu model and movements / animations should not be done.
      model_->Move(current_index, start_drag_index_);
      AnimateToIdealBounds();
    }
    drag_view_->layer()->SetOpacity(1.0f);
  }
  DestroyDragIconProxy();
}

ShelfView::RemovableState ShelfView::RemovableByRipOff(int index) const {
  DCHECK(index >= 0 && index < model_->item_count());
  ShelfItemType type = model_->items()[index].type;
  if (type == TYPE_APP_LIST || type == TYPE_DIALOG || type == TYPE_BACK_BUTTON)
    return NOT_REMOVABLE;

  if (model_->items()[index].pinned_by_policy)
    return NOT_REMOVABLE;

  // Note: Only pinned app shortcuts can be removed!
  const std::string& app_id = model_->items()[index].id.app_id;
  return (type == TYPE_PINNED_APP && model_->IsAppPinned(app_id)) ? REMOVABLE
                                                                  : DRAGGABLE;
}

bool ShelfView::SameDragType(ShelfItemType typea, ShelfItemType typeb) const {
  switch (typea) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
      return (typeb == TYPE_PINNED_APP || typeb == TYPE_BROWSER_SHORTCUT);
    case TYPE_APP_LIST:
    case TYPE_APP:
    case TYPE_BACK_BUTTON:
    case TYPE_DIALOG:
      return typeb == typea;
    case TYPE_UNDEFINED:
      NOTREACHED() << "ShelfItemType must be set.";
      return false;
  }
  NOTREACHED();
  return false;
}

std::pair<int, int> ShelfView::GetDragRange(int index) {
  int min_index = -1;
  int max_index = -1;
  ShelfItemType type = model_->items()[index].type;
  for (int i = 0; i < model_->item_count(); ++i) {
    if (SameDragType(model_->items()[i].type, type)) {
      if (min_index == -1)
        min_index = i;
      max_index = i;
    }
  }
  return std::pair<int, int>(min_index, max_index);
}

void ShelfView::ConfigureChildView(views::View* view) {
  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);
}

void ShelfView::ToggleOverflowBubble() {
  if (IsShowingOverflowBubble()) {
    overflow_bubble_->Hide();
    return;
  }

  if (!overflow_bubble_)
    overflow_bubble_.reset(new OverflowBubble(shelf_));

  ShelfView* overflow_view = new ShelfView(model_, shelf_, shelf_widget_);
  overflow_view->overflow_mode_ = true;
  overflow_view->Init();
  overflow_view->set_owner_overflow_bubble(overflow_bubble_.get());
  overflow_view->OnShelfAlignmentChanged();
  overflow_view->main_shelf_ = this;
  UpdateOverflowRange(overflow_view);

  overflow_bubble_->Show(overflow_button_, overflow_view);

  shelf_->UpdateVisibilityState();
}

void ShelfView::OnFadeOutAnimationEnded() {
  AnimateToIdealBounds();
  StartFadeInLastVisibleItem();
}

void ShelfView::StartFadeInLastVisibleItem() {
  // If overflow button is visible and there is a valid new last item, fading
  // the new last item in after sliding animation is finished.
  if (overflow_button_->visible() && last_visible_index_ >= 0) {
    views::View* last_visible_view = view_model_->view_at(last_visible_index_);
    last_visible_view->layer()->SetOpacity(0);
    bounds_animator_->SetAnimationDelegate(
        last_visible_view,
        std::unique_ptr<gfx::AnimationDelegate>(
            new StartFadeAnimationDelegate(this, last_visible_view)));
  }
}

void ShelfView::UpdateOverflowRange(ShelfView* overflow_view) const {
  const int first_overflow_index = last_visible_index_ + 1;
  DCHECK_LE(first_overflow_index, model_->item_count() - 1);
  DCHECK_LT(model_->item_count() - 1, view_model_->view_size());

  overflow_view->first_visible_index_ = first_overflow_index;
  overflow_view->last_visible_index_ = model_->item_count() - 1;
}

gfx::Rect ShelfView::GetMenuAnchorRect(const views::View& source,
                                       const gfx::Point& location,
                                       bool context_menu) const {
  const bool for_item = ShelfItemForView(&source);
  const gfx::Rect shelf_bounds_in_screen =
      is_overflow_mode()
          ? owner_overflow_bubble_->bubble_view()->GetBubbleBounds()
          : GetBoundsInScreen();
  const gfx::Rect& source_bounds_in_screen = source.GetBoundsInScreen();
  // Application menus and touchable menus for items are anchored on the icon
  // bounds.
  if ((features::IsTouchableAppContextMenuEnabled() && for_item) ||
      !context_menu) {
    return source_bounds_in_screen;
  }

  gfx::Point origin;
  switch (shelf_->alignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      origin = gfx::Point(location.x(), shelf_bounds_in_screen.y());
      break;
    case SHELF_ALIGNMENT_LEFT:
      origin = gfx::Point(shelf_bounds_in_screen.right(), location.y());
      break;
    case SHELF_ALIGNMENT_RIGHT:
      origin = gfx::Point(shelf_bounds_in_screen.x(), location.y());
      break;
  }
  return gfx::Rect(origin,
                   for_item ? source_bounds_in_screen.size() : gfx::Size());
}

views::MenuAnchorPosition ShelfView::GetMenuAnchorPosition(
    bool for_item,
    bool context_menu) const {
  if (features::IsTouchableAppContextMenuEnabled()) {
    return shelf_->IsHorizontalAlignment()
               ? views::MENU_ANCHOR_BUBBLE_TOUCHABLE_ABOVE
               : views::MENU_ANCHOR_BUBBLE_TOUCHABLE_LEFT;
  }
  if (!context_menu) {
    switch (shelf_->alignment()) {
      case SHELF_ALIGNMENT_BOTTOM:
      case SHELF_ALIGNMENT_BOTTOM_LOCKED:
        return views::MENU_ANCHOR_BUBBLE_ABOVE;
      case SHELF_ALIGNMENT_LEFT:
        return views::MENU_ANCHOR_BUBBLE_RIGHT;
      case SHELF_ALIGNMENT_RIGHT:
        return views::MENU_ANCHOR_BUBBLE_LEFT;
    }
  }
  return shelf_->IsHorizontalAlignment() ? views::MENU_ANCHOR_FIXED_BOTTOMCENTER
                                         : views::MENU_ANCHOR_FIXED_SIDECENTER;
}

gfx::Rect ShelfView::GetBoundsForDragInsertInScreen() {
  gfx::Size preferred_size;
  if (is_overflow_mode()) {
    DCHECK(owner_overflow_bubble_);
    gfx::Rect bubble_bounds =
        owner_overflow_bubble_->bubble_view()->GetBubbleBounds();
    preferred_size = bubble_bounds.size();
  } else {
    const int last_button_index = view_model_->view_size() - 1;
    gfx::Rect last_button_bounds =
        view_model_->view_at(last_button_index)->bounds();
    if (overflow_button_->visible()) {
      // When overflow button is visible, last_button_bounds should be
      // overflow button's bounds.
      last_button_bounds = overflow_button_->bounds();
    }

    if (shelf_->IsHorizontalAlignment()) {
      preferred_size =
          gfx::Size(last_button_bounds.right(), ShelfConstants::shelf_size());
    } else {
      preferred_size =
          gfx::Size(ShelfConstants::shelf_size(), last_button_bounds.bottom());
    }
  }
  gfx::Point origin(GetMirroredXWithWidthInView(0, preferred_size.width()), 0);

  // In overflow mode, we should use OverflowBubbleView as a source for
  // converting |origin| to screen coordinates. When a scroll operation is
  // occurred in OverflowBubble, the bounds of ShelfView in OverflowBubble can
  // be changed.
  if (is_overflow_mode())
    ConvertPointToScreen(owner_overflow_bubble_->bubble_view(), &origin);
  else
    ConvertPointToScreen(this, &origin);

  return gfx::Rect(origin, preferred_size);
}

int ShelfView::CancelDrag(int modified_index) {
  FinalizeRipOffDrag(true);
  if (!drag_view_)
    return modified_index;
  bool was_dragging = dragging();
  int drag_view_index = view_model_->GetIndexOfView(drag_view_);
  drag_pointer_ = NONE;
  drag_view_ = nullptr;
  if (drag_view_index == modified_index) {
    // The view that was being dragged is being modified. Don't do anything.
    return modified_index;
  }
  if (!was_dragging)
    return modified_index;

  // Restore previous position, tracking the position of the modified view.
  bool at_end = modified_index == view_model_->view_size();
  views::View* modified_view = (modified_index >= 0 && !at_end)
                                   ? view_model_->view_at(modified_index)
                                   : nullptr;
  model_->Move(drag_view_index, start_drag_index_);

  // If the modified view will be at the end of the list, return the new end of
  // the list.
  if (at_end)
    return view_model_->view_size();
  return modified_view ? view_model_->GetIndexOfView(modified_view) : -1;
}

gfx::Size ShelfView::CalculatePreferredSize() const {
  gfx::Rect overflow_bounds;
  CalculateIdealBounds(&overflow_bounds);

  int last_button_index = last_visible_index_;
  if (!is_overflow_mode() && overflow_button_ && overflow_button_->visible())
    ++last_button_index;

  // When an item is dragged off from the overflow bubble, it is moved to last
  // position and and changed to invisible. Overflow bubble size should be
  // shrunk to fit only for visible items.
  // If |dragged_to_another_shelf_| is set, there will be no
  // invisible items in the shelf.
  if (is_overflow_mode() && dragged_off_shelf_ && !dragged_to_another_shelf_ &&
      RemovableByRipOff(view_model_->GetIndexOfView(drag_view_)) == REMOVABLE)
    last_button_index--;

  const gfx::Rect last_button_bounds =
      last_button_index >= first_visible_index_
          ? view_model_->ideal_bounds(last_button_index)
          : gfx::Rect(gfx::Size(ShelfConstants::shelf_size(),
                                ShelfConstants::shelf_size()));

  if (shelf_->IsHorizontalAlignment())
    return gfx::Size(last_button_bounds.right(), ShelfConstants::shelf_size());

  return gfx::Size(ShelfConstants::shelf_size(), last_button_bounds.bottom());
}

void ShelfView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // This bounds change is produced by the shelf movement (rotation, alignment
  // change, etc.) and all content has to follow. Using an animation at that
  // time would produce a time lag since the animation of the BoundsAnimator has
  // itself a delay before it arrives at the required location. As such we tell
  // the animator to go there immediately. We still want to use an animation
  // when the bounds change is caused by entering or exiting tablet mode.
  if (shelf_->is_tablet_mode_animation_running()) {
    AnimateToIdealBounds();
    if (IsShowingOverflowBubble()) {
      overflow_bubble_->bubble_view()->shelf_view()->OnBoundsChanged(
          previous_bounds);
    }
    return;
  }

  BoundsAnimatorDisabler disabler(bounds_animator_.get());

  LayoutToIdealBounds();
  shelf_->NotifyShelfIconPositionsChanged();

  if (IsShowingOverflowBubble())
    overflow_bubble_->Hide();
}

views::FocusTraversable* ShelfView::GetPaneFocusTraversable() {
  return this;
}

void ShelfView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kToolbar;
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ASH_SHELF_ACCESSIBLE_NAME));
}

void ShelfView::OnGestureEvent(ui::GestureEvent* event) {
  // Do not forward events to |shelf_| (which forwards events to the shelf
  // layout manager) as we do not want gestures on the overflow to open the app
  // list for example.
  if (is_overflow_mode()) {
    main_shelf_->overflow_bubble()->bubble_view()->ProcessGestureEvent(*event);
    event->StopPropagation();
    return;
  }

  // Convert the event location from current view to screen, since swiping up on
  // the shelf can open the fullscreen app list. Updating the bounds of the app
  // list during dragging is based on screen coordinate space.
  gfx::Point location_in_screen(event->location());
  View::ConvertPointToScreen(this, &location_in_screen);
  event->set_location(location_in_screen);
  if (shelf_->ProcessGestureEvent(*event))
    event->StopPropagation();
}

bool ShelfView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  shelf_->ProcessMouseWheelEvent(event);
  return true;
}

void ShelfView::ShelfItemAdded(int model_index) {
  {
    base::AutoReset<bool> cancelling_drag(&cancelling_drag_model_changed_,
                                          true);
    model_index = CancelDrag(model_index);
  }
  const ShelfItem& item(model_->items()[model_index]);
  views::View* view = CreateViewForItem(item);
  AddChildView(view);
  // Hide the view, it'll be made visible when the animation is done. Using
  // opacity 0 here to avoid messing with CalculateIdealBounds which touches
  // the view's visibility.
  view->layer()->SetOpacity(0);
  view_model_->Add(view, model_index);

  // Give the button its ideal bounds. That way if we end up animating the
  // button before this animation completes it doesn't appear at some random
  // spot (because it was in the middle of animating from 0,0 0x0 to its
  // target).
  gfx::Rect overflow_bounds;
  CalculateIdealBounds(&overflow_bounds);
  view->SetBoundsRect(view_model_->ideal_bounds(model_index));

  if (ShouldShowShelfItem(item)) {
    // The first animation moves all the views to their target position. |view|
    // is hidden, so it visually appears as though we are providing space for
    // it. When done we'll fade the view in.
    AnimateToIdealBounds();
    if (model_index <= last_visible_index_) {
      bounds_animator_->SetAnimationDelegate(
          view, std::unique_ptr<gfx::AnimationDelegate>(
                    new StartFadeAnimationDelegate(this, view)));
    } else {
      // Undo the hiding if animation does not run.
      view->layer()->SetOpacity(1.0f);
    }
  } else {
    view->SetVisible(false);
  }
}

void ShelfView::ShelfItemRemoved(int model_index, const ShelfItem& old_item) {
  if (old_item.id == context_menu_id_ && shelf_menu_model_adapter_)
    shelf_menu_model_adapter_->Cancel();

  views::View* view = view_model_->view_at(model_index);
  view_model_->Remove(model_index);

  {
    base::AutoReset<bool> cancelling_drag(&cancelling_drag_model_changed_,
                                          true);
    CancelDrag(-1);
  }

  // When the overflow bubble is visible, the overflow range needs to be set
  // before CalculateIdealBounds() gets called. Otherwise CalculateIdealBounds()
  // could trigger a ShelfItemChanged() by hiding the overflow bubble and
  // since the overflow bubble is not yet synced with the ShelfModel this
  // could cause a crash.
  if (overflow_bubble_ && overflow_bubble_->IsShowing()) {
    UpdateOverflowRange(overflow_bubble_->bubble_view()->shelf_view());
  }

  if (view->visible()) {
    // The first animation fades out the view. When done we'll animate the rest
    // of the views to their target location.
    bounds_animator_->AnimateViewTo(view, view->bounds());
    bounds_animator_->SetAnimationDelegate(
        view, std::unique_ptr<gfx::AnimationDelegate>(
                  new FadeOutAnimationDelegate(this, view)));
  } else {
    // We don't need to show a fade out animation for invisible |view|. When an
    // item is ripped out from the shelf, its |view| is already invisible.
    AnimateToIdealBounds();
  }

  if (view == tooltip_.GetCurrentAnchorView())
    tooltip_.Close();
}

void ShelfView::ShelfItemChanged(int model_index, const ShelfItem& old_item) {
  const ShelfItem& item(model_->items()[model_index]);

  // Bail if the view and shelf sizes do not match. ShelfItemChanged may be
  // called here before ShelfItemAdded, due to ChromeLauncherController's
  // item initialization, which calls SetItem during ShelfItemAdded.
  if (static_cast<int>(model_->items().size()) != view_model_->view_size())
    return;

  if (old_item.type != item.type) {
    // Type changed, swap the views.
    model_index = CancelDrag(model_index);
    std::unique_ptr<views::View> old_view(view_model_->view_at(model_index));
    bounds_animator_->StopAnimatingView(old_view.get());
    // Removing and re-inserting a view in our view model will strip the ideal
    // bounds from the item. To avoid recalculation of everything the bounds
    // get remembered and restored after the insertion to the previous value.
    gfx::Rect old_ideal_bounds = view_model_->ideal_bounds(model_index);
    view_model_->Remove(model_index);
    views::View* new_view = CreateViewForItem(item);
    AddChildView(new_view);
    view_model_->Add(new_view, model_index);
    view_model_->set_ideal_bounds(model_index, old_ideal_bounds);

    bool new_item_is_visible = ShouldShowShelfItem(item);
    if (ShouldShowShelfItem(old_item) != new_item_is_visible) {
      views::View* view = view_model_->view_at(model_index);
      view->SetVisible(new_item_is_visible);
      if (!new_item_is_visible) {
        // Nothing else to do.
        return;
      }
    }

    new_view->SetBoundsRect(old_view->bounds());
    if (overflow_button_ && overflow_button_->visible())
      AnimateToIdealBounds();
    else
      bounds_animator_->AnimateViewTo(new_view, old_ideal_bounds);

    // If an item is being pinned or unpinned, show the new status of the
    // shelf immediately so that the separator gets drawn as needed.
    if (old_item.type == TYPE_PINNED_APP || item.type == TYPE_PINNED_APP)
      AnimateToIdealBounds();
    return;
  }

  views::View* view = view_model_->view_at(model_index);
  switch (item.type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP:
    case TYPE_DIALOG: {
      CHECK_EQ(ShelfButton::kViewClassName, view->GetClassName());
      ShelfButton* button = static_cast<ShelfButton*>(view);
      ReflectItemStatus(item, button);
      button->SetImage(item.image);
      button->SchedulePaint();
      break;
    }

    default:
      break;
  }
}

void ShelfView::ShelfItemMoved(int start_index, int target_index) {
  view_model_->Move(start_index, target_index);
  // When cancelling a drag due to a shelf item being added, the currently
  // dragged item is moved back to its initial position. AnimateToIdealBounds
  // will be called again when the new item is added to the |view_model_| but
  // at this time the |view_model_| is inconsistent with the |model_|.
  if (!cancelling_drag_model_changed_)
    AnimateToIdealBounds();
}

void ShelfView::ShelfItemDelegateChanged(const ShelfID& id,
                                         ShelfItemDelegate* old_delegate,
                                         ShelfItemDelegate* delegate) {}

void ShelfView::ShelfItemStatusChanged(const ShelfID& id) {
  int index = model_->ItemIndexByID(id);
  if (index < 0)
    return;

  const ShelfItem item = model_->items()[index];
  views::View* view = view_model_->view_at(index);
  CHECK_EQ(ShelfButton::kViewClassName, view->GetClassName());
  // TODO(manucornet): Add a helper to get the button.
  ShelfButton* button = static_cast<ShelfButton*>(view);
  ReflectItemStatus(item, button);
  button->SchedulePaint();
}

void ShelfView::AfterItemSelected(
    const ShelfItem& item,
    views::Button* sender,
    std::unique_ptr<ui::Event> event,
    views::InkDrop* ink_drop,
    ShelfAction action,
    base::Optional<std::vector<mojom::MenuItemPtr>> menu_items) {
  shelf_button_pressed_metric_tracker_.ButtonPressed(*event, sender, action);

  // The app list handles its own ink drop effect state changes.
  if (action != SHELF_ACTION_APP_LIST_SHOWN) {
    if (action != SHELF_ACTION_NEW_WINDOW_CREATED && menu_items &&
        menu_items->size() > 1) {
      // Show the app menu with 2 or more items, if no window was created.
      ink_drop->AnimateToState(views::InkDropState::ACTIVATED);
      context_menu_id_ = item.id;
      ShowMenu(std::make_unique<ShelfApplicationMenuModel>(
                   item.title, std::move(*menu_items),
                   model_->GetShelfItemDelegate(item.id)),
               sender, gfx::Point(), false,
               ui::GetMenuSourceTypeForEvent(*event));
    } else {
      ink_drop->AnimateToState(views::InkDropState::ACTION_TRIGGERED);
    }
  }
  scoped_root_window_for_new_windows_.reset();
}

void ShelfView::AfterGetContextMenuItems(
    const ShelfID& shelf_id,
    const gfx::Point& point,
    views::View* source,
    ui::MenuSourceType source_type,
    std::vector<mojom::MenuItemPtr> menu_items) {
  context_menu_id_ = shelf_id;
  const int64_t display_id = GetDisplayIdForView(this);
  std::unique_ptr<ShelfContextMenuModel> menu_model =
      std::make_unique<ShelfContextMenuModel>(
          std::move(menu_items), model_->GetShelfItemDelegate(shelf_id),
          display_id);
  ShowMenu(std::move(menu_model), source, point, true /* context_menu */,
           source_type);
}

void ShelfView::ShowContextMenuForView(views::View* source,
                                       const gfx::Point& point,
                                       ui::MenuSourceType source_type) {
  last_pressed_index_ = -1;
  const ShelfItem* item = ShelfItemForView(source);
  const int64_t display_id = GetDisplayIdForView(this);
  if (!item || !model_->GetShelfItemDelegate(item->id)) {
    context_menu_id_ = ShelfID();
    std::unique_ptr<ShelfContextMenuModel> menu_model =
        std::make_unique<ShelfContextMenuModel>(
            std::vector<mojom::MenuItemPtr>(), nullptr, display_id);
    ShowMenu(std::move(menu_model), source, point, true, source_type);
    return;
  }

  // Get any custom entries; show the context menu in AfterGetContextMenuItems.
  model_->GetShelfItemDelegate(item->id)->GetContextMenuItems(
      display_id, base::Bind(&ShelfView::AfterGetContextMenuItems,
                             weak_factory_.GetWeakPtr(), item->id, point,
                             source, source_type));
}

void ShelfView::ShowMenu(std::unique_ptr<ui::SimpleMenuModel> menu_model,
                         views::View* source,
                         const gfx::Point& click_point,
                         bool context_menu,
                         ui::MenuSourceType source_type) {
  DCHECK(!IsShowingMenu());
  if (menu_model->GetItemCount() == 0)
    return;
  menu_owner_ = source;

  closing_event_time_ = base::TimeTicks();

  // NOTE: If you convert to HAS_MNEMONICS be sure to update menu building code.
  int run_types = 0;
  if (context_menu) {
    run_types |=
        views::MenuRunner::CONTEXT_MENU | views::MenuRunner::FIXED_ANCHOR;
  }

  // Only use the touchable layout if the menu is for an app.
  if (features::IsTouchableAppContextMenuEnabled())
    run_types |= views::MenuRunner::USE_TOUCHABLE_LAYOUT;

  const ShelfItem* item = ShelfItemForView(source);
  // Only selected shelf items with context menu opened can be dragged.
  if (context_menu && item && ShelfButtonIsInDrag(item->type, source) &&
      source_type == ui::MenuSourceType::MENU_SOURCE_TOUCH) {
    run_types |= views::MenuRunner::SEND_GESTURE_EVENTS_TO_OWNER;
  }

  shelf_menu_model_adapter_ = std::make_unique<ShelfMenuModelAdapter>(
      item ? item->id.app_id : std::string(), std::move(menu_model), source,
      source_type,
      base::BindOnce(&ShelfView::OnMenuClosed, base::Unretained(this), source));
  shelf_menu_model_adapter_->Run(
      GetMenuAnchorRect(*source, click_point, context_menu),
      GetMenuAnchorPosition(item, context_menu), run_types);
}

void ShelfView::OnMenuClosed(views::View* source) {
  menu_owner_ = nullptr;
  context_menu_id_ = ShelfID();

  closing_event_time_ = shelf_menu_model_adapter_->GetClosingEventTime();

  const ShelfItem* item = ShelfItemForView(source);
  if (item && (item->type != TYPE_APP_LIST))
    static_cast<ShelfButton*>(source)->OnMenuClosed();

  shelf_menu_model_adapter_.reset();

  // Auto-hide or alignment might have changed, but only for this shelf.
  shelf_->UpdateVisibilityState();
}

void ShelfView::OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) {
  shelf_->NotifyShelfIconPositionsChanged();
  PreferredSizeChanged();

  if (shelf_->is_tablet_mode_animation_running()) {
    float opacity = 0.f;
    const gfx::SlideAnimation* animation =
        bounds_animator_->GetAnimationForView(GetBackButton());
    if (animation)
      opacity = static_cast<float>(animation->GetCurrentValue());
    if (!IsTabletModeEnabled())
      opacity = 1.f - opacity;

    GetBackButton()->layer()->SetOpacity(opacity);
    GetBackButton()->SetFocusBehavior(FocusBehavior::ALWAYS);
  }
}

void ShelfView::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  shelf_->set_is_tablet_mode_animation_running(false);

  if (snap_back_from_rip_off_view_ && animator == bounds_animator_.get()) {
    if (!animator->IsAnimating(snap_back_from_rip_off_view_)) {
      // Coming here the animation of the ShelfButton is finished and the
      // previously hidden status can be shown again. Since the button itself
      // might have gone away or changed locations we check that the button
      // is still in the shelf and show its status again.
      for (int index = 0; index < view_model_->view_size(); index++) {
        views::View* view = view_model_->view_at(index);
        if (view == snap_back_from_rip_off_view_) {
          CHECK_EQ(ShelfButton::kViewClassName, view->GetClassName());
          ShelfButton* button = static_cast<ShelfButton*>(view);
          button->ClearState(ShelfButton::STATE_HIDDEN);
          break;
        }
      }
      snap_back_from_rip_off_view_ = nullptr;
    }
  }

  UpdateBackButton();
}

bool ShelfView::IsRepostEvent(const ui::Event& event) {
  if (closing_event_time_.is_null())
    return false;

  // If the current (press down) event is a repost event, the time stamp of
  // these two events should be the same.
  return closing_event_time_ == event.time_stamp();
}

bool ShelfView::ShouldShowShelfItem(const ShelfItem& item) {
  // We only consider hiding shelf items in tablet mode.
  if (!IsTabletModeEnabled()) {
    return true;
  }
  // We also don't do any hiding if the relevant flag is off.
  if (!chromeos::switches::ShouldHideActiveAppsFromShelf()) {
    return true;
  }
  // Hide running apps that aren't also pinned.
  return item.type != TYPE_APP;
}

const ShelfItem* ShelfView::ShelfItemForView(const views::View* view) const {
  const int view_index = view_model_->GetIndexOfView(view);
  return (view_index < 0) ? nullptr : &(model_->items()[view_index]);
}

int ShelfView::CalculateShelfDistance(const gfx::Point& coordinate) const {
  const gfx::Rect bounds = GetBoundsInScreen();
  int distance = shelf_->SelectValueForShelfAlignment(
      bounds.y() - coordinate.y(), coordinate.x() - bounds.right(),
      bounds.x() - coordinate.x());
  return distance > 0 ? distance : 0;
}

bool ShelfView::CanPrepareForDrag(Pointer pointer,
                                  const ui::LocatedEvent& event) {
  // Bail if dragging has already begun, or if no item has been pressed.
  if (dragging() || !drag_view_)
    return false;

  // Dragging only begins once the pointer has travelled a minimum distance.
  if ((std::abs(event.x() - drag_origin_.x()) < kMinimumDragDistance) &&
      (std::abs(event.y() - drag_origin_.y()) < kMinimumDragDistance)) {
    return false;
  }

  return true;
}

void ShelfView::UpdateBackButton() {
  const bool virtual_keyboard_visible =
      Shell::Get()->system_tray_model()->virtual_keyboard()->visible();
  gfx::Transform rotation;
  // Rotate the back button when virtual keyboard is visible.
  if (virtual_keyboard_visible) {
    rotation.Rotate(270.0);
    rotation.Translate(-GetBackButton()->height(), 0);
  }
  GetBackButton()->layer()->SetOpacity(IsTabletModeEnabled() ? 1.f : 0.f);
  GetBackButton()->layer()->SetTransform(rotation);
  GetBackButton()->SetFocusBehavior(
      IsTabletModeEnabled() ? FocusBehavior::ALWAYS : FocusBehavior::NEVER);
}

void ShelfView::SetDragImageBlur(const gfx::Size& size, int blur_radius) {
  drag_image_->SetPaintToLayer();
  drag_image_->layer()->SetFillsBoundsOpaquely(false);
  drag_image_mask_ = views::Painter::CreatePaintedLayer(
      views::Painter::CreateSolidRoundRectPainter(SK_ColorBLACK,
                                                  size.width() / 2.0f));
  drag_image_mask_->layer()->SetBounds(gfx::Rect(size));
  drag_image_->layer()->SetMaskLayer(drag_image_mask_->layer());
  drag_image_->layer()->SetBackgroundBlur(blur_radius);
}

}  // namespace ash
