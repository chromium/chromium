// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/drag_drop/drag_image_view.h"
#include "ash/keyboard/keyboard_util.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_application_menu_model.h"
#include "ash/shelf/shelf_button.h"
#include "ash/shelf/shelf_context_menu_model.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_menu_model_adapter.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/adapters.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/animation_metrics_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
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

// The distance of the cursor from the outer rim of the shelf before it
// separates.
constexpr int kRipOffDistance = 48;

// The rip off drag and drop proxy image should get scaled by this factor.
constexpr float kDragAndDropProxyScale = 1.2f;

// The opacity represents that this partially disappeared item will get removed.
constexpr float kDraggedImageOpacity = 0.5f;

namespace {

// White with ~20% opacity.
constexpr SkColor kSeparatorColor = SkColorSetARGB(0x32, 0xFF, 0xFF, 0xFF);

// The dimensions, in pixels, of the separator between pinned and unpinned
// items.
constexpr int kSeparatorSize = 20;
constexpr int kSeparatorThickness = 1;

constexpr char kShelfIconMoveAnimationHistogram[] =
    "Ash.ShelfIcon.AnimationSmoothness.Move";
constexpr char kShelfIconFadeInAnimationHistogram[] =
    "Ash.ShelfIcon.AnimationSmoothness.FadeIn";
constexpr char kShelfIconFadeOutAnimationHistogram[] =
    "Ash.ShelfIcon.AnimationSmoothness.FadeOut";

enum class IconAnimationType {
  kMoveAnimation,
  kFadeInAnimation,
  kFadeOutAnimation
};

// Helper to check if tablet mode is enabled.
bool IsTabletModeEnabled() {
  return Shell::Get()->tablet_mode_controller() &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

// A class to temporarily disable a given bounds animator.
class BoundsAnimatorDisabler {
 public:
  explicit BoundsAnimatorDisabler(views::BoundsAnimator* bounds_animator)
      : old_duration_(bounds_animator->GetAnimationDuration()),
        bounds_animator_(bounds_animator) {
    bounds_animator_->SetAnimationDuration(
        base::TimeDelta::FromMilliseconds(1));
  }

  ~BoundsAnimatorDisabler() {
    bounds_animator_->SetAnimationDuration(old_duration_);
  }

 private:
  // The previous animation duration.
  base::TimeDelta old_duration_;
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
    // Build a list of all the views that we are able to focus.
    std::vector<views::View*> focusable_views;

    for (int i : shelf_view_->visible_views_indices())
      focusable_views.push_back(shelf_view_->view_model()->view_at(i));

    // Where are we starting from?
    int start_index = 0;
    for (size_t i = 0; i < focusable_views.size(); ++i) {
      if (focusable_views[i] == starting_view) {
        start_index = i;
        break;
      }
    }
    int new_index =
        start_index +
        (search_direction == FocusSearch::SearchDirection::kBackwards ? -1 : 1);
    // Loop around.
    if (new_index < 0)
      new_index = focusable_views.size() - 1;
    else if (new_index >= static_cast<int>(focusable_views.size()))
      new_index = 0;

    return focusable_views[new_index];
  }

 private:
  ShelfView* shelf_view_;

  DISALLOW_COPY_AND_ASSIGN(ShelfFocusSearch);
};

class IconAnimationMetricsReporter : public ui::AnimationMetricsReporter {
 public:
  explicit IconAnimationMetricsReporter(IconAnimationType type) : type_(type) {}
  IconAnimationMetricsReporter(const IconAnimationMetricsReporter&) = delete;
  IconAnimationMetricsReporter& operator=(const IconAnimationMetricsReporter&) =
      delete;
  ~IconAnimationMetricsReporter() override = default;

 private:
  void Report(int value) override {
    switch (type_) {
      case IconAnimationType::kMoveAnimation:
        base::UmaHistogramPercentage(kShelfIconMoveAnimationHistogram, value);
        break;
      case IconAnimationType::kFadeInAnimation:
        base::UmaHistogramPercentage(kShelfIconFadeInAnimationHistogram, value);
        break;
      case IconAnimationType::kFadeOutAnimation:
        base::UmaHistogramPercentage(kShelfIconFadeOutAnimationHistogram,
                                     value);
        break;
    }
  }

  IconAnimationType type_ = IconAnimationType::kMoveAnimation;
};

// Returns the id of the display on which |view| is shown.
int64_t GetDisplayIdForView(const View* view) {
  aura::Window* window = view->GetWidget()->GetNativeWindow();
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
}

// Whether |item_view| is a ShelfAppButton and its state is STATE_DRAGGING.
bool ShelfButtonIsInDrag(const ShelfItemType item_type,
                         const views::View* item_view) {
  switch (item_type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP:
      return static_cast<const ShelfAppButton*>(item_view)->state() &
             ShelfAppButton::STATE_DRAGGING;
    case TYPE_DIALOG:
    case TYPE_UNDEFINED:
      return false;
  }
}

// Called back by the shelf item delegates to determine whether an app menu item
// should be included in the shelf app menu given its corresponding window. This
// is used to filter out items whose windows are on inactive desks when the per-
// desk shelf feature is enabled.
bool ShouldIncludeMenuItem(aura::Window* window) {
  if (!features::IsPerDeskShelfEnabled())
    return true;
  return desks_util::BelongsToActiveDesk(window);
}

// Returns true if the app associated with |app_id| is a Remote App.
bool IsRemoteApp(const std::string& app_id) {
  AccountId account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  apps::AppRegistryCache* cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id);
  return cache && cache->GetAppType(app_id) == apps::mojom::AppType::kRemote;
}

}  // namespace

// ImplicitAnimationObserver used when adding an item.
class ShelfView::FadeInAnimationDelegate
    : public ui::ImplicitAnimationObserver {
 public:
  explicit FadeInAnimationDelegate(ShelfView* shelf_view)
      : shelf_view_(shelf_view) {}
  ~FadeInAnimationDelegate() override { StopObservingImplicitAnimations(); }

 private:
  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    shelf_view_->OnFadeInAnimationEnded();
  }

  ShelfView* shelf_view_ = nullptr;
};

// AnimationDelegate used when deleting an item. This steadily decreased the
// opacity of the layer as the animation progress.
class ShelfView::FadeOutAnimationDelegate : public gfx::AnimationDelegate {
 public:
  FadeOutAnimationDelegate(ShelfView* host, std::unique_ptr<views::View> view)
      : shelf_view_(host), view_(std::move(view)) {}
  ~FadeOutAnimationDelegate() override = default;

  // AnimationDelegate overrides:
  void AnimationProgressed(const Animation* animation) override {
    view_->layer()->SetOpacity(1 - animation->GetCurrentValue());
  }
  void AnimationEnded(const Animation* animation) override {
    // Ensures that |view| is not used after destruction.
    shelf_view_->StopAnimatingViewIfAny(view_.get());

    // Remove the view which has been faded away.
    view_.reset();

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

ShelfView::ShelfView(ShelfModel* model,
                     Shelf* shelf,
                     ApplicationDragAndDropHost* drag_and_drop_host,
                     ShelfButtonDelegate* shelf_button_delegate)
    : model_(model),
      shelf_(shelf),
      view_model_(std::make_unique<views::ViewModel>()),
      bounds_animator_(
          std::make_unique<views::BoundsAnimator>(this,
                                                  /*use_transforms=*/true)),
      focus_search_(std::make_unique<ShelfFocusSearch>(this)),
      drag_and_drop_host_(drag_and_drop_host),
      shelf_button_delegate_(shelf_button_delegate) {
  DCHECK(model_);
  DCHECK(shelf_);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
  bounds_animator_->AddObserver(this);
  bounds_animator_->SetAnimationDuration(
      ShelfConfig::Get()->shelf_animation_duration());
  set_context_menu_controller(this);
  set_allow_deactivate_on_esc(true);

  announcement_view_ = new views::View();
  AddChildView(announcement_view_);
}

ShelfView::~ShelfView() {
  // Shell destroys the TabletModeController before destroying all root windows.
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  bounds_animator_->RemoveObserver(this);
  model_->RemoveObserver(this);
}

int ShelfView::GetSizeOfAppButtons(int count, int button_size) {
  const int button_spacing = ShelfConfig::Get()->button_spacing();

  if (count == 0)
    return 0;

  const int app_size = count * button_size;
  int total_padding = button_spacing * (count - 1);
  return app_size + total_padding;
}

void ShelfView::Init() {
  move_animation_reporter_ = std::make_unique<IconAnimationMetricsReporter>(
      IconAnimationType::kMoveAnimation);
  fade_in_animation_reporter_ = std::make_unique<IconAnimationMetricsReporter>(
      IconAnimationType::kFadeInAnimation);
  fade_out_animation_reporter_ = std::make_unique<IconAnimationMetricsReporter>(
      IconAnimationType::kFadeOutAnimation);

  separator_ = new views::Separator();
  separator_->SetColor(kSeparatorColor);
  separator_->SetPreferredHeight(kSeparatorSize);
  separator_->SetVisible(false);
  ConfigureChildView(separator_, ui::LAYER_TEXTURED);
  AddChildView(separator_);

  model()->AddObserver(this);

  const ShelfItems& items(model_->items());

  for (ShelfItems::const_iterator i = items.begin(); i != items.end(); ++i) {
    views::View* child = CreateViewForItem(*i);
    child->SetPaintToLayer();
    int index = static_cast<int>(i - items.begin());
    view_model_->Add(child, index);
    // Add child view so it has the same ordering as in the |view_model_|.
    AddChildViewAt(child, index);
  }

  fade_in_animation_delegate_ = std::make_unique<FadeInAnimationDelegate>(this);

  // We'll layout when our bounds change.
}

gfx::Rect ShelfView::GetIdealBoundsOfItemIcon(const ShelfID& id) {
  int index = model_->ItemIndexByID(id);
  if (!base::Contains(visible_views_indices_, index))
    return gfx::Rect();

  const gfx::Rect& ideal_bounds(view_model_->ideal_bounds(index));
  ShelfAppButton* button = GetShelfAppButton(id);
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

void ShelfView::UpdateVisibleShelfItemBoundsUnion() {
  visible_shelf_item_bounds_union_.SetRect(0, 0, 0, 0);
  for (const int i : visible_views_indices_) {
    const views::View* child = view_model_->view_at(i);
    if (ShouldShowTooltipForChildView(child)) {
      visible_shelf_item_bounds_union_.Union(
          GetChildViewTargetMirroredBounds(child));
    }
  }
}

bool ShelfView::ShouldShowTooltipForView(const views::View* view) const {
  if (!view || !view->parent())
    return false;

  if (view->parent() == this)
    return ShouldShowTooltipForChildView(view);

  return false;
}

ShelfAppButton* ShelfView::GetShelfAppButton(const ShelfID& id) {
  const int index = model_->ItemIndexByID(id);
  if (index < 0)
    return nullptr;

  views::View* const view = view_model_->view_at(index);
  DCHECK_EQ(ShelfAppButton::kViewClassName, view->GetClassName());
  return static_cast<ShelfAppButton*>(view);
}

void ShelfView::StopAnimatingViewIfAny(views::View* view) {
  if (bounds_animator_->IsAnimating(view))
    bounds_animator_->StopAnimatingView(view);
}

bool ShelfView::IsShelfViewHandlingDragAndDrop() const {
  // If the ShelfView has a drag icon proxy, the drag originated from the
  // AppsGridView. When the drag originates from the shelf, the
  // ScrollableShelfView is the ApplicationDragAndDropHost, so ShelfView will
  // not have a drag proxy.
  return !!drag_image_widget_;
}

int ShelfView::GetButtonSize() const {
  return ShelfConfig::Get()->GetShelfButtonSize(
      shelf_->hotseat_widget()->target_hotseat_density());
}

int ShelfView::GetButtonIconSize() const {
  return ShelfConfig::Get()->GetShelfButtonIconSize(
      shelf_->hotseat_widget()->target_hotseat_density());
}

int ShelfView::GetShelfItemRippleSize() const {
  return GetButtonSize() +
         2 * ShelfConfig::Get()->scrollable_shelf_ripple_padding();
}

void ShelfView::LayoutIfAppIconsOffsetUpdates() {
  if (app_icons_layout_offset_ != CalculateAppIconsLayoutOffset())
    LayoutToIdealBounds();
}

ShelfAppButton* ShelfView::GetShelfItemViewWithContextMenu() {
  if (context_menu_id_.IsNull())
    return nullptr;
  const int item_index = model_->ItemIndexByID(context_menu_id_);
  if (item_index < 0)
    return nullptr;
  return static_cast<ShelfAppButton*>(view_model_->view_at(item_index));
}

void ShelfView::AnnounceShelfItemNotificationBadge(views::View* button) {
  announcement_view_->GetViewAccessibility().OverrideName(
      l10n_util::GetStringFUTF16(IDS_SHELF_ITEM_HAS_NOTIFICATION_BADGE,
                                 GetTitleForView(button)));
  announcement_view_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                               /*send_native_event=*/true);
}

bool ShelfView::ShouldHideTooltip(const gfx::Point& cursor_location) const {
  // There are thin gaps between launcher buttons but the tooltip shouldn't hide
  // in the gaps, but the tooltip should hide if the mouse moved totally outside
  // of the buttons area.

  return !visible_shelf_item_bounds_union_.Contains(cursor_location);
}

const std::vector<aura::Window*> ShelfView::GetOpenWindowsForView(
    views::View* view) {
  std::vector<aura::Window*> window_list =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);
  std::vector<aura::Window*> open_windows;
  const ShelfItem* item = ShelfItemForView(view);

  // The concept of a list of open windows doesn't make sense for something
  // that isn't an app shortcut: return an empty list.
  if (!item)
    return open_windows;

  for (auto* window : window_list) {
    const std::string window_app_id =
        ShelfID::Deserialize(window->GetProperty(kShelfIDKey)).app_id;
    if (window_app_id == item->id.app_id) {
      // TODO: In the very first version we only show one window. Add the proper
      // UI to show all windows for a given open app.
      open_windows.push_back(window);
    }
  }
  return open_windows;
}

base::string16 ShelfView::GetTitleForView(const views::View* view) const {
  if (view->parent() == this)
    return GetTitleForChildView(view);

  return base::string16();
}

views::View* ShelfView::GetViewForEvent(const ui::Event& event) {
  if (event.target() == GetWidget()->GetNativeWindow())
    return this;

  return nullptr;
}

gfx::Rect ShelfView::GetVisibleItemsBoundsInScreen() {
  gfx::Size preferred_size = GetPreferredSize();
  gfx::Point origin(GetMirroredXWithWidthInView(0, preferred_size.width()), 0);
  ConvertPointToScreen(this, &origin);
  return gfx::Rect(origin, preferred_size);
}

gfx::Size ShelfView::CalculatePreferredSize() const {
  const int hotseat_size = shelf_->hotseat_widget()->GetHotseatSize();
  if (visible_views_indices_.empty()) {
    // There are no visible shelf items.
    return shelf_->IsHorizontalAlignment() ? gfx::Size(0, hotseat_size)
                                           : gfx::Size(hotseat_size, 0);
  }

  const gfx::Rect last_button_bounds =
      view_model_->ideal_bounds(visible_views_indices_.back());

  if (shelf_->IsHorizontalAlignment())
    return gfx::Size(last_button_bounds.right(), hotseat_size);

  return gfx::Size(hotseat_size, last_button_bounds.bottom());
}

void ShelfView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // This bounds change is produced by the shelf movement (rotation, alignment
  // change, etc.) and all content has to follow. Using an animation at that
  // time would produce a time lag since the animation of the BoundsAnimator has
  // itself a delay before it arrives at the required location. As such we tell
  // the animator to go there immediately. We still want to use an animation
  // when the bounds change is caused by entering or exiting tablet mode, with
  // an exception of usage within the scrollable shelf. With scrollable shelf
  // (and hotseat), tablet mode transition causes hotseat bounds changes, so
  // animating shelf items as well would introduce a lag.

  BoundsAnimatorDisabler disabler(bounds_animator_.get());

  LayoutToIdealBounds();
  shelf_->NotifyShelfIconPositionsChanged();
}

bool ShelfView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.IsControlDown() &&
      keyboard_util::IsArrowKeyCode(event.key_code())) {
    bool swap_with_next = (event.key_code() == ui::VKEY_DOWN ||
                           event.key_code() == ui::VKEY_RIGHT);
    SwapButtons(GetFocusManager()->GetFocusedView(), swap_with_next);
    return true;
  }
  return views::View::OnKeyPressed(event);
}

void ShelfView::OnMouseEvent(ui::MouseEvent* event) {
  gfx::Point location_in_screen(event->location());
  View::ConvertPointToScreen(this, &location_in_screen);

  switch (event->type()) {
    case ui::ET_MOUSEWHEEL:
      // The mousewheel event is handled by the ScrollableShelfView.
      break;
    case ui::ET_MOUSE_PRESSED:
      if (!event->IsOnlyLeftMouseButton()) {
        if (event->IsOnlyRightMouseButton()) {
          ShowContextMenuForViewImpl(this, location_in_screen,
                                     ui::MENU_SOURCE_MOUSE);
          event->SetHandled();
        }
        return;
      }

      FALLTHROUGH;
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_RELEASED:
      // Convert the event location from current view to screen, since dragging
      // the shelf by mouse can open the fullscreen app list. Updating the
      // bounds of the app list during dragging is based on screen coordinate
      // space.
      event->set_location(location_in_screen);

      event->SetHandled();
      shelf_->ProcessMouseEvent(*event->AsMouseEvent());
      break;
    default:
      break;
  }
}

views::FocusTraversable* ShelfView::GetPaneFocusTraversable() {
  // ScrollableShelfView should handles the focus traversal.
  return nullptr;
}

const char* ShelfView::GetClassName() const {
  return "ShelfView";
}

void ShelfView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kToolbar;
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ASH_SHELF_ACCESSIBLE_NAME));
}

View* ShelfView::GetTooltipHandlerForPoint(const gfx::Point& point) {
  // Similar implementation as views::View, but without going into each
  // child's subviews.
  View::Views children = GetChildrenInZOrder();
  for (auto* child : base::Reversed(children)) {
    if (!child->GetVisible())
      continue;

    gfx::Point point_in_child_coords(point);
    ConvertPointToTarget(this, child, &point_in_child_coords);
    if (child->HitTestPoint(point_in_child_coords) &&
        ShouldShowTooltipForChildView(child)) {
      return child;
    }
  }
  // If none of our children qualifies, just return the shelf view itself.
  return this;
}

void ShelfView::OnShelfButtonAboutToRequestFocusFromTabTraversal(
    ShelfButton* button,
    bool reverse) {
  if (ShouldFocusOut(reverse, button)) {
    shelf_->shelf_focus_cycler()->FocusOut(reverse, SourceView::kShelfView);
  }
}

void ShelfView::ButtonPressed(views::Button* sender,
                              const ui::Event& event,
                              views::InkDrop* ink_drop) {
  if (!ShouldEventActivateButton(sender, event)) {
    ink_drop->SnapToHidden();
    return;
  }

  // Prevent concurrent requests that may show application or context menus.
  if (!item_awaiting_response_.IsNull()) {
    const ShelfItem* item = ShelfItemForView(sender);
    if (item && item->id != item_awaiting_response_)
      ink_drop->AnimateToState(views::InkDropState::DEACTIVATED);
    return;
  }

  // Ensure the keyboard is hidden and stays hidden (as long as it isn't locked)
  if (keyboard::KeyboardUIController::Get()->IsEnabled())
    keyboard::KeyboardUIController::Get()->HideKeyboardExplicitlyBySystem();

  // Record the index for the last pressed shelf item.
  last_pressed_index_ = view_model_->GetIndexOfView(sender);
  DCHECK_LT(-1, last_pressed_index_);

  // Place new windows on the same display as the button. Opening windows is
  // usually an async operation so we wait until window activation changes
  // (ShelfItemStatusChanged) before destroying the scoped object. Post a task
  // to destroy the scoped object just in case the window activation event does
  // not get fired.
  aura::Window* window = sender->GetWidget()->GetNativeWindow();
  scoped_display_for_new_windows_ =
      std::make_unique<display::ScopedDisplayForNewWindows>(
          window->GetRootWindow());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ShelfView::DestroyScopedDisplay,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(100));

  // Slow down activation animations if Control key is pressed.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> slowing_animations;
  if (event.IsControlDown()) {
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
                      SHELF_ACTION_NONE, {});
    return;
  }

  // Notify the item of its selection; handle the result in AfterItemSelected.
  item_awaiting_response_ = item.id;
  model_->GetShelfItemDelegate(item.id)->ItemSelected(
      ui::Event::Clone(event), GetDisplayIdForView(this), LAUNCH_FROM_SHELF,
      base::BindOnce(&ShelfView::AfterItemSelected, weak_factory_.GetWeakPtr(),
                     item, sender, ui::Event::Clone(event), ink_drop),
      base::BindRepeating(&ShouldIncludeMenuItem));
}

bool ShelfView::IsShowingMenuForView(const views::View* view) const {
  return IsShowingMenu() &&
         shelf_menu_model_adapter_->IsShowingMenuForView(*view);
}

////////////////////////////////////////////////////////////////////////////////
// ShelfView, FocusTraversable implementation:

views::FocusSearch* ShelfView::GetFocusSearch() {
  return focus_search_.get();
}

////////////////////////////////////////////////////////////////////////////////
// ShelfView, AccessiblePaneView implementation:

views::View* ShelfView::GetDefaultFocusableChild() {
  return default_last_focusable_child_ ? FindLastFocusableChild()
                                       : FindFirstFocusableChild();
}

void ShelfView::ShowContextMenuForViewImpl(views::View* source,
                                           const gfx::Point& point,
                                           ui::MenuSourceType source_type) {
  // Prevent concurrent requests that may show application or context menus.
  const ShelfItem* item = ShelfItemForView(source);
  if (!item_awaiting_response_.IsNull()) {
    if (item && item->id != item_awaiting_response_) {
      static_cast<views::Button*>(source)->AnimateInkDrop(
          views::InkDropState::DEACTIVATED, nullptr);
    }
    return;
  }
  last_pressed_index_ = -1;
  if (!item || !model_->GetShelfItemDelegate(item->id)) {
    ShowShelfContextMenu(ShelfID(), point, source, source_type, nullptr);
    return;
  }

  item_awaiting_response_ = item->id;
  context_menu_callback_.Reset(base::BindOnce(
      &ShelfView::ShowShelfContextMenu, weak_factory_.GetWeakPtr(), item->id,
      point, source, source_type));

  const int64_t display_id = GetDisplayIdForView(this);
  model_->GetShelfItemDelegate(item->id)->GetContextMenu(
      display_id, context_menu_callback_.callback());
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

void ShelfView::OnShelfConfigUpdated() {
  // Ensure the shelf app buttons have an icon which is up to date with the
  // current ShelfConfig sizing.
  for (int i = 0; i < view_model_->view_size(); i++) {
    ShelfAppButton* button =
        static_cast<ShelfAppButton*>(view_model_->view_at(i));
    if (!button->IsIconSizeCurrent())
      ShelfItemChanged(i, model_->items()[i]);
  }
}

bool ShelfView::ShouldEventActivateButton(View* view, const ui::Event& event) {
  // This only applies to app buttons.
  DCHECK_EQ(ShelfAppButton::kViewClassName, view->GetClassName());
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

  const bool repost = IsRepostEvent(event);

  // Ignore if this is a repost event on the last pressed shelf item.
  int index = view_model_->GetIndexOfView(view);
  if (index == -1)
    return false;
  return !repost || last_pressed_index_ != index;
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
  drag_image_widget_ =
      DragImageView::Create(root_window, ui::mojom::DragEventSource::kMouse);
  DragImageView* drag_image = GetDragImage();
  if (blur_radius > 0)
    SetDragImageBlur(icon.size(), blur_radius);
  drag_image->SetImage(icon);
  gfx::Size size = drag_image->GetPreferredSize();
  size.set_width(size.width() * scale_factor);
  size.set_height(size.height() * scale_factor);
  gfx::Rect drag_image_bounds(origin_in_screen_coordinates, size);
  drag_image->SetBoundsInScreen(drag_image_bounds);

  // Turn off the default visibility animation.
  drag_image_widget_->SetVisibilityAnimationTransition(
      views::Widget::ANIMATE_NONE);
  drag_image->SetWidgetVisible(true);
  // Add a layer in order to ensure the icon properly animates when a drag
  // starts from AppsGridView and ends in the Shelf.
  drag_image->SetPaintToLayer();
  drag_image->layer()->SetFillsBoundsOpaquely(false);
}

void ShelfView::UpdateDragIconProxy(
    const gfx::Point& location_in_screen_coordinates) {
  // TODO(jennyz): Investigate why drag_image_widget_ becomes null at this point
  // per crbug.com/34722, while the app list item is still being dragged around.
  if (drag_image_widget_) {
    GetDragImage()->SetScreenPosition(location_in_screen_coordinates -
                                      drag_image_offset_);
  }
}

void ShelfView::UpdateDragIconProxyByLocation(
    const gfx::Point& origin_in_screen_coordinates) {
  if (drag_image_widget_)
    GetDragImage()->SetScreenPosition(origin_in_screen_coordinates);
}

bool ShelfView::IsDraggedView(const views::View* view) const {
  return drag_view_ == view;
}

views::View* ShelfView::FindFirstFocusableChild() {
  if (visible_views_indices_.empty())
    return nullptr;
  return view_model_->view_at(visible_views_indices_.front());
}

views::View* ShelfView::FindLastFocusableChild() {
  if (visible_views_indices_.empty())
    return nullptr;
  return view_model_->view_at(visible_views_indices_.back());
}

views::View* ShelfView::FindFirstOrLastFocusableChild(bool last) {
  return last ? FindLastFocusableChild() : FindFirstFocusableChild();
}

bool ShelfView::HandleGestureEvent(const ui::GestureEvent* event) {
  // Avoid changing |event|'s location since |event| may be received by post
  // event handlers.
  ui::GestureEvent copy_event(*event);

  // Convert the event location from current view to screen, since swiping up on
  // the shelf can open the fullscreen app list. Updating the bounds of the app
  // list during dragging is based on screen coordinate space.
  gfx::Point location_in_screen(copy_event.location());
  View::ConvertPointToScreen(this, &location_in_screen);
  copy_event.set_location(location_in_screen);

  if (shelf_->ProcessGestureEvent(copy_event))
    return true;

  return false;
}

bool ShelfView::ShouldShowTooltipForChildView(
    const views::View* child_view) const {
  DCHECK_EQ(this, child_view->parent());

  // Don't show a tooltip for a view that's currently being dragged.
  if (child_view == drag_view_)
    return false;

  return ShelfItemForView(child_view) && !IsShowingMenuForView(child_view);
}

// static
void ShelfView::ConfigureChildView(views::View* view,
                                   ui::LayerType layer_type) {
  view->SetPaintToLayer(layer_type);
  view->layer()->SetFillsBoundsOpaquely(false);
}

void ShelfView::CalculateIdealBounds() {
  DCHECK(model()->item_count() == view_model_->view_size());

  const int button_spacing = ShelfConfig::Get()->button_spacing();
  UpdateSeparatorIndex();

  const int hotseat_size = shelf_->hotseat_widget()->GetHotseatSize();

  // Don't show the separator if it isn't needed, or would appear after all
  // visible items.
  separator_->SetVisible(separator_index_ != -1 &&
                         separator_index_ < visible_views_indices_.back());
  // Set |separator_index_| to -1 if it is not visible.
  if (!separator_->GetVisible())
    separator_index_ = -1;

  app_icons_layout_offset_ = CalculateAppIconsLayoutOffset();
  int x = shelf()->PrimaryAxisValue(app_icons_layout_offset_, 0);
  int y = shelf()->PrimaryAxisValue(0, app_icons_layout_offset_);

  // The padding is handled in ScrollableShelfView.

  const int button_size = GetButtonSize();
  for (int i = 0; i < view_model_->view_size(); ++i) {
    const bool is_visible = view_model_->view_at(i)->GetVisible();
    if (!is_visible) {
      // Layout hidden views with empty bounds so they don't consume horizontal
      // space. Note that |separator_index_| cannot be the index of a hidden
      // view.
      DCHECK_NE(i, separator_index_);
      view_model_->set_ideal_bounds(i, gfx::Rect(x, y, 0, 0));
      continue;
    }

    view_model_->set_ideal_bounds(i, gfx::Rect(x, y, button_size, button_size));

    x = shelf()->PrimaryAxisValue(x + button_size + button_spacing, x);
    y = shelf()->PrimaryAxisValue(y, y + button_size + button_spacing);

    if (i == separator_index_) {
      // Place the separator halfway between the two icons it separates,
      // vertically centered.
      int half_space = button_spacing / 2;
      int secondary_offset = (hotseat_size - kSeparatorSize) / 2;
      x -= shelf()->PrimaryAxisValue(half_space, 0);
      y -= shelf()->PrimaryAxisValue(0, half_space);
      separator_->SetBounds(
          x + shelf()->PrimaryAxisValue(0, secondary_offset),
          y + shelf()->PrimaryAxisValue(secondary_offset, 0),
          shelf()->PrimaryAxisValue(kSeparatorThickness, kSeparatorSize),
          shelf()->PrimaryAxisValue(kSeparatorSize, kSeparatorThickness));
      x += shelf()->PrimaryAxisValue(half_space, 0);
      y += shelf()->PrimaryAxisValue(0, half_space);
    }
  }
}

views::View* ShelfView::CreateViewForItem(const ShelfItem& item) {
  views::View* view = nullptr;
  switch (item.type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP:
    case TYPE_DIALOG: {
      ShelfAppButton* button = new ShelfAppButton(
          this, shelf_button_delegate_ ? shelf_button_delegate_ : this);
      button->SetImage(item.image);
      button->ReflectItemStatus(item);
      view = button;
      break;
    }

    case TYPE_UNDEFINED:
      return nullptr;
  }

  view->set_context_menu_controller(this);

  ConfigureChildView(view, ui::LAYER_NOT_DRAWN);
  return view;
}

int ShelfView::GetAvailableSpaceForAppIcons() const {
  return shelf()->PrimaryAxisValue(width(), height());
}

void ShelfView::UpdateSeparatorIndex() {
  // A separator is shown after the last pinned item only if it's followed by a
  // visible app item.
  int first_unpinned_index = -1;
  int last_pinned_index = -1;

  int dragged_item_index = -1;
  if (drag_view_)
    dragged_item_index = view_model_->GetIndexOfView(drag_view_);

  const bool can_drag_view_across_separator =
      drag_view_ && CanDragAcrossSeparator(drag_view_);
  for (int i = model()->item_count() - 1; i >= 0; --i) {
    const auto& item = model()->items()[i];
    if (IsItemPinned(item)) {
      // Dragged pinned item may be moved to the unpinned side of the shelf and
      // may end up right of an unpinned app. Dismisses the dragged item to
      // check the next one.
      if (i == dragged_item_index && can_drag_view_across_separator)
        continue;

      last_pinned_index = i;
      break;
    }

    if (item.type == TYPE_APP && item.is_on_active_desk)
      first_unpinned_index = i;
  }

  // If there is no unpinned item in shelf, return -1 as the separator should be
  // hidden.
  if (first_unpinned_index == -1) {
    separator_index_ = -1;
    return;
  }

  // If the dragged item is between the pinned apps and unpinned apps, move it
  // to the pinned app side if it is closer to the pinned section compared to
  // its ideal bounds.
  if (can_drag_view_across_separator &&
      last_pinned_index < dragged_item_index &&
      dragged_item_index <= first_unpinned_index &&
      drag_view_relative_to_ideal_bounds_ == RelativePosition::kLeft) {
    separator_index_ = dragged_item_index;
    return;
  }

  separator_index_ = last_pinned_index;
}

void ShelfView::DestroyDragIconProxy() {
  drag_image_widget_.reset();
  drag_image_offset_ = gfx::Vector2d(0, 0);
}

views::UniqueWidgetPtr
ShelfView::RetrieveDragIconProxyAndClearDragProxyState() {
  // TODO(https://crub.com/1045186): Make ScrollableShelfView the only
  // ApplicationDragAndDropHost in the view hierarchy, and remove this.
  views::UniqueWidgetPtr temp_drag_image_view = std::move(drag_image_widget_);
  DestroyDragIconProxy();
  return temp_drag_image_view;
}

bool ShelfView::ShouldStartDrag(
    const std::string& app_id,
    const gfx::Point& location_in_screen_coordinates) const {
  // Remote Apps are not pinnable.
  if (IsRemoteApp(app_id))
    return false;

  // Do not start drag if an operation is already going on - or the cursor is
  // not inside. This could happen if mouse / touch operations overlap.
  return (drag_and_drop_shelf_id_.IsNull() && !app_id.empty() &&
          GetBoundsInScreen().Contains(location_in_screen_coordinates));
}

bool ShelfView::StartDrag(const std::string& app_id,
                          const gfx::Point& location_in_screen_coordinates) {
  if (!ShouldStartDrag(app_id, location_in_screen_coordinates))
    return false;

  // If the AppsGridView (which was dispatching this event) was opened by our
  // button, ShelfView dragging operations are locked and we have to unlock.
  CancelDrag(-1);
  drag_and_drop_item_pinned_ = false;
  drag_and_drop_shelf_id_ = ShelfID(app_id);
  // Check if the application is pinned - if not, we have to pin it so
  // that we can re-arrange the shelf order accordingly. Note that items have
  // to be pinned to give them the same (order) possibilities as a shortcut.
  if (!model_->IsAppPinned(app_id)) {
    ShelfModel::ScopedUserTriggeredMutation user_triggered(model_);
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
  const gfx::Point start_point_in_screen =
      drag_and_drop_view->GetBoundsInScreen().CenterPoint();
  gfx::Point pt = start_point_in_screen;
  views::View::ConvertPointFromScreen(drag_and_drop_view, &pt);
  gfx::Point point_in_root = start_point_in_screen;
  wm::ConvertPointFromScreen(
      window_util::GetRootWindowAt(location_in_screen_coordinates),
      &point_in_root);
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
  wm::ConvertPointFromScreen(
      window_util::GetRootWindowAt(location_in_screen_coordinates),
      &point_in_root);
  ui::MouseEvent event(ui::ET_MOUSE_DRAGGED, pt, point_in_root,
                       ui::EventTimeForNow(), 0, 0);
  PointerDraggedOnButton(drag_and_drop_view, DRAG_AND_DROP, event);
  return true;
}

void ShelfView::EndDrag(bool cancel) {
  drag_scroll_dir_ = 0;
  scrolling_timer_.Stop();
  speed_up_drag_scrolling_.Stop();

  if (drag_and_drop_shelf_id_.IsNull())
    return;

  views::View* drag_and_drop_view =
      view_model_->view_at(model_->ItemIndexByID(drag_and_drop_shelf_id_));
  PointerReleasedOnButton(drag_and_drop_view, DRAG_AND_DROP, cancel);

  // Either destroy the temporarily created item - or - make the item visible.
  if (drag_and_drop_item_pinned_ && cancel) {
    ShelfModel::ScopedUserTriggeredMutation user_triggered(model_);
    model_->UnpinAppWithID(drag_and_drop_shelf_id_.app_id);
  } else if (drag_and_drop_view) {
    std::unique_ptr<gfx::AnimationDelegate> animation_delegate;

    // Resets the dragged view's opacity at the end of drag. Otherwise, if
    // the app is already pinned on shelf before drag starts, the dragged view
    // will be invisible when drag ends.
    animation_delegate =
        std::make_unique<StartFadeAnimationDelegate>(this, drag_and_drop_view);

    if (cancel) {
      // When a hosted drag gets canceled, the item can remain in the same slot
      // and it might have moved within the bounds. In that case the item need
      // to animate back to its correct location.
      AnimateToIdealBounds();
      bounds_animator_->SetAnimationDelegate(drag_and_drop_view,
                                             std::move(animation_delegate));
    } else {
      drag_and_drop_view->SetSize(pre_drag_and_drop_size_);
    }
  }

  drag_and_drop_shelf_id_ = ShelfID();
}

void ShelfView::SwapButtons(views::View* button_to_swap, bool with_next) {
  if (!button_to_swap)
    return;

  // Find the index of the button to swap in the view model.
  int src_index = -1;
  for (int i = 0; i < view_model_->view_size(); ++i) {
    View* view = view_model_->view_at(i);
    if (view == button_to_swap) {
      src_index = i;
      break;
    }
  }

  // Swapping items in the model is sufficient, everything will then be
  // reflected in the views.
  if (model_->Swap(src_index, with_next)) {
    AnimateToIdealBounds();
    const ShelfItem src_item = model_->items()[src_index];
    const ShelfItem dst_item =
        model_->items()[src_index + (with_next ? 1 : -1)];
    AnnounceSwapEvent(src_item, dst_item);
  }
}

void ShelfView::PointerPressedOnButton(views::View* view,
                                       Pointer pointer,
                                       const ui::LocatedEvent& event) {
  if (drag_view_)
    return;

  if (IsShowingMenu())
    shelf_menu_model_adapter_->Cancel();

  int index = view_model_->GetIndexOfView(view);
  if (index == -1 || view_model_->view_size() < 1)
    return;  // View is being deleted, ignore request.

  // Only when the repost event occurs on the same shelf item, we should ignore
  // the call in ShelfView::ButtonPressed(...).
  is_repost_event_on_same_item_ =
      IsRepostEvent(event) && (last_pressed_index_ == index);

  CHECK_EQ(ShelfAppButton::kViewClassName, view->GetClassName());
  drag_view_ = static_cast<ShelfAppButton*>(view);
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
  drag_scroll_dir_ = 0;
  scrolling_timer_.Stop();
  speed_up_drag_scrolling_.Stop();

  is_repost_event_on_same_item_ = false;

  if (canceled) {
    CancelDrag(-1);
  } else if (drag_pointer_ == pointer) {
    FinalizeRipOffDrag(false);
    drag_pointer_ = NONE;

    // Check if the pin status of |drag_view_| should be changed when
    // |drag_view_| is dragged over the separator. Do nothing if |drag_view_| is
    // already handled in FinalizedRipOffDrag.
    if (drag_view_) {
      if (ShouldUpdateDraggedViewPinStatus(view_model_->GetIndexOfView(view))) {
        const std::string drag_app_id = ShelfItemForView(drag_view_)->id.app_id;
        ShelfModel::ScopedUserTriggeredMutation user_triggered(model_);
        if (model_->IsAppPinned(drag_app_id)) {
          model_->UnpinAppWithID(drag_app_id);
        } else {
          model_->PinAppWithID(drag_app_id);
        }
      }
    }
    AnimateToIdealBounds();
  }

  if (drag_pointer_ != NONE)
    return;

  drag_and_drop_host_->DestroyDragIconProxy();

  // If the drag pointer is NONE, no drag operation is going on and the
  // |drag_view_| can be released.
  drag_view_ = nullptr;
  drag_view_relative_to_ideal_bounds_ = RelativePosition::kNotAvailable;
}

void ShelfView::LayoutToIdealBounds() {
  if (bounds_animator_->IsAnimating()) {
    AnimateToIdealBounds();
    return;
  }

  CalculateIdealBounds();
  views::ViewModelUtils::SetViewBoundsToIdealBounds(*view_model_);
  UpdateVisibleShelfItemBoundsUnion();
}

bool ShelfView::IsItemPinned(const ShelfItem& item) const {
  return IsPinnedShelfItemType(item.type);
}

bool ShelfView::IsItemVisible(const ShelfItem& item) const {
  return IsItemPinned(item) || item.is_on_active_desk;
}

void ShelfView::OnTabletModeChanged() {
  // The layout change will happen as part of shelf config update.
}

void ShelfView::AnimateToIdealBounds() {
  CalculateIdealBounds();
  bounds_animator_->SetAnimationMetricsReporter(move_animation_reporter_.get());

  for (int i = 0; i < view_model_->view_size(); ++i) {
    View* view = view_model_->view_at(i);
    bounds_animator_->AnimateViewTo(view, view_model_->ideal_bounds(i));
    // Now that the item animation starts, we have to make sure that the
    // padding of the first gets properly transferred to the new first item.
    if (view->border())
      view->SetBorder(views::NullBorder());
  }
  UpdateVisibleShelfItemBoundsUnion();
}

void ShelfView::FadeIn(views::View* view) {
  view->SetVisible(true);
  view->layer()->SetOpacity(0);

  ui::ScopedLayerAnimationSettings fade_in_animation_settings(
      view->layer()->GetAnimator());
  fade_in_animation_settings.SetTweenType(gfx::Tween::EASE_OUT);
  fade_in_animation_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  fade_in_animation_settings.AddObserver(fade_in_animation_delegate_.get());
  fade_in_animation_settings.SetAnimationMetricsReporter(
      fade_in_animation_reporter_.get());
  view->layer()->SetOpacity(1.f);
}

void ShelfView::PrepareForDrag(Pointer pointer, const ui::LocatedEvent& event) {
  DCHECK(!dragging());
  DCHECK(drag_view_);
  drag_pointer_ = pointer;
  start_drag_index_ = view_model_->GetIndexOfView(drag_view_);
  drag_scroll_dir_ = 0;

  if (start_drag_index_ == -1) {
    CancelDrag(-1);
    return;
  }

  // Cancel in-flight request for app item context menu model (made when app
  // context menu is requested), to prevent the pending callback from showing
  // a context menu just after drag starts.
  if (!context_menu_callback_.IsCancelled()) {
    context_menu_callback_.Cancel();
    item_awaiting_response_ = ShelfID();
  }

  // Move the view to the front so that it appears on top of other views.
  ReorderChildView(drag_view_, -1);
  bounds_animator_->StopAnimatingView(drag_view_);

  drag_view_->OnDragStarted(&event);

  drag_view_->layer()->SetOpacity(0.0f);
  drag_and_drop_host_->CreateDragIconProxyByLocationWithNoAnimation(
      event.root_location(), drag_view_->GetImage(), drag_view_,
      /*scale_factor=*/1.0f, /*blur_radius=*/0);
}

void ShelfView::ContinueDrag(const ui::LocatedEvent& event) {
  DCHECK(dragging());
  DCHECK(drag_view_);
  DCHECK_NE(-1, view_model_->GetIndexOfView(drag_view_));

  const bool dragged_off_shelf_before = dragged_off_shelf_;

  // Handle rip off functionality if this is not a drag and drop host operation
  // and not the app list item.
  if (drag_and_drop_shelf_id_.IsNull() &&
      RemovableByRipOff(view_model_->GetIndexOfView(drag_view_)) !=
          NOT_REMOVABLE) {
    HandleRipOffDrag(event);
    // Check if the item got ripped off the shelf - if it did we are done.
    if (dragged_off_shelf_) {
      drag_scroll_dir_ = 0;
      scrolling_timer_.Stop();
      speed_up_drag_scrolling_.Stop();
      if (!dragged_off_shelf_before)
        model_->OnItemRippedOff();
      return;
    }
  }

  // Calculates the drag point in screen before MoveDragViewTo is called.
  gfx::Point drag_point_in_screen(event.location());
  ConvertPointToScreen(drag_view_, &drag_point_in_screen);

  gfx::Point drag_point(event.location());
  ConvertPointToTarget(drag_view_, this, &drag_point);
  MoveDragViewTo(shelf_->PrimaryAxisValue(drag_point.x() - drag_origin_.x(),
                                          drag_point.y() - drag_origin_.y()));
  drag_and_drop_host_->UpdateDragIconProxy(drag_point_in_screen -
                                           drag_origin_.OffsetFromOrigin());
  if (dragged_off_shelf_before)
    model_->OnItemReturnedFromRipOff(view_model_->GetIndexOfView(drag_view_));
}

void ShelfView::MoveDragViewTo(int primary_axis_coordinate) {
  const int current_item_index = view_model_->GetIndexOfView(drag_view_);
  const std::pair<int, int> indices(GetDragRange(current_item_index));
  if (shelf_->IsHorizontalAlignment()) {
    int x = GetMirroredXWithWidthInView(primary_axis_coordinate,
                                        drag_view_->width());
    x = std::max(view_model_->ideal_bounds(indices.first).x(), x);
    x = std::min(view_model_->ideal_bounds(indices.second).right() -
                     view_model_->ideal_bounds(current_item_index).width(),
                 x);
    if (drag_view_->x() != x)
      drag_view_->SetX(x);
  } else {
    int y = std::max(view_model_->ideal_bounds(indices.first).y(),
                     primary_axis_coordinate);
    y = std::min(view_model_->ideal_bounds(indices.second).bottom() -
                     view_model_->ideal_bounds(current_item_index).height(),
                 y);
    if (drag_view_->y() != y)
      drag_view_->SetY(y);
  }

  int target_index = views::ViewModelUtils::DetermineMoveIndex(
      *view_model_, drag_view_, shelf_->IsHorizontalAlignment(),
      drag_view_->x(), drag_view_->y());
  target_index =
      base::ClampToRange(target_index, indices.first, indices.second);

  // Check the relative position of |drag_view_| and its ideal bounds if it can
  // be dragged across the separator to pin or unpin.
  if (CanDragAcrossSeparator(drag_view_)) {
    // Compare the center points of |drag_view_| and its ideal bounds to
    // determine whether the separator should be moved to the left or right by
    // using |drag_view_relative_to_ideal_bounds_|. The actual position will
    // be updated in CalculateIdealBounds.
    gfx::Point drag_view_center = drag_view_->bounds().CenterPoint();
    int drag_view_position =
        shelf()->PrimaryAxisValue(drag_view_center.x(), drag_view_center.y());
    gfx::Point ideal_bound_center =
        view_model_->ideal_bounds(target_index).CenterPoint();
    int ideal_bound_position = shelf()->PrimaryAxisValue(
        ideal_bound_center.x(), ideal_bound_center.y());

    drag_view_relative_to_ideal_bounds_ =
        drag_view_position < ideal_bound_position ? RelativePosition::kLeft
                                                  : RelativePosition::kRight;
    if (target_index == current_item_index) {
      AnimateToIdealBounds();
      NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                               true /* send_native_event */);
    }
  }

  if (target_index == current_item_index)
    return;
  // Change the model if the dragged item index is changed, the ShelfItemMoved()
  // callback will handle the |view_model_| update.
  model_->Move(current_item_index, target_index);
  bounds_animator_->StopAnimatingView(drag_view_);
}

void ShelfView::CreateDragIconProxy(
    const gfx::Point& location_in_screen_coordinates,
    const gfx::ImageSkia& icon,
    views::View* replaced_view,
    const gfx::Vector2d& cursor_offset_from_center,
    float scale_factor,
    bool animate_visibility) {
  drag_replaced_view_ = replaced_view;
  aura::Window* root_window =
      drag_replaced_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
  drag_image_widget_ =
      DragImageView::Create(root_window, ui::mojom::DragEventSource::kMouse);
  DragImageView* drag_image = GetDragImage();
  drag_image->SetImage(icon);
  gfx::Size size = drag_image->GetPreferredSize();
  size.set_width(std::round(size.width() * scale_factor));
  size.set_height(std::round(size.height() * scale_factor));
  drag_image_offset_ = gfx::Vector2d(size.width() / 2, size.height() / 2) +
                       cursor_offset_from_center;
  gfx::Rect drag_image_bounds(
      location_in_screen_coordinates - drag_image_offset_, size);
  drag_image->SetBoundsInScreen(drag_image_bounds);
  if (!animate_visibility) {
    drag_image_widget_->SetVisibilityAnimationTransition(
        views::Widget::ANIMATE_NONE);
  }
  drag_image->SetWidgetVisible(true);
}

void ShelfView::HandleRipOffDrag(const ui::LocatedEvent& event) {
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
      drag_and_drop_host_->CreateDragIconProxyByLocationWithNoAnimation(
          event.root_location(), drag_view_->GetImage(), GetDragImage(),
          /*scale_factor=*/1.0f, /*blur_radius=*/0);

      // Destroy our proxy view item.
      DestroyDragIconProxy();
      // Re-insert the item and return simply false since the caller will handle
      // the move as in any normal case.
      dragged_off_shelf_ = false;

      return;
    }
    // Move our proxy view item.
    UpdateDragIconProxy(screen_location);
    return;
  }

  // Mark the item as dragged off the shelf if the drag distance exceeds
  // |kRipOffDistance|.
  int delta = CalculateShelfDistance(screen_location);
  bool dragged_off_shelf = delta > kRipOffDistance;

  if (dragged_off_shelf) {
    // Replaces a proxy icon provided by drag_and_drop_host_ - keep cursor
    // position consistent with the host provided icon, and disable
    // visibility animations (to prevent the proxy icon from lingering on
    // when replaced with the icon provided by the host).
    const gfx::Point center = drag_view_->GetLocalBounds().CenterPoint();
    const gfx::Vector2d cursor_offset_from_center = drag_origin_ - center;
    // Create a proxy view item which can be moved anywhere.
    CreateDragIconProxy(event.root_location(), drag_view_->GetImage(),
                        drag_view_, cursor_offset_from_center,
                        kDragAndDropProxyScale, /*animate_visibility=*/false);

    dragged_off_shelf_ = true;

    drag_and_drop_host_->DestroyDragIconProxy();

    if (RemovableByRipOff(current_index) == REMOVABLE) {
      // Move the item to the back and hide it. ShelfItemMoved() callback will
      // handle the |view_model_| update and call AnimateToIdealBounds().
      if (current_index != model_->item_count() - 1)
        model_->Move(current_index, model_->item_count() - 1);
      // Make the item partially disappear to show that it will get removed if
      // dropped.
      GetDragImage()->SetOpacity(kDraggedImageOpacity);
    }
  }
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
    if (RemovableByRipOff(current_index) != REMOVABLE) {
      // Make sure we do not try to remove un-removable items like items which
      // were not pinned or have to be always there.
      cancel = true;
      snap_back = true;
    } else {
      // Make sure the item stays invisible upon removal.
      drag_view_->SetVisible(false);
      ShelfModel::ScopedUserTriggeredMutation user_triggered(model_);
      model_->UnpinAppWithID(model_->items()[current_index].id.app_id);
    }
  }
  if (cancel || snap_back) {
    if (!cancelling_drag_model_changed_) {
      // Only do something if the change did not come through a model change.
      gfx::Rect drag_bounds = GetDragImage()->GetBoundsInScreen();
      gfx::Point relative_to = GetBoundsInScreen().origin();
      gfx::Rect target(
          gfx::PointAtOffsetFromOrigin(drag_bounds.origin() - relative_to),
          drag_bounds.size());
      drag_view_->SetBoundsRect(target);
      // Hide the status from the active item since we snap it back now. Upon
      // animation end the flag gets cleared if |snap_back_from_rip_off_view_|
      // is set.
      snap_back_from_rip_off_view_ = drag_view_;
      drag_view_->AddState(ShelfAppButton::STATE_HIDDEN);
      // When a canceling drag model is happening, the view model is diverged
      // from the menu model and movements / animations should not be done.
      model_->Move(current_index, start_drag_index_);
      AnimateToIdealBounds();
    }
    drag_view_->layer()->SetOpacity(1.0f);
    model_->OnItemReturnedFromRipOff(model_->item_count() - 1);
  }
  DestroyDragIconProxy();
}

ShelfView::RemovableState ShelfView::RemovableByRipOff(int index) const {
  DCHECK(index >= 0 && index < model_->item_count());
  ShelfItemType type = model_->items()[index].type;
  if (type == TYPE_DIALOG)
    return NOT_REMOVABLE;

  if (model_->items()[index].pinned_by_policy)
    return NOT_REMOVABLE;

  // Note: Only pinned app shortcuts can be removed!
  const std::string& app_id = model_->items()[index].id.app_id;
  return (type == TYPE_PINNED_APP && model_->IsAppPinned(app_id)) ? REMOVABLE
                                                                  : DRAGGABLE;
}

bool ShelfView::SameDragType(ShelfItemType typea, ShelfItemType typeb) const {
  if (IsPinnedShelfItemType(typea) && IsPinnedShelfItemType(typeb))
    return true;
  if (typea == TYPE_UNDEFINED || typeb == TYPE_UNDEFINED) {
    NOTREACHED() << "ShelfItemType must be set.";
    return false;
  }
  // Running app or dialog.
  return typea == typeb;
}

bool ShelfView::ShouldFocusOut(bool reverse, views::View* button) {
  // The logic here seems backwards, but is actually correct. For instance if
  // the ShelfView's internal focus cycling logic attemmpts to focus the first
  // child after hitting Tab, we intercept that and instead, advance through
  // to the status area.
  return (reverse && button == FindLastFocusableChild()) ||
         (!reverse && button == FindFirstFocusableChild());
}

std::pair<int, int> ShelfView::GetDragRange(int index) {
  DCHECK(base::Contains(visible_views_indices_, index));
  const ShelfItem& dragged_item = model_->items()[index];

  // If |drag_view_| is allowed to be dragged across the separator, return the
  // first and the last index of the |visible_views_indices_|.
  if (CanDragAcrossSeparator(drag_view_)) {
    return std::make_pair(visible_views_indices_[0],
                          visible_views_indices_.back());
  }

  int first = -1;
  int last = -1;
  for (int i : visible_views_indices_) {
    if (SameDragType(model_->items()[i].type, dragged_item.type)) {
      if (first == -1)
        first = i;
      last = i;
    } else if (first != -1) {
      break;
    }
  }
  DCHECK_NE(first, -1);
  DCHECK_NE(last, -1);

  // TODO(afakhry): Consider changing this when taking into account inactive
  // desks.
  return std::make_pair(first, last);
}

bool ShelfView::ShouldUpdateDraggedViewPinStatus(int dragged_view_index) {
  if (!features::IsDragUnpinnedAppToPinEnabled())
    return false;

  DCHECK(base::Contains(visible_views_indices_, dragged_view_index));
  bool is_moved_item_pinned =
      IsPinnedShelfItemType(model_->items()[dragged_view_index].type);
  if (separator_index_ == -1) {
    // If |separator_index_| equals to -1, all the apps in shelf are expected to
    // have the same pinned status.
    for (auto index : visible_views_indices_) {
      if (index != dragged_view_index) {
        // Return true if the pin status of the moved item is different from
        // others.
        return is_moved_item_pinned !=
               IsPinnedShelfItemType(model_->items()[index].type);
      }
    }
    return false;
  }
  // If the separator is shown, check whether the pin status of dragged item
  // matches the pin status implied by the dragged view position relative to the
  // separator.
  bool should_pinned_by_position = dragged_view_index <= separator_index_;
  return should_pinned_by_position != is_moved_item_pinned;
}

bool ShelfView::CanDragAcrossSeparator(views::View* drag_view) const {
  if (!features::IsDragUnpinnedAppToPinEnabled())
    return false;

  DCHECK(drag_view);
  // The dragged item is not allowed to be unpinned if |drag_view| is pinned by
  // policy, dragged from app list, or its item type is TYPE_BROWSER_SHORTCUT.
  // Therefore, the |drag_view| can not be dragged across the separator.
  bool can_change_pin_state =
      ShelfItemForView(drag_view)->type == TYPE_PINNED_APP ||
      ShelfItemForView(drag_view)->type == TYPE_APP;
  // Note that |drag_and_drop_shelf_id_| is set only when the current drag view
  // is from app list, which can not be dragged to the unpinned app side.
  return !ShelfItemForView(drag_view)->pinned_by_policy &&
         drag_and_drop_shelf_id_ == ShelfID() && can_change_pin_state;
}

void ShelfView::OnFadeInAnimationEnded() {
  // Call PreferredSizeChanged() to notify container to re-layout at the end
  // of fade-in animation.
  PreferredSizeChanged();
}

void ShelfView::OnFadeOutAnimationEnded() {
  // Call PreferredSizeChanged() to notify container to re-layout at the end
  // of removal animation.
  PreferredSizeChanged();

  AnimateToIdealBounds();
}

gfx::Rect ShelfView::GetMenuAnchorRect(const views::View& source,
                                       const gfx::Point& location,
                                       bool context_menu) const {
  // Application menus for items are anchored on the icon bounds.
  if (ShelfItemForView(&source) || !context_menu)
    return source.GetBoundsInScreen();

  const gfx::Rect shelf_bounds_in_screen = GetBoundsInScreen();
  gfx::Point origin;
  switch (shelf_->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      origin = gfx::Point(location.x(), shelf_bounds_in_screen.y());
      break;
    case ShelfAlignment::kLeft:
      origin = gfx::Point(shelf_bounds_in_screen.right(), location.y());
      break;
    case ShelfAlignment::kRight:
      origin = gfx::Point(shelf_bounds_in_screen.x(), location.y());
      break;
  }
  return gfx::Rect(origin, gfx::Size());
}

void ShelfView::AnnounceShelfAlignment() {
  base::string16 announcement;
  switch (shelf_->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      announcement = l10n_util::GetStringUTF16(IDS_SHELF_ALIGNMENT_BOTTOM);
      break;
    case ShelfAlignment::kLeft:
      announcement = l10n_util::GetStringUTF16(IDS_SHELF_ALIGNMENT_LEFT);
      break;
    case ShelfAlignment::kRight:
      announcement = l10n_util::GetStringUTF16(IDS_SHELF_ALIGNMENT_RIGHT);
      break;
  }
  announcement_view_->GetViewAccessibility().OverrideName(announcement);
  announcement_view_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                               /*send_native_event=*/true);
}

void ShelfView::AnnounceShelfAutohideBehavior() {
  base::string16 announcement;
  switch (shelf_->auto_hide_behavior()) {
    case ShelfAutoHideBehavior::kAlways:
      announcement = l10n_util::GetStringUTF16(IDS_SHELF_STATE_AUTO_HIDE);
      break;
    case ShelfAutoHideBehavior::kNever:
      announcement = l10n_util::GetStringUTF16(IDS_SHELF_STATE_ALWAYS_SHOWN);
      break;
    case ShelfAutoHideBehavior::kAlwaysHidden:
      announcement = l10n_util::GetStringUTF16(IDS_SHELF_STATE_ALWAYS_HIDDEN);
      break;
  }
  announcement_view_->GetViewAccessibility().OverrideName(announcement);
  announcement_view_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                               /*send_native_event=*/true);
}

void ShelfView::AnnouncePinUnpinEvent(const ShelfItem& item, bool pinned) {
  base::string16 item_title =
      item.title.empty()
          ? l10n_util::GetStringUTF16(IDS_SHELF_ITEM_GENERIC_NAME)
          : item.title;
  base::string16 announcement = l10n_util::GetStringFUTF16(
      pinned ? IDS_SHELF_ITEM_WAS_PINNED : IDS_SHELF_ITEM_WAS_UNPINNED,
      item_title);
  announcement_view_->GetViewAccessibility().OverrideName(announcement);
  announcement_view_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                               /*send_native_event=*/true);
}

void ShelfView::AnnounceSwapEvent(const ShelfItem& first_item,
                                  const ShelfItem& second_item) {
  base::string16 first_item_title =
      first_item.title.empty()
          ? l10n_util::GetStringUTF16(IDS_SHELF_ITEM_GENERIC_NAME)
          : first_item.title;
  base::string16 second_item_title =
      second_item.title.empty()
          ? l10n_util::GetStringUTF16(IDS_SHELF_ITEM_GENERIC_NAME)
          : second_item.title;
  base::string16 announcement = l10n_util::GetStringFUTF16(
      IDS_SHELF_ITEMS_WERE_SWAPPED, first_item_title, second_item_title);
  announcement_view_->GetViewAccessibility().OverrideName(announcement);
  announcement_view_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                               /*send_native_event=*/true);
}

gfx::Rect ShelfView::GetBoundsForDragInsertInScreen() {
  const ScrollableShelfView* scrollable_shelf_view =
      shelf_->hotseat_widget()->scrollable_shelf_view();
  gfx::Rect bounds = scrollable_shelf_view->visible_space();
  views::View::ConvertRectToScreen(scrollable_shelf_view, &bounds);
  return bounds;
}

int ShelfView::CancelDrag(int modified_index) {
  drag_scroll_dir_ = 0;
  scrolling_timer_.Stop();
  speed_up_drag_scrolling_.Stop();

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

void ShelfView::OnGestureEvent(ui::GestureEvent* event) {
  if (!ShouldHandleGestures(*event))
    return;

  if (HandleGestureEvent(event))
    event->StopPropagation();
}

void ShelfView::ShelfItemAdded(int model_index) {
  {
    base::AutoReset<bool> cancelling_drag(&cancelling_drag_model_changed_,
                                          true);
    model_index = CancelDrag(model_index);
  }
  const ShelfItem& item(model_->items()[model_index]);
  views::View* view = CreateViewForItem(item);
  // Hide the view, it'll be made visible when the animation is done. Using
  // opacity 0 here to avoid messing with CalculateIdealBounds which touches
  // the view's visibility.
  view->layer()->SetOpacity(0);
  view_model_->Add(view, model_index);

  // Add child view so it has the same ordering as in the |view_model_|.
  // Note: No need to call UpdateShelfItemViewsVisibility() here directly, since
  // it will be called by ScrollableShelfView::ViewHierarchyChanged() as a
  // result of the below call.
  AddChildViewAt(view, model_index);

  // Give the button its ideal bounds. That way if we end up animating the
  // button before this animation completes it doesn't appear at some random
  // spot (because it was in the middle of animating from 0,0 0x0 to its
  // target).
  CalculateIdealBounds();
  view->SetBoundsRect(view_model_->ideal_bounds(model_index));

  if (model_->is_current_mutation_user_triggered() &&
      drag_and_drop_shelf_id_ != item.id) {
    view->ScrollViewToVisible();
  }

  // The first animation moves all the views to their target position. |view|
  // is hidden, so it visually appears as though we are providing space for
  // it. When done we'll fade the view in.
  AnimateToIdealBounds();
  DCHECK_LE(model_index, visible_views_indices_.back());
  bounds_animator_->SetAnimationDelegate(
      view, std::unique_ptr<gfx::AnimationDelegate>(
                new StartFadeAnimationDelegate(this, view)));

  if (model_->is_current_mutation_user_triggered() &&
      item.type == TYPE_PINNED_APP) {
    AnnouncePinUnpinEvent(item, /*pinned=*/true);
  }
}

void ShelfView::ShelfItemRemoved(int model_index, const ShelfItem& old_item) {
  if (old_item.id == context_menu_id_ && shelf_menu_model_adapter_)
    shelf_menu_model_adapter_->Cancel();

  // If std::move is not called on |view|, |view| will be deleted once out of
  // scope.
  std::unique_ptr<views::View> view(view_model_->view_at(model_index));
  view_model_->Remove(model_index);

  {
    base::AutoReset<bool> cancelling_drag(&cancelling_drag_model_changed_,
                                          true);
    CancelDrag(-1);
  }

  if (view.get() == shelf_->tooltip()->GetCurrentAnchorView())
    shelf_->tooltip()->Close();

  if (view->GetVisible() && view->layer()->opacity() > 0.0f) {
    UpdateShelfItemViewsVisibility();

    // The first animation fades out the view. When done we'll animate the rest
    // of the views to their target location.
    bounds_animator_->SetAnimationMetricsReporter(
        fade_out_animation_reporter_.get());
    bounds_animator_->AnimateViewTo(view.get(), view->bounds());
    bounds_animator_->SetAnimationDelegate(
        view.get(), std::unique_ptr<gfx::AnimationDelegate>(
                        new FadeOutAnimationDelegate(this, std::move(view))));
  } else {
    // Ensures that |view| is not used after destruction.
    StopAnimatingViewIfAny(view.get());

    // Removes |view| to trigger ViewHierarchyChanged function in the parent
    // view if any.
    view.reset();

    // If there is no fade out animation, notify the parent view of the
    // changed size before bounds animations start.
    PreferredSizeChanged();

    // We don't need to show a fade out animation for invisible |view|. When an
    // item is ripped out from the shelf, its |view| is already invisible.
    AnimateToIdealBounds();
  }

  if (model_->is_current_mutation_user_triggered() &&
      old_item.type == TYPE_PINNED_APP) {
    AnnouncePinUnpinEvent(old_item, /*pinned=*/false);
  }
}

void ShelfView::ShelfItemChanged(int model_index, const ShelfItem& old_item) {
  // Bail if the view and shelf sizes do not match. ShelfItemChanged may be
  // called here before ShelfItemAdded, due to ChromeLauncherController's
  // item initialization, which calls SetItem during ShelfItemAdded.
  if (static_cast<int>(model_->items().size()) != view_model_->view_size())
    return;

  const ShelfItem& item = model_->items()[model_index];

  // If there's a change in the item's active desk, perform the update at the
  // end of this function in order to guarantee that both |model_| and
  // |view_model_| are consistent if there are other changes in the item.
  base::ScopedClosureRunner run_at_scope_exit;
  if (old_item.is_on_active_desk != item.is_on_active_desk) {
    run_at_scope_exit.ReplaceClosure(base::BindOnce(
        &ShelfView::ShelfItemsUpdatedForDeskChange, base::Unretained(this)));
  }

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
    // The view must be added to the |view_model_| before it's added as a child
    // so that the model is consistent when UpdateShelfItemViewsVisibility() is
    // called as a result the hierarchy changes caused by AddChildView(). See
    // ScrollableShelfView::ViewHierarchyChanged().
    view_model_->Add(new_view, model_index);
    AddChildView(new_view);
    view_model_->set_ideal_bounds(model_index, old_ideal_bounds);

    bounds_animator_->StopAnimatingView(new_view);
    new_view->SetBoundsRect(old_view->bounds());
    bounds_animator_->AnimateViewTo(new_view, old_ideal_bounds);

    // If an item is being pinned or unpinned, show the new status of the
    // shelf immediately so that the separator gets drawn as needed.
    if (old_item.type == TYPE_PINNED_APP || item.type == TYPE_PINNED_APP) {
      if (model_->is_current_mutation_user_triggered())
        AnnouncePinUnpinEvent(old_item, item.type == TYPE_PINNED_APP);
      AnimateToIdealBounds();
    }
    return;
  }

  views::View* view = view_model_->view_at(model_index);
  switch (item.type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP:
    case TYPE_DIALOG: {
      CHECK_EQ(ShelfAppButton::kViewClassName, view->GetClassName());
      ShelfAppButton* button = static_cast<ShelfAppButton*>(view);
      button->ReflectItemStatus(item);
      button->SetImage(item.image);
      button->SchedulePaint();
      break;
    }
    case TYPE_UNDEFINED:
      break;
  }
}

void ShelfView::ShelfItemsUpdatedForDeskChange() {
  DCHECK(features::IsPerDeskShelfEnabled());

  // The order here matters, since switching/removing desks, or moving windows
  // between desks will affect shelf items' visibility, we need to update the
  // visibility of the views first before we layout.
  UpdateShelfItemViewsVisibility();
  // Signal to the parent ScrollableShelfView so that it can recenter the items
  // after their visibility have been updated (via
  // `UpdateAvailableSpaceAndScroll()`).
  PreferredSizeChanged();
  LayoutToIdealBounds();
}

void ShelfView::ShelfItemMoved(int start_index, int target_index) {
  view_model_->Move(start_index, target_index);

  // Reorder the child view to be in the same order as in the |view_model_|.
  ReorderChildView(view_model_->view_at(target_index), target_index);
  NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                           true /* send_native_event */);

  // When cancelling a drag due to a shelf item being added, the currently
  // dragged item is moved back to its initial position. AnimateToIdealBounds
  // will be called again when the new item is added to the |view_model_| but
  // at this time the |view_model_| is inconsistent with the |model_|.
  if (!cancelling_drag_model_changed_)
    AnimateToIdealBounds();
}

void ShelfView::ShelfItemDelegateChanged(const ShelfID& id,
                                         ShelfItemDelegate* old_delegate,
                                         ShelfItemDelegate* delegate) {
  if (id == context_menu_id_ && shelf_menu_model_adapter_)
    shelf_menu_model_adapter_->Cancel();
}

void ShelfView::ShelfItemStatusChanged(const ShelfID& id) {
  scoped_display_for_new_windows_.reset();

  int index = model_->ItemIndexByID(id);
  if (index < 0)
    return;

  const ShelfItem item = model_->items()[index];
  ShelfAppButton* button = GetShelfAppButton(id);
  button->ReflectItemStatus(item);
  button->SchedulePaint();
}

void ShelfView::ShelfItemRippedOff() {
  // On the display where the drag started, there is nothing to do.
  if (dragging())
    return;
  // When a dragged item has been ripped off the shelf, it is moved to the end.
  // Now we need to hide it.
  view_model_->view_at(model_->item_count() - 1)->layer()->SetOpacity(0.f);
}

void ShelfView::ShelfItemReturnedFromRipOff(int index) {
  // On the display where the drag started, there is nothing to do.
  if (dragging())
    return;
  // Show the item and prevent it from animating into place from the position
  // where it was sitting with zero opacity.
  views::View* view = view_model_->view_at(index);
  const gfx::Rect bounds = bounds_animator_->GetTargetBounds(view);
  bounds_animator_->StopAnimatingView(view);
  view->SetBoundsRect(bounds);
  view->layer()->SetOpacity(1.f);
}

void ShelfView::OnShelfAlignmentChanged(aura::Window* root_window,
                                        ShelfAlignment old_alignment) {
  LayoutToIdealBounds();
  for (const auto& visible_index : visible_views_indices_)
    view_model_->view_at(visible_index)->Layout();

  AnnounceShelfAlignment();
}

void ShelfView::OnShelfAutoHideBehaviorChanged(aura::Window* root_window) {
  AnnounceShelfAutohideBehavior();
}

void ShelfView::AfterItemSelected(const ShelfItem& item,
                                  views::Button* sender,
                                  std::unique_ptr<ui::Event> event,
                                  views::InkDrop* ink_drop,
                                  ShelfAction action,
                                  ShelfItemDelegate::AppMenuItems menu_items) {
  item_awaiting_response_ = ShelfID();
  shelf_button_pressed_metric_tracker_.ButtonPressed(*event, sender, action);

  // Record AppList metric for any action considered an app launch.
  if (action == SHELF_ACTION_NEW_WINDOW_CREATED ||
      action == SHELF_ACTION_WINDOW_ACTIVATED) {
    Shell::Get()->app_list_controller()->RecordShelfAppLaunched();
  }

  // The app list handles its own ink drop effect state changes.
  if (action == SHELF_ACTION_APP_LIST_DISMISSED) {
    ink_drop->SnapToActivated();
    ink_drop->AnimateToState(views::InkDropState::HIDDEN);
  } else if (action != SHELF_ACTION_APP_LIST_SHOWN && !dragging()) {
    if (action != SHELF_ACTION_NEW_WINDOW_CREATED && menu_items.size() > 1 &&
        !dragging()) {
      // Show the app menu with 2 or more items, if no window was created. The
      // menu is not shown in case item drag started while the selection request
      // was in progress.
      ink_drop->AnimateToState(views::InkDropState::ACTIVATED);
      context_menu_id_ = item.id;
      ShowMenu(std::make_unique<ShelfApplicationMenuModel>(
                   item.title, std::move(menu_items),
                   model_->GetShelfItemDelegate(item.id)),
               sender, gfx::Point(), /*context_menu=*/false,
               ui::GetMenuSourceTypeForEvent(*event));
      shelf_->UpdateVisibilityState();
    } else {
      ink_drop->AnimateToState(views::InkDropState::ACTION_TRIGGERED);
    }
  }
  shelf_->shelf_layout_manager()->OnShelfItemSelected(action);
}

void ShelfView::ShowShelfContextMenu(
    const ShelfID& shelf_id,
    const gfx::Point& point,
    views::View* source,
    ui::MenuSourceType source_type,
    std::unique_ptr<ui::SimpleMenuModel> model) {
  context_menu_id_ = shelf_id;
  if (!model) {
    const int64_t display_id = GetDisplayIdForView(this);
    model = std::make_unique<ShelfContextMenuModel>(nullptr, display_id);
  }
  ShowMenu(std::move(model), source, point, /*context_menu=*/true, source_type);
}

void ShelfView::ShowMenu(std::unique_ptr<ui::SimpleMenuModel> menu_model,
                         views::View* source,
                         const gfx::Point& click_point,
                         bool context_menu,
                         ui::MenuSourceType source_type) {
  // Delayed callbacks to show context and application menus may conflict; hide
  // the old menu before showing a new menu in that case.
  if (IsShowingMenu())
    shelf_menu_model_adapter_->Cancel();

  item_awaiting_response_ = ShelfID();
  if (menu_model->GetItemCount() == 0)
    return;
  menu_owner_ = source;

  closing_event_time_ = base::TimeTicks();

  // NOTE: If you convert to HAS_MNEMONICS be sure to update menu building code.
  int run_types = views::MenuRunner::USE_TOUCHABLE_LAYOUT;
  if (context_menu) {
    run_types |=
        views::MenuRunner::CONTEXT_MENU | views::MenuRunner::FIXED_ANCHOR;
  }

  const ShelfItem* item = ShelfItemForView(source);
  // Only selected shelf items with context menu opened can be dragged.
  if (context_menu && item && ShelfButtonIsInDrag(item->type, source) &&
      source_type == ui::MenuSourceType::MENU_SOURCE_TOUCH) {
    run_types |= views::MenuRunner::SEND_GESTURE_EVENTS_TO_OWNER;
  }

  shelf_menu_model_adapter_ = std::make_unique<ShelfMenuModelAdapter>(
      item ? item->id.app_id : std::string(), std::move(menu_model), source,
      source_type,
      base::BindOnce(&ShelfView::OnMenuClosed, base::Unretained(this), source),
      IsTabletModeEnabled(),
      /*for_application_menu_items*/ !context_menu);
  shelf_menu_model_adapter_->Run(
      GetMenuAnchorRect(*source, click_point, context_menu),
      shelf_->IsHorizontalAlignment() ? views::MenuAnchorPosition::kBubbleAbove
                                      : views::MenuAnchorPosition::kBubbleLeft,
      run_types);

  if (!context_menu_shown_callback_.is_null())
    context_menu_shown_callback_.Run();
}

void ShelfView::OnMenuClosed(views::View* source) {
  menu_owner_ = nullptr;
  context_menu_id_ = ShelfID();

  closing_event_time_ = shelf_menu_model_adapter_->GetClosingEventTime();

  const ShelfItem* item = ShelfItemForView(source);
  if (item)
    static_cast<ShelfAppButton*>(source)->OnMenuClosed();

  shelf_menu_model_adapter_.reset();

  const bool is_in_drag = item && ShelfButtonIsInDrag(item->type, source);
  // Update the shelf visibility since auto-hide or alignment might have
  // changes, but don't update if shelf item is being dragged. Since shelf
  // should be kept as visible during shelf item drag even menu is closed.
  if (!is_in_drag)
    shelf_->UpdateVisibilityState();
}

void ShelfView::OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) {
  shelf_->NotifyShelfIconPositionsChanged();

  // Do not call PreferredSizeChanged() so that container does not re-layout
  // during the bounds animation.
}

void ShelfView::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  shelf_->set_is_tablet_mode_animation_running(false);

  if (snap_back_from_rip_off_view_ && animator == bounds_animator_.get()) {
    if (!animator->IsAnimating(snap_back_from_rip_off_view_)) {
      // Coming here the animation of the ShelfAppButton is finished and the
      // previously hidden status can be shown again. Since the button itself
      // might have gone away or changed locations we check that the button
      // is still in the shelf and show its status again.
      const auto& entries = view_model_->entries();
      const auto iter = std::find_if(
          entries.begin(), entries.end(), [this](const auto& entry) {
            return entry.view == snap_back_from_rip_off_view_;
          });
      if (iter != entries.end())
        snap_back_from_rip_off_view_->ClearState(ShelfAppButton::STATE_HIDDEN);

      snap_back_from_rip_off_view_ = nullptr;
    }
  }
}

bool ShelfView::IsRepostEvent(const ui::Event& event) {
  if (closing_event_time_.is_null())
    return false;

  // If the current (press down) event is a repost event, the time stamp of
  // these two events should be the same.
  return closing_event_time_ == event.time_stamp();
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

void ShelfView::SetDragImageBlur(const gfx::Size& size, int blur_radius) {
  DragImageView* drag_image = GetDragImage();
  drag_image->SetPaintToLayer();
  drag_image->layer()->SetFillsBoundsOpaquely(false);
  const uint32_t radius = std::round(size.width() / 2.f);
  drag_image->layer()->SetRoundedCornerRadius({radius, radius, radius, radius});
  drag_image->layer()->SetBackgroundBlur(blur_radius);
}

bool ShelfView::ShouldHandleGestures(const ui::GestureEvent& event) const {
  if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    float x_offset = event.details().scroll_x_hint();
    float y_offset = event.details().scroll_y_hint();
    if (!shelf_->IsHorizontalAlignment())
      std::swap(x_offset, y_offset);

    return std::abs(x_offset) < std::abs(y_offset);
  }

  return true;
}

base::string16 ShelfView::GetTitleForChildView(const views::View* view) const {
  const ShelfItem* item = ShelfItemForView(view);
  return item ? item->title : base::string16();
}

void ShelfView::UpdateShelfItemViewsVisibility() {
  visible_views_indices_.clear();
  for (int i = 0; i < view_model_->view_size(); ++i) {
    View* view = view_model_->view_at(i);
    // To receive drag event continuously from |drag_view_| during the dragging
    // off from the shelf, don't make |drag_view_| invisible. It will be
    // eventually invisible and removed from the |view_model_| by
    // FinalizeRipOffDrag().
    const bool has_to_show = dragged_off_shelf_ && view == drag_view();
    const bool is_visible = has_to_show || IsItemVisible(model()->items()[i]);
    view->SetVisible(is_visible);

    if (is_visible)
      visible_views_indices_.push_back(i);
  }
}

void ShelfView::DestroyScopedDisplay() {
  scoped_display_for_new_windows_.reset();
}

int ShelfView::CalculateAppIconsLayoutOffset() const {
  const ScrollableShelfView* scrollable_shelf_view =
      shelf_->hotseat_widget()->scrollable_shelf_view();
  const gfx::Insets& edge_padding_insets =
      scrollable_shelf_view->edge_padding_insets();

  return shelf_->IsHorizontalAlignment() ? edge_padding_insets.left()
                                         : edge_padding_insets.top();
}

DragImageView* ShelfView::GetDragImage() {
  return static_cast<DragImageView*>(drag_image_widget_->GetContentsView());
}

gfx::Rect ShelfView::GetChildViewTargetMirroredBounds(
    const views::View* child) const {
  DCHECK_EQ(this, child->parent());
  return GetMirroredRect(bounds_animator_->GetTargetBounds(child));
}

}  // namespace ash
