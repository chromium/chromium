// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_item_util.h"
#include "ash/app_list/views/app_drag_icon_proxy.h"
#include "ash/app_list/views/ghost_image_view.h"
#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/keyboard/keyboard_util.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/shelf_item.h"
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
#include "ash/style/ash_color_provider.h"
#include "ash/system/status_area_widget.h"
#include "ash/user_education/user_education_class_properties.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "chromeos/utils/haptics_util.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/user_education/common/events.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/menu_source_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/separator.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

using gfx::Animation;
using views::View;

namespace ash {

// The rip off distance, where the shelf icon gets unpinned if dragged over this
// distance from the outer edge of the shelf, depends on the shelf size. The
// distance is calculated by multiplying the shelf size by
// `kRipOffDistanceFactor`.
constexpr float kRipOffDistanceFactor = 0.75f;

// The rip off drag and drop proxy image should get scaled by this factor.
constexpr float kDragAndDropProxyScale = 1.2f;

// The opacity represents that this partially disappeared item will get removed.
constexpr float kDraggedImageOpacity = 0.5f;

namespace {

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

// A class to temporarily disable a given bounds animator.
class BoundsAnimatorDisabler {
 public:
  explicit BoundsAnimatorDisabler(views::BoundsAnimator* bounds_animator)
      : old_duration_(bounds_animator->GetAnimationDuration()),
        bounds_animator_(bounds_animator) {
    bounds_animator_->SetAnimationDuration(base::Milliseconds(1));
  }

  BoundsAnimatorDisabler(const BoundsAnimatorDisabler&) = delete;
  BoundsAnimatorDisabler& operator=(const BoundsAnimatorDisabler&) = delete;

  ~BoundsAnimatorDisabler() {
    bounds_animator_->SetAnimationDuration(old_duration_);
  }

 private:
  // The previous animation duration.
  base::TimeDelta old_duration_;
  // The bounds animator which gets used.
  raw_ptr<views::BoundsAnimator> bounds_animator_;
};

void ReportMoveAnimationSmoothness(int smoothness) {
  base::UmaHistogramPercentage(kShelfIconMoveAnimationHistogram, smoothness);
}

void ReportFadeInAnimationSmoothness(int smoothness) {
  base::UmaHistogramPercentage(kShelfIconFadeInAnimationHistogram, smoothness);
}

void ReportFadeOutAnimationSmoothness(int smoothness) {
  base::UmaHistogramPercentage(kShelfIconFadeOutAnimationHistogram, smoothness);
}

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
    case TYPE_UNPINNED_BROWSER_SHORTCUT:
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
  return cache && cache->GetAppType(app_id) == apps::AppType::kRemote;
}

bool IsStandaloneBrowser(const std::string& app_id) {
  AccountId account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  apps::AppRegistryCache* cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id);
  return cache &&
         cache->GetAppType(app_id) == apps::AppType::kStandaloneBrowser;
}

// Records the user metric action for whenever a shelf item is pinned or
// unpinned.
void RecordPinUnpinUserAction(bool pinned) {
  if (pinned) {
    base::RecordAction(base::UserMetricsAction("Shelf_ItemPinned"));
  } else {
    base::RecordAction(base::UserMetricsAction("Shelf_ItemUnpinned"));
  }
}

}  // namespace

// Helper class that resets a view opacity when called. Used to reset drag view
// opacity after animation to drop the drag icon proxy to its final bounds ends.
// The resetter is no-op if the target view gets destroyed before the resetter
// get run.
class ShelfView::ViewOpacityResetter : public views::ViewObserver {
 public:
  explicit ViewOpacityResetter(views::View* view) : view_(view) {
    view_observer_.Observe(view);
  }
  ViewOpacityResetter(const ViewOpacityResetter&) = delete;
  ViewOpacityResetter& operator=(const ViewOpacityResetter&) = delete;

  ~ViewOpacityResetter() override { Run(); }

  // views::ViewObserver:
  void OnViewIsDeleting(View* observed_view) override {
    view_ = nullptr;
    view_observer_.Reset();
  }

  void Run() {
    if (view_ && view_->layer())
      view_->layer()->SetOpacity(1.0f);
  }

 private:
  raw_ptr<views::View> view_;
  base::ScopedObservation<views::View, views::ViewObserver> view_observer_{
      this};
};

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

  raw_ptr<ShelfView> shelf_view_ = nullptr;
};

// AnimationDelegate used when deleting an item. This steadily decreased the
// opacity of the layer as the animation progress.
class ShelfView::FadeOutAnimationDelegate : public gfx::AnimationDelegate {
 public:
  FadeOutAnimationDelegate(ShelfView* host, std::unique_ptr<views::View> view)
      : shelf_view_(host), view_(std::move(view)) {}

  FadeOutAnimationDelegate(const FadeOutAnimationDelegate&) = delete;
  FadeOutAnimationDelegate& operator=(const FadeOutAnimationDelegate&) = delete;

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
  raw_ptr<ShelfView> shelf_view_;
  std::unique_ptr<views::View> view_;
};

// AnimationDelegate used to trigger fading an element in. When an item is
// inserted this delegate is attached to the animation that expands the size of
// the item.  When done it kicks off another animation to fade the item in.
class ShelfView::StartFadeAnimationDelegate : public gfx::AnimationDelegate {
 public:
  StartFadeAnimationDelegate(ShelfView* host, views::View* view)
      : shelf_view_(host), view_(view) {}

  StartFadeAnimationDelegate(const StartFadeAnimationDelegate&) = delete;
  StartFadeAnimationDelegate& operator=(const StartFadeAnimationDelegate&) =
      delete;

  ~StartFadeAnimationDelegate() override = default;

  // AnimationDelegate overrides:
  void AnimationEnded(const Animation* animation) override {
    shelf_view_->FadeIn(view_);
  }
  void AnimationCanceled(const Animation* animation) override {
    view_->layer()->SetOpacity(1.0f);
  }

 private:
  raw_ptr<ShelfView> shelf_view_;
  raw_ptr<views::View> view_;
};

// static
const int ShelfView::kMinimumDragDistance = 8;

ShelfView::ShelfView(ShelfModel* model,
                     Shelf* shelf,
                     Delegate* delegate,
                     ShelfButtonDelegate* shelf_button_delegate)
    : model_(model),
      shelf_(shelf),
      view_model_(std::make_unique<views::ViewModel>()),
      delegate_(delegate),
      bounds_animator_(
          std::make_unique<views::BoundsAnimator>(this,
                                                  /*use_transforms=*/true)),
      shelf_button_delegate_(shelf_button_delegate) {
  DCHECK(model_);
  DCHECK(shelf_);
  Shell::Get()->AddShellObserver(this);
  shelf_->AddObserver(this);
  bounds_animator_->AddObserver(this);
  bounds_animator_->SetAnimationDuration(
      ui::ScopedAnimationDurationScaleMode::duration_multiplier() *
      ShelfConfig::Get()->shelf_animation_duration());
  set_context_menu_controller(this);
  set_allow_deactivate_on_esc(true);

  if (features::IsUserEducationEnabled()) {
    // NOTE: Set `kHelpBubbleContextKey` before `views::kElementIdentifierKey`
    // in case registration causes a help bubble to be created synchronously.
    SetProperty(kHelpBubbleContextKey, HelpBubbleContext::kAsh);
  }
  SetProperty(views::kElementIdentifierKey, kShelfViewElementId);

  announcement_view_ = new views::View();
  AddChildView(announcement_view_.get());

  GetViewAccessibility().SetRole(ax::mojom::Role::kToolbar);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF8(IDS_ASH_SHELF_ACCESSIBLE_NAME));
}

ShelfView::~ShelfView() {
  shelf_->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  bounds_animator_->RemoveObserver(this);
  model_->RemoveObserver(this);
}

int ShelfView::GetSizeOfAppButtons(int count, int button_size) {
  const int button_spacing = ShelfConfig::Get()->button_spacing();
  return button_size * count + button_spacing * std::max(0, count - 1);
}

void ShelfView::Init(views::FocusSearch* focus_search) {
  auto separator = std::make_unique<views::Separator>();
  separator->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  separator->SetPreferredLength(kSeparatorSize);
  separator->SetVisible(false);
  ConfigureChildView(separator.get(), ui::LAYER_TEXTURED);
  separator_ = AddChildView(std::move(separator));

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

  focus_search_ = focus_search;

  // We'll layout when our bounds change.
}

bool ShelfView::IsShowingMenu() const {
  return shelf_menu_model_adapter_ &&
         shelf_menu_model_adapter_->IsShowingMenu();
}

void ShelfView::UpdateVisibleShelfItemBoundsUnion() {
  visible_shelf_item_bounds_union_.SetRect(0, 0, 0, 0);
  for (const auto i : visible_views_indices_) {
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
  DCHECK(views::IsViewClass<ShelfAppButton>(view));
  return static_cast<ShelfAppButton*>(view);
}

void ShelfView::StopAnimatingViewIfAny(views::View* view) {
  if (bounds_animator_->IsAnimating(view))
    bounds_animator_->StopAnimatingView(view);
}

int ShelfView::GetButtonSize() const {
  return ShelfConfig::Get()->GetShelfButtonSize(
      shelf_->hotseat_widget()->target_hotseat_density());
}

int ShelfView::GetButtonIconSize() const {
  return ShelfConfig::Get()->GetShelfButtonIconSize(
      shelf_->hotseat_widget()->target_hotseat_density());
}

int ShelfView::GetShortcutIconSize() const {
  return ShelfConfig::Get()->GetShelfShortcutIconSize();
}

int ShelfView::GetShelfShortcutIconContainerSize() const {
  return GetShortcutIconSize() +
         ShelfConfig::Get()->GetShelfShortcutIconBorderSize() * 2;
}

int ShelfView::GetShelfShortcutHostBadgeIconSize() const {
  return ShelfConfig::Get()->GetShelfShortcutHostBadgeIconSize();
}

int ShelfView::GetShelfShortcutHostBadgeContainerSize() const {
  return GetShelfShortcutHostBadgeIconSize() +
         ShelfConfig::Get()->GetShelfShortcutHostBadgeBorderSize() * 2;
}

int ShelfView::GetShelfShortcutTeardropCornerRadiusSize() const {
  return ShelfConfig::Get()->GetShelfShortcutTeardropCornerRadiusSize();
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
  announcement_view_->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringFUTF16(IDS_SHELF_ITEM_HAS_NOTIFICATION_BADGE,
                                 GetTitleForView(button)));
}

bool ShelfView::LocationInsideVisibleShelfItemBounds(
    const gfx::Point& location) const {
  return visible_shelf_item_bounds_union_.Contains(location);
}

bool ShelfView::ShouldHideTooltip(const gfx::Point& cursor_location,
                                  views::View* delegate_view) const {
  // There are thin gaps between launcher buttons but the tooltip shouldn't hide
  // in the gaps, but the tooltip should hide if the mouse moved totally outside
  // of the buttons area.
  return !LocationInsideVisibleShelfItemBounds(cursor_location);
}

const std::vector<aura::Window*> ShelfView::GetOpenWindowsForView(
    views::View* view) {
  std::vector<raw_ptr<aura::Window, VectorExperimental>> window_list =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);
  std::vector<aura::Window*> open_windows;
  const ShelfItem* item = ShelfItemForView(view);

  // The concept of a list of open windows doesn't make sense for something
  // that isn't an app shortcut: return an empty list.
  if (!item)
    return open_windows;

  for (aura::Window* window : window_list) {
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

std::u16string ShelfView::GetTitleForView(const views::View* view) const {
  if (view->parent() == this)
    return GetTitleForChildView(view);

  return std::u16string();
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

gfx::Size ShelfView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
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

gfx::Rect ShelfView::GetAnchorBoundsInScreen() const {
  // Anchor bounds is the smallest rect containing all visible items.
  gfx::Rect anchor_bounds_in_screen;
  for (const size_t i : visible_views_indices_) {
    const views::View* const child = view_model_->view_at(i);
    anchor_bounds_in_screen.Union(child->GetBoundsInScreen());
  }
  // When the shelf is in overflow, visible items may exist outside `parent()`
  // bounds but they are clipped. Since they are not visible to the user, do
  // not consider them as part of anchor bounds.
  if (parent()) {
    anchor_bounds_in_screen.Intersect(parent()->GetBoundsInScreen());
  }
  // Fall back to default anchor bounds if there are no visible items.
  return anchor_bounds_in_screen.IsEmpty()
             ? views::AccessiblePaneView::GetAnchorBoundsInScreen()
             : anchor_bounds_in_screen;
}

void ShelfView::OnThemeChanged() {
  views::AccessiblePaneView::OnThemeChanged();
  if (separator_)
    separator_->SchedulePaint();
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
    case ui::EventType::kMousewheel:
      // The mousewheel event is handled by the ScrollableShelfView.
      break;
    case ui::EventType::kMousePressed:
      if (!event->IsOnlyLeftMouseButton()) {
        if (event->IsOnlyRightMouseButton()) {
          ShowContextMenuForViewImpl(this, location_in_screen,
                                     ui::MENU_SOURCE_MOUSE);
          event->SetHandled();
        }
        return;
      }

      [[fallthrough]];
    case ui::EventType::kMouseDragged:
    case ui::EventType::kMouseReleased:
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

View* ShelfView::GetTooltipHandlerForPoint(const gfx::Point& point) {
  // Similar implementation as views::View, but without going into each
  // child's subviews.
  View::Views children = GetChildrenInZOrder();
  for (views::View* child : base::Reversed(children)) {
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

void ShelfView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (!details.is_add) {
    if (details.child == current_ghost_view_.get()) {
      current_ghost_view_ = nullptr;
      current_ghost_view_index_ = std::nullopt;
    }
    if (details.child == last_ghost_view_.get()) {
      last_ghost_view_ = nullptr;
      current_ghost_view_index_ = std::nullopt;
    }
  }
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
  DCHECK(last_pressed_index_.has_value());

  // Place new windows on the same display as the button. Opening windows is
  // usually an async operation so we wait until window activation changes
  // (ShelfItemStatusChanged) before destroying the scoped object. Post a task
  // to destroy the scoped object just in case the window activation event does
  // not get fired.
  aura::Window* window = sender->GetWidget()->GetNativeWindow();
  scoped_display_for_new_windows_ =
      std::make_unique<display::ScopedDisplayForNewWindows>(
          window->GetRootWindow());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ShelfView::DestroyScopedDisplay,
                     weak_factory_.GetWeakPtr()),
      base::Milliseconds(100));

  // Slow down activation animations if Control key is pressed.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> slowing_animations;
  if (event.IsControlDown()) {
    slowing_animations = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
        ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  }

  // Collect usage statistics before we decide what to do with the click.
  switch (model_->items()[last_pressed_index_.value()].type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP:
    case TYPE_UNPINNED_BROWSER_SHORTCUT:
      base::RecordAction(base::UserMetricsAction("Launcher_ClickOnApp"));
      break;

    case TYPE_DIALOG:
      break;

    case TYPE_UNDEFINED:
      NOTREACHED() << "ShelfItemType must be set.";
  }

  // Run AfterItemSelected directly if the item has no delegate (ie. in tests).
  const ShelfItem& item = model_->items()[last_pressed_index_.value()];
  if (!model_->GetShelfItemDelegate(item.id)) {
    AfterItemSelected(item, sender, event.Clone(), ink_drop, SHELF_ACTION_NONE,
                      {});
    return;
  }

  // Notify the item of its selection; handle the result in AfterItemSelected.
  item_awaiting_response_ = item.id;
  model_->GetShelfItemDelegate(item.id)->ItemSelected(
      event.Clone(), GetDisplayIdForView(this), LAUNCH_FROM_SHELF,
      base::BindOnce(&ShelfView::AfterItemSelected, weak_factory_.GetWeakPtr(),
                     item, sender, event.Clone(), ink_drop),
      base::BindRepeating(&ShouldIncludeMenuItem));
}

bool ShelfView::IsShowingMenuForView(const views::View* view) const {
  return IsShowingMenu() &&
         shelf_menu_model_adapter_->IsShowingMenuForView(*view);
}

////////////////////////////////////////////////////////////////////////////////
// ShelfView, FocusTraversable implementation:

views::FocusSearch* ShelfView::GetFocusSearch() {
  return focus_search_;
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
      views::InkDrop::Get(source)->AnimateToState(
          views::InkDropState::DEACTIVATED, nullptr);
    }
    return;
  }
  last_pressed_index_ = std::nullopt;
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

void ShelfView::OnShelfConfigUpdated() {
  // Ensure the shelf app buttons have an icon which is up to date with the
  // current ShelfConfig sizing.
  for (size_t i = 0; i < view_model_->view_size(); i++) {
    ShelfAppButton* button =
        static_cast<ShelfAppButton*>(view_model_->view_at(i));
    if (!button->IsIconSizeCurrent())
      ShelfItemChanged(i, model_->items()[i]);
  }
}

bool ShelfView::ShouldEventActivateButton(View* view, const ui::Event& event) {
  // This only applies to app buttons.
  DCHECK(views::IsViewClass<ShelfAppButton>(view));
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
  auto index = view_model_->GetIndexOfView(view);
  if (!index.has_value())
    return false;
  return !repost || last_pressed_index_ != index;
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
  if (view->layer()) {
    return;
  }
  view->SetPaintToLayer(layer_type);
  view->layer()->SetFillsBoundsOpaquely(false);
}

void ShelfView::CalculateIdealBounds() {
  DCHECK(static_cast<size_t>(model()->item_count()) ==
         view_model_->view_size());

  const int button_spacing = ShelfConfig::Get()->button_spacing();
  UpdateSeparatorIndex();

  // Don't show the separator if it isn't needed, or would appear after all
  // visible items.
  separator_->SetVisible(separator_index_.has_value() &&
                         separator_index_ < visible_views_indices_.back());
  // Set |separator_index_| to nullopt if it is not visible.
  if (!separator_->GetVisible())
    separator_index_ = std::nullopt;

  app_icons_layout_offset_ = CalculateAppIconsLayoutOffset();
  int x = shelf()->PrimaryAxisValue(app_icons_layout_offset_, 0);
  int y = shelf()->PrimaryAxisValue(0, app_icons_layout_offset_);

  // The padding is handled in ScrollableShelfView.

  const int button_size = GetButtonSize();
  for (size_t i = 0; i < view_model_->view_size(); ++i) {
    if (view_model_->view_at(i)->GetVisible()) {
      gfx::Rect ideal_view_bounds(x, y, button_size, button_size);
      view_model_->set_ideal_bounds(i, ideal_view_bounds);
      if (view_model_->view_at(i) == drag_view_ &&
          current_ghost_view_index_ != i && !dragged_off_shelf_) {
        if (current_ghost_view_)
          current_ghost_view_->FadeOut();

        last_ghost_view_ = current_ghost_view_;

        auto current_ghost_view = std::make_unique<GhostImageView>(GridIndex());
        gfx::Size icon_size = drag_view_
                                  ->GetIdealIconBounds(ideal_view_bounds.size(),
                                                       /*icon_scale=*/1.0f)
                                  .size();
        gfx::Rect ghost_view_bounds = ideal_view_bounds;

        // Ensure that the ghost_view_bounds are a square that encloses the
        // icon_size with the same center. The ghost view should draw as a
        // circle.
        const int icon_width = std::min(icon_size.width(), icon_size.height());
        ghost_view_bounds.ClampToCenteredSize(
            gfx::Size(icon_width, icon_width));

        current_ghost_view->Init(ghost_view_bounds,
                                 ghost_view_bounds.width() / 2);

        current_ghost_view_ = AddChildView(std::move(current_ghost_view));
        current_ghost_view_->FadeIn();
        current_ghost_view_index_ = i;
      }
      x = shelf()->PrimaryAxisValue(x + button_size + button_spacing, x);
      y = shelf()->PrimaryAxisValue(y, y + button_size + button_spacing);
    } else {
      view_model_->set_ideal_bounds(i, gfx::Rect(x, y, 0, 0));
    }
  }
}

views::View* ShelfView::CreateViewForItem(const ShelfItem& item) {
  views::View* view = nullptr;
  switch (item.type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP:
    case TYPE_UNPINNED_BROWSER_SHORTCUT:
    case TYPE_DIALOG: {
      ShelfAppButton* button = new ShelfAppButton(
          this, shelf_button_delegate_ ? shelf_button_delegate_.get() : this);
      UpdateButton(button, item);
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

void ShelfView::UpdateButton(ShelfAppButton* button, const ShelfItem& item) {
  button->ReflectItemStatus(item);
  button->SetMainAndMaybeHostBadgeImage(item.image, item.has_placeholder_icon,
                                        item.badge_image);
  button->SetNotificationBadgeColor(item.notification_badge_color);
  button->GetViewAccessibility().SetName(item.accessible_name);
  // If an empty accessible name of the item is provided, in such a case we want
  // the cache to have the name corresponding to the implementation in
  // `UpdateAccessibleName`
  button->UpdateAccessibleName();
  button->SchedulePaint();
}

int ShelfView::GetAvailableSpaceForAppIcons() const {
  return shelf()->PrimaryAxisValue(width(), height());
}

void ShelfView::UpdateSeparatorIndex() {
  // A separator is shown after the last pinned item only if it's followed by a
  // visible app item.
  std::optional<size_t> first_unpinned_index = std::nullopt;
  std::optional<size_t> last_pinned_index = std::nullopt;

  std::optional<size_t> dragged_item_index = std::nullopt;
  if (drag_view_)
    dragged_item_index = view_model_->GetIndexOfView(drag_view_);

  const bool can_drag_view_across_separator =
      drag_view_ && CanDragAcrossSeparator(drag_view_);

  for (size_t i : base::Reversed(visible_views_indices_)) {
    const auto& item = model()->items()[i];
    if (IsItemPinned(item)) {
      // The dragged item is temporarily moved to the end of the shelf if it is
      // ripped off by dragging. Ignore this case and continue to find the
      // correct separator index.
      if (dragged_off_shelf_ &&
          i == static_cast<size_t>(model()->item_count() - 1)) {
        continue;
      }

      last_pinned_index = i;
      break;
    }

    if (!IsPinnedShelfItemType(item.type) && item.is_on_active_desk) {
      first_unpinned_index = i;
    }
  }

  // If there is no unpinned item in shelf, return -1 as the separator should be
  // hidden.
  if (!first_unpinned_index.has_value()) {
    separator_index_ = std::nullopt;
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

bool ShelfView::ShouldHandleDrag(const std::string& app_id,
                                 const gfx::Point& location_in_screen) const {
  // Remote Apps are not pinnable.
  if (IsRemoteApp(app_id))
    return false;

  // Do not handle drag if an operation for another app is already in progress,
  // or the cursor is not inside the shelf bounds. This could happen if mouse /
  // touch operations overlap.
  return !app_id.empty() &&
         (drag_and_drop_shelf_id_.IsNull() ||
          drag_and_drop_shelf_id_.app_id == app_id) &&
         GetBoundsInScreen().Contains(location_in_screen);
}

bool ShelfView::StartDrag(const std::string& app_id,
                          const gfx::Point& location_in_screen,
                          const gfx::Rect& drag_icon_bounds_in_screen) {
  // Don't start a drag if another one is in progress.
  if (!drag_and_drop_shelf_id_.IsNull())
    return false;

  if (!ShouldHandleDrag(app_id, location_in_screen))
    return false;

  DCHECK(!is_active_drag_and_drop_host_);
  is_active_drag_and_drop_host_ = true;

  // If the AppsGridView (which was dispatching this event) was opened by our
  // button, ShelfView dragging operations are locked and we have to unlock.
  CancelDrag(std::nullopt);
  drag_and_drop_item_pinned_ = false;
  drag_and_drop_shelf_id_ = ShelfID(app_id);
  // Check if the application is pinned - if not, we have to pin it so
  // that we can re-arrange the shelf order accordingly. Note that items have
  // to be pinned to give them the same (order) possibilities as a shortcut.
  if (!model_->IsAppPinned(app_id)) {
    ShelfModel::ScopedUserTriggeredMutation user_triggered(model_);

    if (model_->ItemIndexByAppID(app_id) >= 0) {
      model_->PinExistingItemWithID(app_id);
    } else {
      model_->AddAndPinAppWithFactoryConstructedDelegate(app_id);
      drag_and_drop_item_pinned_ = true;
    }
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
  wm::ConvertPointFromScreen(window_util::GetRootWindowAt(location_in_screen),
                             &point_in_root);
  ui::MouseEvent event(ui::EventType::kMousePressed, pt, point_in_root,
                       ui::EventTimeForNow(), 0, 0);
  PointerPressedOnButton(drag_and_drop_view, DRAG_AND_DROP, event);

  // Drag the item where it really belongs.
  Drag(location_in_screen, drag_icon_bounds_in_screen);
  return true;
}

bool ShelfView::Drag(const gfx::Point& location_in_screen,
                     const gfx::Rect& drag_icon_bounds_in_screen) {
  if (drag_and_drop_shelf_id_.IsNull() ||
      !GetBoundsInScreen().Contains(location_in_screen))
    return false;

  drag_icon_bounds_in_screen_ = drag_icon_bounds_in_screen;
  gfx::Point pt = location_in_screen;
  views::View* drag_and_drop_view =
      view_model_->view_at(model_->ItemIndexByID(drag_and_drop_shelf_id_));
  ConvertPointFromScreen(drag_and_drop_view, &pt);
  gfx::Point point_in_root = location_in_screen;
  wm::ConvertPointFromScreen(window_util::GetRootWindowAt(location_in_screen),
                             &point_in_root);
  ui::MouseEvent event(ui::EventType::kMouseDragged, pt, point_in_root,
                       ui::EventTimeForNow(), 0, 0);
  PointerDraggedOnButton(drag_and_drop_view, DRAG_AND_DROP, event);
  return true;
}

void ShelfView::EndDrag(bool cancel) {
  drag_scroll_dir_ = 0;
  scrolling_timer_.Stop();
  speed_up_drag_scrolling_.Stop();

  if (drag_and_drop_shelf_id_.IsNull()) {
    is_active_drag_and_drop_host_ = false;
    return;
  }

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
      // TODO(crbug.com/40266934): Remove the check below once the bounds
      // animator works better with zero animation duration.
      if (!bounds_animator_->GetAnimationDuration().is_zero()) {
        bounds_animator_->SetAnimationDelegate(drag_and_drop_view,
                                               std::move(animation_delegate));
      }

    } else {
      drag_and_drop_view->SetSize(pre_drag_and_drop_size_);
    }
  }
  drag_icon_bounds_in_screen_ = gfx::Rect();
  drag_and_drop_shelf_id_ = ShelfID();
  is_active_drag_and_drop_host_ = false;
}

void ShelfView::SwapButtons(views::View* button_to_swap, bool with_next) {
  if (!button_to_swap)
    return;

  // Find the index of the button to swap in the view model.
  size_t src_index = static_cast<size_t>(-1);
  for (size_t i = 0; i < view_model_->view_size(); ++i) {
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

  auto index = view_model_->GetIndexOfView(view);
  if (!index.has_value() || view_model_->view_size() < 1)
    return;  // View is being deleted, ignore request.

  // Reset drag icon proxy from previous drag (which could be set if the drop
  // animation is still in progress), as drag icon proxy is not expected to be
  // reused after it starts animating out.
  drag_icon_proxy_.reset();
  drag_image_layer_.reset();

  // Only when the repost event occurs on the same shelf item, we should ignore
  // the call in ShelfView::ButtonPressed(...).
  is_repost_event_on_same_item_ =
      IsRepostEvent(event) && (last_pressed_index_ == index);

  CHECK(views::IsViewClass<ShelfAppButton>(view));
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

void ShelfView::PointerDraggedOnButton(const views::View* view,
                                       Pointer pointer,
                                       const ui::LocatedEvent& event) {
  if (CanPrepareForDrag(pointer, event))
    PrepareForDrag(pointer, event);

  if (drag_pointer_ == pointer)
    ContinueDrag(event);
}

void ShelfView::PointerReleasedOnButton(const views::View* view,
                                        Pointer pointer,
                                        bool canceled) {
  drag_scroll_dir_ = 0;
  scrolling_timer_.Stop();
  speed_up_drag_scrolling_.Stop();

  is_repost_event_on_same_item_ = false;

  if (canceled) {
    CancelDrag(std::nullopt);
  } else if (drag_pointer_ == pointer) {
    FinalizeRipOffDrag(false);
    drag_pointer_ = NONE;

    // Check if the pin status of |drag_view_| should be changed when
    // |drag_view_| is dragged over the separator. Do nothing if |drag_view_| is
    // already handled in FinalizedRipOffDrag.
    if (drag_view_) {
      if (ShouldUpdateDraggedViewPinStatus(
              view_model_->GetIndexOfView(view).value())) {
        const std::string drag_app_id = ShelfItemForView(drag_view_)->id.app_id;
        ShelfModel::ScopedUserTriggeredMutation user_triggered(model_);
        if (model_->IsAppPinned(drag_app_id)) {
          model_->UnpinAppWithID(drag_app_id);
        } else {
          model_->PinExistingItemWithID(drag_app_id);
        }
      }
    }
    AnimateToIdealBounds();
  }

  if (drag_pointer_ != NONE)
    return;

  delegate_->CancelScrollForItemDrag();

  if (!drag_view_ || dragged_off_shelf_) {
    drag_icon_proxy_.reset();
    drag_image_layer_.reset();
  }

  const gfx::Rect target_bounds_in_screen =
      CalculateDropTargetBoundsForDragViewInScreen();

  if (drag_icon_proxy_) {
    AnimateDragIconProxy(target_bounds_in_screen);
  } else if (drag_image_layer_) {
    AnimateDragImageLayer(target_bounds_in_screen);
  } else if (drag_view_) {
    drag_view_->layer()->SetOpacity(1.0f);
  }

  // If the drag pointer is NONE, no drag operation is going on and the
  // |drag_view_| can be released.
  drag_view_ = nullptr;
  drag_view_relative_to_ideal_bounds_ = RelativePosition::kNotAvailable;
  RemoveGhostView();
}

void ShelfView::AnimateDragImageLayer(
    const gfx::Rect& target_bounds_in_screen) {
  DCHECK(drag_image_layer_);

  if (!delegate_->AreBoundsWithinVisibleSpace(target_bounds_in_screen)) {
    drag_image_layer_.reset();
    drag_view_->layer()->SetOpacity(1.0f);
    return;
  }

  ui::Layer* target_layer = drag_image_layer_->root();
  if (target_layer) {
    target_layer->GetAnimator()->AbortAllAnimations();

    gfx::Rect current_bounds = target_layer->bounds();
    if (current_bounds.IsEmpty()) {
      OnDragIconProxyAnimatedOut(
          std::make_unique<ViewOpacityResetter>(drag_view_));
      return;
    }

    // |target_layer| bounds are in display coordinates.
    display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(
            GetWidget()->GetNativeWindow());
    current_bounds.Offset(display.bounds().OffsetFromOrigin());

    const gfx::Transform transform = gfx::TransformBetweenRects(
        gfx::RectF(current_bounds), gfx::RectF(target_bounds_in_screen));

    views::AnimationBuilder builder;
    builder.SetPreemptionStrategy(ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET)
        .OnEnded(base::BindOnce(
            &ShelfView::OnDragIconProxyAnimatedOut, weak_factory_.GetWeakPtr(),
            std::make_unique<ViewOpacityResetter>(drag_view_)))
        .OnAborted(base::BindOnce(
            &ShelfView::OnDragIconProxyAnimatedOut, weak_factory_.GetWeakPtr(),
            std::make_unique<ViewOpacityResetter>(drag_view_)))
        .Once()
        .SetDuration(base::Milliseconds(200))
        .SetTransform(target_layer, transform, gfx::Tween::FAST_OUT_LINEAR_IN);
  }
}

void ShelfView::AnimateDragIconProxy(const gfx::Rect& target_bounds_in_screen) {
  DCHECK(drag_icon_proxy_);

  if (!delegate_->AreBoundsWithinVisibleSpace(target_bounds_in_screen)) {
    drag_icon_proxy_.reset();
    drag_view_->layer()->SetOpacity(1.0f);
    return;
  }

  drag_icon_proxy_->AnimateToBoundsAndCloseWidget(
      target_bounds_in_screen,
      base::BindOnce(&ShelfView::OnDragIconProxyAnimatedOut,
                     base::Unretained(this),
                     std::make_unique<ViewOpacityResetter>(drag_view_)));
}

gfx::Rect ShelfView::CalculateDropTargetBoundsForDragViewInScreen() {
  if (!drag_view_) {
    return gfx::Rect();
  }

  const gfx::Rect drag_view_ideal_bounds = view_model_->ideal_bounds(
      view_model_->GetIndexOfView(drag_view_).value());
  gfx::Rect target_bounds_in_screen =
      drag_view_->GetIdealIconBounds(drag_view_ideal_bounds.size(),
                                     /*icon_scale=*/1.0f);
  target_bounds_in_screen.Offset(drag_view_ideal_bounds.x(),
                                 drag_view_ideal_bounds.y());
  target_bounds_in_screen = GetMirroredRect(target_bounds_in_screen);
  views::View::ConvertRectToScreen(this, &target_bounds_in_screen);

  return target_bounds_in_screen;
}

void ShelfView::OnDragIconProxyAnimatedOut(
    std::unique_ptr<ViewOpacityResetter> opacity_resetter) {
  opacity_resetter->Run();
  drag_icon_proxy_.reset();
  drag_image_layer_.reset();
}

void ShelfView::LayoutToIdealBounds() {
  if (bounds_animator_->IsAnimating()) {
    AnimateToIdealBounds();
    return;
  }

  CalculateIdealBounds();
  views::ViewModelUtils::SetViewBoundsToIdealBounds(*view_model_);
  UpdateSeparatorBounds(/*animate=*/false);
  UpdateVisibleShelfItemBoundsUnion();

  // Notify user education features that anchor bounds have changed.
  if (features::IsUserEducationEnabled()) {
    views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
        user_education::kHelpBubbleAnchorBoundsChangedEvent, this);
  }
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

  move_animation_tracker_.emplace(
      GetWidget()->GetCompositor()->RequestNewThroughputTracker());
  move_animation_tracker_->Start(metrics_util::ForSmoothnessV3(
      base::BindRepeating(&ReportMoveAnimationSmoothness)));

  for (size_t i = 0; i < view_model_->view_size(); ++i) {
    View* view = view_model_->view_at(i);
    bounds_animator_->AnimateViewTo(view, view_model_->ideal_bounds(i));
    // Now that the item animation starts, we have to make sure that the
    // padding of the first gets properly transferred to the new first item.
    view->SetBorder(nullptr);
  }

  UpdateSeparatorBounds(/*animate=*/true);
  UpdateVisibleShelfItemBoundsUnion();
}

void ShelfView::UpdateSeparatorBounds(bool animate) {
  // The `- 1` is because we expect at least one item after the separator.
  if (!separator_index_.has_value() ||
      separator_index_ >= view_model_->view_size() - 1) {
    return;
  }

  // The ` + 1` is because we compute the separator position as an offset
  // leftward from the item just after the separator.
  gfx::Rect icon_bounds_beside_separator =
      view_model_->ideal_bounds(separator_index_.value() + 1);

  // Calculate the position value on the secondary axis.
  int secondary_offset =
      (shelf_->hotseat_widget()->GetHotseatSize() - kSeparatorSize) / 2;

  // Because we will be subtracting half the button spacing, round it up to
  // favor leftward or upward.
  const int half_button_spacing_rounded_up =
      (ShelfConfig::Get()->button_spacing() + 1) / 2;
  const int separator_x = shelf()->PrimaryAxisValue(
      icon_bounds_beside_separator.x() - half_button_spacing_rounded_up,
      secondary_offset);
  const int separator_y = shelf()->PrimaryAxisValue(
      secondary_offset,
      icon_bounds_beside_separator.y() - half_button_spacing_rounded_up);
  gfx::Rect separator_bounds(
      separator_x, separator_y,
      shelf()->PrimaryAxisValue(kSeparatorThickness, kSeparatorSize),
      shelf()->PrimaryAxisValue(kSeparatorSize, kSeparatorThickness));

  if (animate)
    bounds_animator_->AnimateViewTo(separator_, separator_bounds);
  else
    separator_->SetBoundsRect(separator_bounds);
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

  ui::AnimationThroughputReporter reporter(
      fade_in_animation_settings.GetAnimator(),
      metrics_util::ForSmoothnessV3(
          base::BindRepeating(&ReportFadeInAnimationSmoothness)));

  view->layer()->SetOpacity(1.f);
}

void ShelfView::PrepareForDrag(Pointer pointer, const ui::LocatedEvent& event) {
  DCHECK(!dragging());
  DCHECK(drag_view_);
  drag_pointer_ = pointer;
  start_drag_index_ = view_model_->GetIndexOfView(drag_view_);
  drag_scroll_dir_ = 0;

  if (!start_drag_index_.has_value()) {
    CancelDrag(std::nullopt);
    return;
  }

  // Cancel in-flight request for app item context menu model (made when app
  // context menu is requested), to prevent the pending callback from showing
  // a context menu just after drag starts.
  if (!context_menu_callback_.IsCancelled()) {
    GetShelfAppButton(item_awaiting_response_)
        ->OnContextMenuModelRequestCanceled();
    ResetActiveMenuModelRequest();
  }

  // Move the view to the front so that it appears on top of other views.
  ReorderChildView(drag_view_, children().size());
  bounds_animator_->StopAnimatingView(drag_view_);

  drag_view_->OnDragStarted(&event);

  // Drag icon proxy from previous drag may be around if the icon is still
  // animating to the final position. Reset it here to cancel the animation.
  drag_icon_proxy_.reset();
  drag_image_layer_.reset();
  delegate_->CancelScrollForItemDrag();

  drag_view_->layer()->SetOpacity(0.0f);
  if (!is_active_drag_and_drop_host_) {
    aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
    gfx::Point screen_location = event.root_location();
    ::wm::ConvertPointToScreen(root_window, &screen_location);

    const gfx::ImageSkia icon_image =
        drag_view_->GetIconImage(kDragAndDropProxyScale);
    drag_icon_proxy_ = std::make_unique<AppDragIconProxy>(
        root_window, icon_image,
        drag_view_->GetBadgeIconImage(kDragAndDropProxyScale), screen_location,
        gfx::Vector2d(), /*scale_factor=*/1.0f,
        /*is_folder_icon=*/false, icon_image.size());

    if (pointer == MOUSE) {
      chromeos::haptics_util::PlayHapticTouchpadEffect(
          ui::HapticTouchpadEffect::kTick,
          ui::HapticTouchpadEffectStrength::kMedium);
    }
  }
}

void ShelfView::ContinueDrag(const ui::LocatedEvent& event) {
  DCHECK(dragging());
  DCHECK(drag_view_);
  const auto index = view_model_->GetIndexOfView(drag_view_);
  DCHECK(index.has_value());

  const bool dragged_off_shelf_before = dragged_off_shelf_;

  // Handle rip off functionality if this is not a drag and drop host operation
  // and not the app list item.
  if (drag_and_drop_shelf_id_.IsNull() &&
      RemovableByRipOff(index.value()) != NOT_REMOVABLE) {
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

  if (drag_icon_proxy_)
    drag_icon_proxy_->UpdatePosition(drag_point_in_screen);

  // If drag_icon_proxy_ is available, get its item bounds, otherwise (in case
  // of an ApplicationDragAndDropHost drag), last drag icon bounds are cached in
  // `drag_icon_bounds_in_screen_`.
  const gfx::Rect drag_icon_bounds_in_screen =
      drag_icon_proxy_ ? drag_icon_proxy_->GetBoundsInScreen()
                       : drag_icon_bounds_in_screen_;
  delegate_->ScheduleScrollForItemDragIfNeeded(drag_icon_bounds_in_screen);

  if (dragged_off_shelf_before) {
    model_->OnItemReturnedFromRipOff(
        static_cast<int>(view_model_->GetIndexOfView(drag_view_).value()));
  }
}

void ShelfView::MoveDragViewTo(int primary_axis_coordinate) {
  const size_t current_item_index =
      view_model_->GetIndexOfView(drag_view_).value();
  const std::pair<size_t, size_t> indices(GetDragRange(current_item_index));
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

  size_t target_index = views::ViewModelUtils::DetermineMoveIndex(
      *view_model_, drag_view_, shelf_->IsHorizontalAlignment(),
      drag_view_->x(), drag_view_->y());
  target_index = std::clamp(target_index, indices.first, indices.second);

  // Check the relative position of |drag_view_| and its ideal bounds if it can
  // be dragged across the separator to pin.
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

    RelativePosition old_relative_position =
        drag_view_relative_to_ideal_bounds_;
    drag_view_relative_to_ideal_bounds_ =
        drag_view_position < ideal_bound_position ? RelativePosition::kLeft
                                                  : RelativePosition::kRight;
    if (target_index == current_item_index &&
        old_relative_position != drag_view_relative_to_ideal_bounds_) {
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

void ShelfView::HandleRipOffDrag(const ui::LocatedEvent& event) {
  auto current_index = view_model_->GetIndexOfView(drag_view_);
  DCHECK(current_index.has_value());
  std::string dragged_app_id = model_->items()[current_index.value()].id.app_id;

  aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  gfx::Point screen_location = event.root_location();
  ::wm::ConvertPointToScreen(root_window, &screen_location);

  // To avoid ugly forwards and backwards flipping we use different constants
  // for ripping off / re-inserting the items.
  if (dragged_off_shelf_) {
    // If the shelf/overflow bubble bounds contains |screen_location| we insert
    // the item back into the shelf.
    if (GetBoundsForDragInsertInScreen().Contains(screen_location)) {
      if (!is_active_drag_and_drop_host_) {
        const gfx::ImageSkia icon_image =
            drag_view_->GetIconImage(kDragAndDropProxyScale);
        drag_icon_proxy_ = std::make_unique<AppDragIconProxy>(
            root_window, icon_image,
            drag_view_->GetBadgeIconImage(kDragAndDropProxyScale),
            screen_location,
            /*cursor_offset_from_center=*/gfx::Vector2d(),
            /*scale_factor=*/1.0f,
            /*is_folder_icon=*/false, icon_image.size());
      }

      // Re-insert the item and return simply false since the caller will handle
      // the move as in any normal case.
      dragged_off_shelf_ = false;

      // After re-insertion, trigger an animation to ideal bounds to show the
      // ghost view.
      AnimateToIdealBounds();

      return;
    }
    if (drag_icon_proxy_) {
      drag_icon_proxy_->UpdatePosition(screen_location);
    }
    return;
  }

  // Mark the item as dragged off the shelf if the drag distance exceeds
  // `rip_off_distance`.
  int rip_off_distance =
      ShelfConfig::Get()->shelf_size() * kRipOffDistanceFactor;
  int delta = CalculateShelfDistance(screen_location);
  bool dragged_off_shelf = delta > rip_off_distance;

  if (dragged_off_shelf) {
    if (!is_active_drag_and_drop_host_) {
      // Create a new, scaled up drag icon proxy when the item is dragged off
      // shelf - keep cursor position consistent with the  host provided icon.
      const gfx::Point center = drag_view_->GetLocalBounds().CenterPoint();
      const gfx::Vector2d cursor_offset_from_center = drag_origin_ - center;
      const gfx::ImageSkia icon_image =
          drag_view_->GetIconImage(kDragAndDropProxyScale);
      drag_icon_proxy_ = std::make_unique<AppDragIconProxy>(
          root_window, icon_image,
          drag_view_->GetBadgeIconImage(kDragAndDropProxyScale),
          screen_location, cursor_offset_from_center, /*scale_factor=*/1.0f,
          /*is_folder_icon=*/false, icon_image.size());
      delegate_->CancelScrollForItemDrag();
    }

    dragged_off_shelf_ = true;
    RemoveGhostView();

    if (RemovableByRipOff(current_index.value()) == REMOVABLE) {
      // Move the item to the back and hide it. ShelfItemMoved() callback will
      // handle the |view_model_| update and call AnimateToIdealBounds().
      if (current_index.value() !=
          static_cast<size_t>(model_->item_count() - 1)) {
        model_->Move(current_index.value(), model_->item_count() - 1);
      }
      // Make the item partially disappear to show that it will get removed if
      // dropped.
      if (drag_icon_proxy_) {
        drag_icon_proxy_->SetOpacity(kDraggedImageOpacity);
      }
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

  delegate_->CancelScrollForItemDrag();

  auto current_index = view_model_->GetIndexOfView(drag_view_);
  // If the view isn't part of the model anymore, a sync operation must have
  // removed it. In that case we shouldn't change the model and only delete the
  // proxy image.
  if (!current_index.has_value()) {
    drag_icon_proxy_.reset();
    drag_image_layer_.reset();
    return;
  }

  // Set to true when the animation should snap back to where it was before.
  bool snap_back = false;
  // Items which cannot be dragged off will be handled as a cancel.
  if (!cancel) {
    if (RemovableByRipOff(current_index.value()) != REMOVABLE) {
      // Make sure we do not try to remove un-removable items like items which
      // were not pinned or have to be always there.
      cancel = true;
      snap_back = true;
    } else {
      // Make sure the item stays invisible upon removal.
      drag_view_->SetVisible(false);
      ShelfModel::ScopedUserTriggeredMutation user_triggered(model_);
      model_->UnpinAppWithID(model_->items()[current_index.value()].id.app_id);
    }
  }
  if (cancel || snap_back) {
    if (!cancelling_drag_model_changed_) {
      // Only do something if the change did not come through a model change.
      gfx::Rect drag_bounds = drag_icon_proxy_->GetBoundsInScreen();
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
      model_->Move(current_index.value(), start_drag_index_.value());
      AnimateToIdealBounds();
    }
    drag_view_->layer()->SetOpacity(1.0f);
    model_->OnItemReturnedFromRipOff(model_->item_count() - 1);
  }
  drag_icon_proxy_.reset();
  drag_image_layer_.reset();
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

  // Pinned standalone browser apps should not be removable.
  if (IsStandaloneBrowser(app_id))
    return DRAGGABLE;

  return (type == TYPE_PINNED_APP && model_->IsAppPinned(app_id)) ? REMOVABLE
                                                                  : DRAGGABLE;
}

bool ShelfView::SameDragType(ShelfItemType typea, ShelfItemType typeb) const {
  if (IsPinnedShelfItemType(typea) && IsPinnedShelfItemType(typeb))
    return true;
  if (typea == TYPE_UNDEFINED || typeb == TYPE_UNDEFINED) {
    NOTREACHED() << "ShelfItemType must be set.";
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

std::pair<size_t, size_t> ShelfView::GetDragRange(size_t index) {
  DCHECK(base::Contains(visible_views_indices_, index));
  const ShelfItem& dragged_item = model_->items()[index];

  // If |drag_view_| is allowed to be dragged across the separator, return the
  // first and the last index of the |visible_views_indices_|.
  if (CanDragAcrossSeparator(drag_view_)) {
    return std::make_pair(visible_views_indices_[0],
                          visible_views_indices_.back());
  }

  std::optional<size_t> first = std::nullopt;
  std::optional<size_t> last = std::nullopt;
  for (size_t i : visible_views_indices_) {
    if (SameDragType(model_->items()[i].type, dragged_item.type)) {
      if (!first.has_value())
        first = i;
      last = i;
    } else if (first.has_value()) {
      break;
    }
  }
  DCHECK(first.has_value());
  DCHECK(last.has_value());

  // TODO(afakhry): Consider changing this when taking into account inactive
  // desks.
  return std::make_pair(first.value(), last.value());
}

bool ShelfView::ShouldUpdateDraggedViewPinStatus(size_t dragged_view_index) {
  DCHECK(base::Contains(visible_views_indices_, dragged_view_index));
  bool is_moved_item_pinned =
      IsPinnedShelfItemType(model_->items()[dragged_view_index].type);
  if (!separator_index_.has_value()) {
    // If there is no |separator_index_|, all the apps in shelf are expected to
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
  bool should_pinned_by_position =
      dragged_view_index <= separator_index_.value();
  return should_pinned_by_position != is_moved_item_pinned;
}

bool ShelfView::CanDragAcrossSeparator(views::View* drag_view) const {
  DCHECK(drag_view);

  // Only unpinned running apps on shelf can be dragged across the separator to
  // pin.
  bool can_change_pin_state = ShelfItemForView(drag_view)->type == TYPE_APP;

  // Note that |drag_and_drop_shelf_id_| is set only when the current drag view
  // is from app list, which can not be dragged to the unpinned app side.
  return !ShelfItemForView(drag_view)->IsPinStateForced() &&
         drag_and_drop_shelf_id_ == ShelfID() && can_change_pin_state;
}

void ShelfView::OnFadeInAnimationEnded() {
  // Call PreferredSizeChanged() to notify container to re-layout at the end
  // of fade-in animation.
  PreferredSizeChanged();
}

void ShelfView::OnFadeOutAnimationEnded() {
  if (fade_out_animation_tracker_) {
    fade_out_animation_tracker_->Stop();
    fade_out_animation_tracker_.reset();
  }

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

  gfx::Rect shelf_bounds_in_screen;
  if (ShelfConfig::Get()->is_in_app() &&
      display::Screen::GetScreen()->InTabletMode()) {
    // Use the shelf widget background as the menu anchor point in tablet mode
    // and in app.
    ShelfWidget* shelf_widget = shelf_->shelf_widget();
    shelf_bounds_in_screen = shelf_widget->GetOpaqueBackground()->bounds();
    const gfx::Rect widget_bounds =
        shelf_widget->GetRootView()->GetBoundsInScreen();
    shelf_bounds_in_screen.Offset(widget_bounds.x(), widget_bounds.y());
  } else {
    shelf_bounds_in_screen = GetBoundsInScreen();
  }

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
  std::u16string announcement;
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
  announcement_view_->GetViewAccessibility().AnnounceText(announcement);
}

bool ShelfView::IsAnimating() const {
  return bounds_animator_->IsAnimating();
}

gfx::Rect ShelfView::GetDragIconBoundsInScreenForTest() const {
  if (!drag_icon_proxy_)
    return drag_view_ ? drag_view_->GetBoundsInScreen() : gfx::Rect();
  return drag_icon_proxy_->GetBoundsInScreen();
}

void ShelfView::AddAnimationObserver(views::BoundsAnimatorObserver* observer) {
  bounds_animator_->AddObserver(observer);
}

void ShelfView::RemoveAnimationObserver(
    views::BoundsAnimatorObserver* observer) {
  bounds_animator_->RemoveObserver(observer);
}

void ShelfView::AnnounceShelfAutohideBehavior() {
  std::u16string announcement;
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
  announcement_view_->GetViewAccessibility().AnnounceText(announcement);
}

void ShelfView::AnnouncePinUnpinEvent(const ShelfItem& item, bool pinned) {
  std::u16string item_title =
      item.title.empty()
          ? l10n_util::GetStringUTF16(IDS_SHELF_ITEM_GENERIC_NAME)
          : item.title;
  std::u16string announcement = l10n_util::GetStringFUTF16(
      pinned ? IDS_SHELF_ITEM_WAS_PINNED : IDS_SHELF_ITEM_WAS_UNPINNED,
      item_title);
  announcement_view_->GetViewAccessibility().AnnounceText(announcement);
}

void ShelfView::AnnounceSwapEvent(const ShelfItem& first_item,
                                  const ShelfItem& second_item) {
  std::u16string first_item_title =
      first_item.title.empty()
          ? l10n_util::GetStringUTF16(IDS_SHELF_ITEM_GENERIC_NAME)
          : first_item.title;
  std::u16string second_item_title =
      second_item.title.empty()
          ? l10n_util::GetStringUTF16(IDS_SHELF_ITEM_GENERIC_NAME)
          : second_item.title;
  std::u16string announcement = l10n_util::GetStringFUTF16(
      IDS_SHELF_ITEMS_WERE_SWAPPED, first_item_title, second_item_title);
  announcement_view_->GetViewAccessibility().AnnounceText(announcement);
}

gfx::Rect ShelfView::GetBoundsForDragInsertInScreen() {
  const ScrollableShelfView* scrollable_shelf_view =
      shelf_->hotseat_widget()->scrollable_shelf_view();
  gfx::Rect bounds = scrollable_shelf_view->visible_space();
  views::View::ConvertRectToScreen(scrollable_shelf_view, &bounds);
  return bounds;
}

std::optional<size_t> ShelfView::CancelDrag(
    std::optional<size_t> modified_index) {
  drag_scroll_dir_ = 0;
  scrolling_timer_.Stop();
  speed_up_drag_scrolling_.Stop();

  FinalizeRipOffDrag(true);

  delegate_->CancelScrollForItemDrag();
  drag_icon_proxy_.reset();
  drag_image_layer_.reset();

  if (!drag_view_)
    return modified_index;
  bool was_dragging = dragging();
  auto drag_view_index = view_model_->GetIndexOfView(drag_view_);
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
  views::View* modified_view =
      (modified_index.has_value() && !at_end)
          ? view_model_->view_at(modified_index.value())
          : nullptr;
  model_->Move(drag_view_index.value(), start_drag_index_.value());

  // If the modified view will be at the end of the list, return the new end of
  // the list.
  if (at_end)
    return view_model_->view_size();
  return modified_view ? view_model_->GetIndexOfView(modified_view)
                       : std::nullopt;
}

void ShelfView::OnGestureEvent(ui::GestureEvent* event) {
  if (!ShouldHandleGestures(*event))
    return;

  if (HandleGestureEvent(event))
    event->StopPropagation();
}

void ShelfView::MaybeDuplicatePromiseAppForRemoval(
    ShelfAppButton* promise_app_view,
    const ShelfItem& item) {
  if (!ash::features::ArePromiseIconsEnabled()) {
    return;
  }

  if (!promise_app_view || !promise_app_view->is_promise_app()) {
    return;
  }

  if (promise_app_view->app_status() != AppStatus::kInstallSuccess ||
      !promise_app_view->IsDrawn()) {
    return;
  }

  // Search along the `view_model_` for an existing app with the same
  // package id as the promise app to be removed.
  for (const auto& entry : view_model_->entries()) {
    if (entry.view == promise_app_view) {
      continue;
    }
    if (static_cast<ShelfAppButton*>(entry.view)->package_id() ==
        item.id.app_id) {
      // PromiseApps don't get animation for removal if an app already existst
      // in the grid.
      return;
    }
  }

  AddPendingPromiseAppRemoval(item.id.app_id,
                              promise_app_view->icon_image_model());
}

void ShelfView::ShelfItemAdded(int model_index) {
  // ShelfView must keep view_model_ in sync with ShelfModel::items_ as its very
  // first response to ShelfModel changes. Failure to do so can result in UaF in
  // methods like ShelfView::ShelfItemForView, which can be implicitly called by
  // other classes. See https://crbug.com/1238959 for more details.
  const ShelfItem& item(model_->items()[model_index]);
  views::View* view = CreateViewForItem(item);

  {
    base::AutoReset<bool> cancelling_drag(&cancelling_drag_model_changed_,
                                          true);
    model_index = static_cast<int>(CancelDrag(model_index).value());
  }
  view_model_->Add(view, static_cast<size_t>(model_index));

  // If |item| is pinned and the mutation is user-triggered, report the pinning
  // action for accessibility and UMA. Do it now, because if |item| is hidden
  // then we will soon bail out but we still want to report the pinning action.
  if (model_->is_current_mutation_user_triggered() &&
      item.type == TYPE_PINNED_APP) {
    AnnouncePinUnpinEvent(item, /*pinned=*/true);
    RecordPinUnpinUserAction(/*pinned=*/true);
  }

  // Add child view so it has the same ordering as in the |view_model_|.
  // Note: No need to call UpdateShelfItemViewsVisibility() here directly, since
  // it will be called by ScrollableShelfView::ViewHierarchyChanged() as a
  // result of the below call.
  AddChildViewAt(view, model_index);

  if (!IsItemVisible(item))
    return;

  // Hide the view, it'll be made visible when the animation is done. Using
  // opacity 0 here to avoid messing with CalculateIdealBounds which touches
  // the view's visibility.
  view->layer()->SetOpacity(0);

  // Give the button its ideal bounds. That way if we end up animating the
  // button before this animation completes it doesn't appear at some random
  // spot (because it was in the middle of animating from 0,0 0x0 to its
  // target).
  CalculateIdealBounds();
  view->SetBoundsRect(
      view_model_->ideal_bounds(static_cast<size_t>(model_index)));

  if (model_->is_current_mutation_user_triggered() &&
      drag_and_drop_shelf_id_ != item.id) {
    view->ScrollViewToVisible();
  }

  // The first animation moves all the views to their target position. |view|
  // is hidden, so it visually appears as though we are providing space for
  // it. When done we'll fade the view in.
  AnimateToIdealBounds();

  // Attempt to animate the transition from a promise app into an actual app
  std::string package_id = item.package_id;
  auto found = pending_promise_apps_removals_.find(package_id);

  if (item.app_status == AppStatus::kReady &&
      found != pending_promise_apps_removals_.end()) {
    LayoutToIdealBounds();
    static_cast<ShelfAppButton*>(view)->AnimateInFromPromiseApp(
        found->second,
        base::BindRepeating(&ShelfView::FinishAnimationForPromiseApps,
                            weak_factory_.GetWeakPtr(), std::move(package_id)));
    return;
  }
  DCHECK_LE(static_cast<size_t>(model_index), visible_views_indices_.back());
  // TODO(crbug.com/40266934): Remove the check below once the bounds animator
  // works better with zero animation duration.
  if (!bounds_animator_->GetAnimationDuration().is_zero()) {
    bounds_animator_->SetAnimationDelegate(
        view, std::unique_ptr<gfx::AnimationDelegate>(
                  new StartFadeAnimationDelegate(this, view)));
  }
}

void ShelfView::AddPendingPromiseAppRemoval(
    const std::string& id,
    const ui::ImageModel& promise_icon) {
  pending_promise_apps_removals_.emplace(id, promise_icon);
}

void ShelfView::FinishAnimationForPromiseApps(
    const std::string& pending_app_id) {
  auto pending_app_found = pending_promise_apps_removals_.find(pending_app_id);

  // Discard the pending promise app layer.
  if (pending_app_found != pending_promise_apps_removals_.end()) {
    pending_promise_apps_removals_.erase(pending_app_found);
  }
}

void ShelfView::ShelfItemRemoved(int model_index, const ShelfItem& old_item) {
  // ShelfView must keep view_model_ in sync with ShelfModel::items_ as its very
  // first response to ShelfModel changes. Failure to do so can result in UaF in
  // methods like ShelfView::ShelfItemForView, which can be implicitly called by
  // other classes. See https://crbug.com/1238959 for more details.
  //
  // If std::move is not called on |view|, |view| will be deleted once out of
  // scope.
  std::unique_ptr<views::View> view(view_model_->view_at(model_index));

  shelf_button_delegate_->OnButtonWillBeRemoved();

  if (old_item.is_promise_app) {
    MaybeDuplicatePromiseAppForRemoval(static_cast<ShelfAppButton*>(view.get()),
                                       old_item);
  }
  view_model_->Remove(model_index);

  if (old_item.id == context_menu_id_ && shelf_menu_model_adapter_)
    shelf_menu_model_adapter_->Cancel();

  if (old_item.id == item_awaiting_response_)
    ResetActiveMenuModelRequest();

  {
    base::AutoReset<bool> cancelling_drag(&cancelling_drag_model_changed_,
                                          true);
    CancelDrag(std::nullopt);
  }

  if (view.get() == shelf_->tooltip()->GetCurrentAnchorView())
    shelf_->tooltip()->Close();

  // Disable the view while it's fading out to prevent it from getting events.
  view->SetEnabled(false);

  if (view->GetVisible() && view->layer()->opacity() > 0.0f) {
    UpdateShelfItemViewsVisibility();

    // There could be multiple fade out animations running. Only start
    // tracking for the first one.
    if (!fade_out_animation_tracker_) {
      fade_out_animation_tracker_.emplace(
          GetWidget()->GetCompositor()->RequestNewThroughputTracker());
      fade_out_animation_tracker_->Start(metrics_util::ForSmoothnessV3(
          base::BindRepeating(&ReportFadeOutAnimationSmoothness)));
    }

    // The first animation fades out the view. When done we'll animate the rest
    // of the views to their target location.
    bounds_animator_->AnimateViewTo(view.get(), view->bounds());
    auto* const view_ptr = view.get();
    // TODO(crbug.com/40266934): Remove the check below once the bounds animator
    // works better with zero animation duration.
    if (!bounds_animator_->GetAnimationDuration().is_zero()) {
      bounds_animator_->SetAnimationDelegate(
          view_ptr,
          std::make_unique<FadeOutAnimationDelegate>(this, std::move(view)));
    }
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
    RecordPinUnpinUserAction(/*pinned=*/false);
  }
}

void ShelfView::ShelfItemChanged(int model_index, const ShelfItem& old_item) {
  // Bail if the view and shelf sizes do not match. ShelfItemChanged may be
  // called here before ShelfItemAdded, due to ChromeShelfController's
  // item initialization, which calls SetItem during ShelfItemAdded.
  if (model_->items().size() != view_model_->view_size())
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
    model_index = static_cast<int>(CancelDrag(model_index).value());
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
      if (model_->is_current_mutation_user_triggered()) {
        AnnouncePinUnpinEvent(old_item, item.type == TYPE_PINNED_APP);
        RecordPinUnpinUserAction(item.type == TYPE_PINNED_APP);
      }
      AnimateToIdealBounds();
    }
    return;
  }

  views::View* view = view_model_->view_at(model_index);
  switch (item.type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP:
    case TYPE_UNPINNED_BROWSER_SHORTCUT:
    case TYPE_DIALOG: {
      CHECK(views::IsViewClass<ShelfAppButton>(view));
      ShelfAppButton* button = static_cast<ShelfAppButton*>(view);
      UpdateButton(button, item);
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
  for (size_t visible_index : visible_views_indices_)
    view_model_->view_at(visible_index)->DeprecatedLayoutImmediately();

  AnnounceShelfAlignment();
}

void ShelfView::OnShelfAutoHideBehaviorChanged() {
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
      ShowMenu(std::make_unique<ShelfApplicationMenuModel>(
                   item.title, std::move(menu_items),
                   model_->GetShelfItemDelegate(item.id)),
               sender, item.id, gfx::Point(), /*context_menu=*/false,
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
  if (!model) {
    const int64_t display_id = GetDisplayIdForView(this);
    model = std::make_unique<ShelfContextMenuModel>(nullptr, display_id,
                                                    /*menu_in_shelf=*/true);
  }
  ShowMenu(std::move(model), source, shelf_id, point, /*context_menu=*/true,
           source_type);
}

void ShelfView::ShowMenu(std::unique_ptr<ui::SimpleMenuModel> menu_model,
                         views::View* source,
                         const ShelfID& shelf_id,
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

  context_menu_id_ = shelf_id;

  closing_event_time_ = base::TimeTicks();

  // NOTE: If you convert to HAS_MNEMONICS be sure to update menu building code.
  int run_types = views::MenuRunner::USE_ASH_SYS_UI_LAYOUT;
  if (context_menu) {
    run_types |=
        views::MenuRunner::CONTEXT_MENU | views::MenuRunner::FIXED_ANCHOR;
  }

  const ShelfItem* item = ShelfItemForView(source);

  if ((source_type == ui::MenuSourceType::MENU_SOURCE_MOUSE ||
       source_type == ui::MenuSourceType::MENU_SOURCE_KEYBOARD) &&
      item) {
    views::InkDrop::Get(source)->GetInkDrop()->AnimateToState(
        views::InkDropState::ACTIVATED);
  }

  // Only selected shelf items with context menu opened can be dragged.
  if (context_menu && item && ShelfButtonIsInDrag(item->type, source) &&
      source_type == ui::MenuSourceType::MENU_SOURCE_TOUCH) {
    run_types |= views::MenuRunner::SEND_GESTURE_EVENTS_TO_OWNER;
  }

  // UnsafeDangling triaged in https://crbug.com/1423849.
  shelf_menu_model_adapter_ = std::make_unique<ShelfMenuModelAdapter>(
      item ? item->id.app_id : std::string(), std::move(menu_model), source,
      source_type,
      base::BindOnce(&ShelfView::OnMenuClosed, base::Unretained(this),
                     base::UnsafeDangling(source)),
      display::Screen::GetScreen()->InTabletMode(),
      /*for_application_menu_items*/ !context_menu);
  shelf_menu_model_adapter_->Run(
      GetMenuAnchorRect(*source, click_point, context_menu),
      shelf_->IsHorizontalAlignment()
          ? views::MenuAnchorPosition::kBubbleTopRight
          : views::MenuAnchorPosition::kBubbleLeft,
      run_types);

  if (!context_menu_shown_callback_.is_null())
    context_menu_shown_callback_.Run();
}

void ShelfView::OnMenuClosed(MayBeDangling<views::View> source) {
  context_menu_id_ = ShelfID();

  closing_event_time_ = shelf_menu_model_adapter_->GetClosingEventTime();

  const ShelfItem* item = ShelfItemForView(source);
  if (item) {
    static_cast<ShelfAppButton*>(source)->OnMenuClosed();
  }

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

  // Notify user education features that anchor bounds have changed.
  if (features::IsUserEducationEnabled()) {
    views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
        user_education::kHelpBubbleAnchorBoundsChangedEvent, this);
  }

  // Do not call PreferredSizeChanged() so that container does not re-layout
  // during the bounds animation.
}

void ShelfView::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  if (move_animation_tracker_) {
    move_animation_tracker_->Stop();
    move_animation_tracker_.reset();
  }

  if (snap_back_from_rip_off_view_ && animator == bounds_animator_.get()) {
    if (!animator->IsAnimating(snap_back_from_rip_off_view_)) {
      // Coming here the animation of the ShelfAppButton is finished and the
      // previously hidden status can be shown again. Since the button itself
      // might have gone away or changed locations we check that the button
      // is still in the shelf and show its status again.
      if (base::Contains(view_model_->entries(), snap_back_from_rip_off_view_,
                         &views::ViewModelBase::Entry::view))
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
  const auto view_index = view_model_->GetIndexOfView(view);
  return (!view_index.has_value()) ? nullptr
                                   : &(model_->items()[view_index.value()]);
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

bool ShelfView::ShouldHandleGestures(const ui::GestureEvent& event) const {
  if (event.type() == ui::EventType::kGestureScrollBegin) {
    float x_offset = event.details().scroll_x_hint();
    float y_offset = event.details().scroll_y_hint();
    if (!shelf_->IsHorizontalAlignment())
      std::swap(x_offset, y_offset);

    return std::abs(x_offset) < std::abs(y_offset);
  }

  return true;
}

std::u16string ShelfView::GetTitleForChildView(const views::View* view) const {
  const ShelfItem* item = ShelfItemForView(view);
  return item ? item->title : std::u16string();
}

void ShelfView::UpdateShelfItemViewsVisibility() {
  visible_views_indices_.clear();
  for (size_t i = 0; i < view_model_->view_size(); ++i) {
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

  // Note that `edge_padding_insets` fetched from `scrollable_shelf_view` is
  // mirrored under RTL.
  if (scrollable_shelf_view->ShouldAdaptToRTL())
    return edge_padding_insets.right();

  return shelf_->PrimaryAxisValue(edge_padding_insets.left(),
                                  edge_padding_insets.top());
}

gfx::Rect ShelfView::GetChildViewTargetMirroredBounds(
    const views::View* child) const {
  DCHECK_EQ(this, child->parent());
  return GetMirroredRect(bounds_animator_->GetTargetBounds(child));
}

void ShelfView::RemoveGhostView() {
  if (current_ghost_view_) {
    current_ghost_view_index_ = std::nullopt;
    current_ghost_view_->FadeOut();
    current_ghost_view_ = nullptr;
  }

  if (last_ghost_view_) {
    last_ghost_view_->FadeOut();
    last_ghost_view_ = nullptr;
  }
}

void ShelfView::ResetActiveMenuModelRequest() {
  context_menu_callback_.Cancel();
  item_awaiting_response_ = ShelfID();
}

views::View::DropCallback ShelfView::GetDropCallback(
    const ui::DropTargetEvent& event) {
  return base::BindOnce(&ShelfView::EndDragCallback, base::Unretained(this));
}

void ShelfView::EndDragCallback(
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  // TODO(b/271601288): Hook up drop animation with the drag image icon.
  output_drag_op = ui::mojom::DragOperation::kMove;
  drag_image_layer_ = std::move(drag_image_layer_owner);
  EndDrag(false);
}

bool ShelfView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  format_types->insert(GetAppItemFormatType());
  return true;
}

bool ShelfView::CanDrop(const OSExchangeData& data) {
  auto app_info = GetAppInfoFromDropDataForAppType(data);
  if (!app_info || app_info->IsValid()) {
    return false;
  }

  std::set<ui::ClipboardFormatType> format_types;
  format_types.insert(GetAppItemFormatType());
  return data.HasAnyFormat(0, format_types) &&
         app_info->type == DraggableAppType::kAppGridItem;
}

void ShelfView::OnDragExited() {
  EndDrag(/*cancel=*/true);
}

void ShelfView::OnDragEntered(const ui::DropTargetEvent& event) {
  auto app_info = GetAppInfoFromDropDataForAppType(event.data());
  if (!app_info || app_info->IsValid()) {
    return;
  }

  std::string app_id = app_info->app_id;
  if (app_id.empty() || app_info->type != DraggableAppType::kAppGridItem) {
    views::View::OnDragEntered(event);
    return;
  }

  gfx::Point drag_point_in_screen = event.location();
  views::View::ConvertPointToScreen(this, &drag_point_in_screen);
  StartDrag(app_id, drag_point_in_screen, gfx::Rect());
}

int ShelfView::OnDragUpdated(const ui::DropTargetEvent& event) {
  gfx::Point drag_point_in_screen = event.location();
  views::View::ConvertPointToScreen(this, &drag_point_in_screen);
  Drag(drag_point_in_screen,
       drag_view_ ? drag_view_->GetBoundsInScreen() : gfx::Rect());
  return ui::DragDropTypes::DRAG_MOVE;
}

BEGIN_METADATA(ShelfView)
END_METADATA

}  // namespace ash
