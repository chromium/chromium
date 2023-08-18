// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_item.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_label_widget.h"
#include "ash/style/system_shadow.h"
#include "ash/wm/desks/desk_bar_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/templates/saved_desk_animations.h"
#include "ash/wm/drag_window_controller.h"
#include "ash/wm/overview/delayed_animation_observer_impl.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_event_handler.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/overview/scoped_overview_hide_windows.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_mini_view_header_view.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

using ::chromeos::WindowStateType;

// Opacity for fading out during closing a window.
constexpr float kClosingItemOpacity = 0.8f;

// Before closing a window animate both the window and the caption to shrink by
// this fraction of size.
constexpr float kPreCloseScale = 0.02f;

// The amount of translation an item animates by when it is closed by using
// swipe to close.
constexpr int kSwipeToCloseCloseTranslationDp = 96;

// When an item is being dragged, the bounds are outset horizontally by this
// fraction of the width, and vertically by this fraction of the height. The
// outset in each dimension is on both sides, for a total of twice this much
// change in the size of the item along that dimension.
constexpr float kDragWindowScale = 0.05f;

// The shadow types corresponding to the default and dragged states.
constexpr SystemShadow::Type kDefaultShadowType =
    SystemShadow::Type::kElevation12;
constexpr SystemShadow::Type kDraggedShadowType =
    SystemShadow::Type::kElevation24;

// A self-deleting animation observer that runs the given callback when its
// associated animation completes. Optionally takes a callback that is run when
// the animation starts.
class AnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit AnimationObserver(base::OnceClosure on_animation_finished)
      : AnimationObserver(base::NullCallback(),
                          std::move(on_animation_finished)) {}

  AnimationObserver(base::OnceClosure on_animation_started,
                    base::OnceClosure on_animation_finished)
      : on_animation_started_(std::move(on_animation_started)),
        on_animation_finished_(std::move(on_animation_finished)) {
    DCHECK(!on_animation_finished_.is_null());
  }

  AnimationObserver(const AnimationObserver&) = delete;
  AnimationObserver& operator=(const AnimationObserver&) = delete;

  ~AnimationObserver() override = default;

  // ui::ImplicitAnimationObserver:
  void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override {
    if (!on_animation_started_.is_null())
      std::move(on_animation_started_).Run();
  }
  void OnImplicitAnimationsCompleted() override {
    std::move(on_animation_finished_).Run();
    delete this;
  }

 private:
  base::OnceClosure on_animation_started_;
  base::OnceClosure on_animation_finished_;
};

// Applies |new_bounds_in_screen| to |widget|, animating and observing the
// transform if necessary.
void SetWidgetBoundsAndMaybeAnimateTransform(
    views::Widget* widget,
    const gfx::Rect& new_bounds_in_screen,
    OverviewAnimationType animation_type,
    ui::ImplicitAnimationObserver* observer) {
  aura::Window* window = widget->GetNativeWindow();
  gfx::RectF previous_bounds = gfx::RectF(window->GetBoundsInScreen());
  window->SetBoundsInScreen(
      new_bounds_in_screen,
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
  if (animation_type == OVERVIEW_ANIMATION_NONE ||
      animation_type == OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER ||
      previous_bounds.IsEmpty()) {
    window->SetTransform(gfx::Transform());

    // Make sure that |observer|, which could be a self-deleting object, will
    // not be leaked.
    DCHECK(!observer);
    return;
  }

  // For animations, compute the transform needed to place the widget at its
  // new bounds back to the old bounds, and then apply the idenity
  // transform. This so the bounds visually line up the concurrent transform
  // animations. Also transform animations may be more performant.
  const gfx::RectF current_bounds = gfx::RectF(window->GetBoundsInScreen());
  window->SetTransform(
      gfx::TransformBetweenRects(current_bounds, previous_bounds));
  ScopedOverviewAnimationSettings settings(animation_type, window);
  if (observer)
    settings.AddObserver(observer);
  window->SetTransform(gfx::Transform());
}

}  // namespace

OverviewItem::OverviewItem(aura::Window* window,
                           OverviewSession* overview_session,
                           OverviewGrid* overview_grid)
    : root_window_(window->GetRootWindow()),
      transform_window_(this, window),
      overview_session_(overview_session),
      overview_grid_(overview_grid),
      animation_disabler_(window) {
  CreateItemWidget();
  window->AddObserver(this);
  WindowState::Get(window)->AddObserver(this);
}

OverviewItem::~OverviewItem() {
  aura::Window* window = GetWindow();
  WindowState::Get(window)->RemoveObserver(this);
  window->RemoveObserver(this);
}

aura::Window* OverviewItem::GetWindow() {
  return transform_window_.window();
}

bool OverviewItem::Contains(const aura::Window* target) const {
  return transform_window_.Contains(target);
}

void OverviewItem::HideForSavedDeskLibrary(bool animate) {
  // To hide the window, we will set its layer opacity to 0. This would normally
  // also hide the window from the mini view, which we don't want. By setting a
  // property on the window, we can force it to stay visible.
  GetWindow()->SetProperty(kForceVisibleInMiniViewKey, true);

  // Temporarily hide this window in overview, so that dark/light theme change
  // does not reset the layer visible. If `animate` is false, the callback will
  // not run in `PerformFadeOutLayer`. Thus, here we make sure the window is
  // also hidden in that case.
  DCHECK(item_widget_);
  hide_window_in_overview_callback_.Reset(base::BindOnce(
      &OverviewItem::HideWindowInOverview, weak_ptr_factory_.GetWeakPtr()));
  PerformFadeOutLayer(item_widget_->GetLayer(), animate,
                      hide_window_in_overview_callback_.callback());
  if (!animate) {
    // Cancel the callback if we are going to run it directly.
    hide_window_in_overview_callback_.Cancel();
    HideWindowInOverview();
  }

  for (aura::Window* transient_child : GetTransientTreeIterator(GetWindow())) {
    transient_child->SetProperty(kForceVisibleInMiniViewKey, true);
    PerformFadeOutLayer(transient_child->layer(), animate, base::DoNothing());
  }

  item_widget_event_blocker_ =
      std::make_unique<aura::ScopedWindowEventTargetingBlocker>(
          item_widget_->GetNativeWindow());

  HideCannotSnapWarning(animate);
}

void OverviewItem::RevertHideForSavedDeskLibrary(bool animate) {
  // This might run before `HideForSavedDeskLibrary()`, thus cancel the
  // callback to prevent such case.
  hide_window_in_overview_callback_.Cancel();

  // Restore and show the window back to overview.
  ShowWindowInOverview();

  // `item_widget_` may be null during shutdown if the window is minimized.
  if (item_widget_) {
    PerformFadeInLayer(item_widget_->GetLayer(), animate);
  }

  for (aura::Window* transient_child :
       GetTransientTreeIterator(transform_window_.window())) {
    PerformFadeInLayer(transient_child->layer(), animate);
  }

  item_widget_event_blocker_.reset();

  UpdateCannotSnapWarningVisibility(animate);
}

void OverviewItem::OnMovingWindowToAnotherDesk() {
  is_moving_to_another_desk_ = true;
  // Restore the dragged item window, so that its transform is reset to
  // identity.
  RestoreWindow(/*reset_transform=*/true, /*animate=*/true);
}

void OverviewItem::RestoreWindow(bool reset_transform, bool animate) {
  // TODO(oshima): SplitViewController has its own logic to adjust the
  // target state in |SplitViewController::OnOverviewModeEnding|.
  // Unify the mechanism to control it and remove ifs.
  if (Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      !SplitViewController::Get(root_window_)->InSplitViewMode() &&
      reset_transform) {
    MaximizeIfSnapped(GetWindow());
  }

  GetWindow()->ClearProperty(kForceVisibleInMiniViewKey);
  for (aura::Window* transient_child : GetTransientTreeIterator(GetWindow()))
    transient_child->ClearProperty(kForceVisibleInMiniViewKey);

  overview_item_view_->OnOverviewItemWindowRestoring();
  transform_window_.RestoreWindow(reset_transform, animate);

  if (!transform_window_.IsMinimized())
    return;

  const auto enter_exit_type = overview_session_->enter_exit_overview_type();
  if (is_moving_to_another_desk_ ||
      enter_exit_type == OverviewEnterExitType::kImmediateExit) {
    overview_session_->highlight_controller()->OnViewDestroyingOrDisabling(
        overview_item_view_);
    ImmediatelyCloseWidgetOnExit(std::move(item_widget_));
    overview_item_view_ = nullptr;
    return;
  }

  OverviewAnimationType animation_type =
      GetExitOverviewAnimationTypeForMinimizedWindow(enter_exit_type);
  FadeOutWidgetFromOverview(std::move(item_widget_), animation_type);
}

void OverviewItem::EnsureVisible() {
  transform_window_.EnsureVisible();
}

void OverviewItem::Shutdown() {
  TRACE_EVENT0("ui", "OverviewItem::Shutdown");
  // If `hide_windows` still manages the visibility of this overview item
  // window, remove it from the list without showing.
  ScopedOverviewHideWindows* hide_windows =
      overview_session_->hide_windows_for_saved_desks_grid();
  if (item_widget_ && hide_windows &&
      hide_windows->HasWindow(item_widget_->GetNativeWindow())) {
    hide_windows->RemoveWindow(item_widget_->GetNativeWindow(),
                               /*show_window=*/false);
  }

  DestroyMirrorsForDragging();
  item_widget_.reset();
  overview_item_view_ = nullptr;
}

void OverviewItem::PrepareForOverview() {
  transform_window_.PrepareForOverview();
  prepared_for_overview_ = true;
}

float OverviewItem::GetItemScale(const gfx::Size& size) {
  gfx::SizeF inset_size(size.width(), size.height());
  return ScopedOverviewTransformWindow::GetItemScale(
      GetTargetBoundsInScreen().size(), inset_size,
      transform_window_.GetTopInset(), kHeaderHeightDp);
}

gfx::RectF OverviewItem::GetTransformedBounds() const {
  return transform_window_.GetTransformedBounds();
}

gfx::RectF OverviewItem::GetTargetBoundsInScreen() const {
  return ::ash::GetTargetBoundsInScreen(transform_window_.window());
}

gfx::RectF OverviewItem::GetWindowTargetBoundsWithInsets() const {
  gfx::RectF window_target_bounds = target_bounds_;
  window_target_bounds.Inset(gfx::InsetsF::TLBR(kHeaderHeightDp, 0, 0, 0));
  return window_target_bounds;
}

void OverviewItem::SetBounds(const gfx::RectF& target_bounds,
                             OverviewAnimationType animation_type) {
  if (in_bounds_update_ ||
      !Shell::Get()->overview_controller()->InOverviewSession()) {
    return;
  }

  // Do not animate if the resulting bounds does not change or current animation
  // is still in progress. The original window may change bounds so we still
  // need to call `SetItemBounds()` to update the window transform.
  OverviewAnimationType new_animation_type = animation_type;
  if (GetWindow()->layer()->GetAnimator()->is_animating() ||
      target_bounds == target_bounds_) {
    new_animation_type = OVERVIEW_ANIMATION_NONE;
  }

  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  // If `target_bounds_` is empty, this is the first update. Let
  // `UpdateHeaderLayout()` know, as we do not want `item_widget_` to be
  // animated with the window.
  const bool is_first_update = target_bounds_.IsEmpty();
  target_bounds_ = target_bounds;

  // Run at the exit of this function to update rounded corners, shadow and the
  // cannot snap widget.
  base::ScopedClosureRunner at_exit_runner(base::BindOnce(
      [](base::WeakPtr<OverviewItem> item,
         OverviewAnimationType animation_type) {
        if (!item.get()) {
          return;
        }

        // Shadow is normally set after an animation is finished. In the case of
        // no animations, manually set the shadow. Shadow relies on both the
        // window transform and `item_widget_`'s new bounds so set it after
        // `SetItemBounds()` and `UpdateHeaderLayout()`. Do not apply the shadow
        // for drop target.
        if (animation_type == OVERVIEW_ANIMATION_NONE) {
          item->UpdateRoundedCornersAndShadow();
        }

        if (RoundedLabelWidget* widget = item->cannot_snap_widget_.get()) {
          SetWidgetBoundsAndMaybeAnimateTransform(
              widget,
              widget->GetBoundsCenteredIn(ToStableSizeRoundedRect(
                  item->GetWindowTargetBoundsWithInsets())),
              animation_type, nullptr);
        }
      },
      weak_ptr_factory_.GetWeakPtr(), new_animation_type));

  // For non minimized windows, we simply apply the transform and update the
  // header.
  if (!transform_window_.IsMinimized()) {
    UpdateHeaderLayout(is_first_update ? OVERVIEW_ANIMATION_NONE
                                       : new_animation_type);
    SetItemBounds(target_bounds, new_animation_type, is_first_update);
    return;
  }

  // If the window is minimized we can avoid applying transforms on the original
  // window.
  item_widget_->GetLayer()->GetAnimator()->StopAnimating();

  const gfx::Rect minimized_bounds = ToStableSizeRoundedRect(target_bounds);
  OverviewAnimationType minimized_animation_type =
      is_first_update ? OVERVIEW_ANIMATION_NONE : new_animation_type;
  SetWidgetBoundsAndMaybeAnimateTransform(
      item_widget_.get(), minimized_bounds, minimized_animation_type,
      minimized_animation_type ==
              OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW
          ? new AnimationObserver{base::BindOnce(
                                      &OverviewItem::
                                          OnItemBoundsAnimationStarted,
                                      weak_ptr_factory_.GetWeakPtr()),
                                  base::BindOnce(
                                      &OverviewItem::OnItemBoundsAnimationEnded,
                                      weak_ptr_factory_.GetWeakPtr())}
          : nullptr);

  // Minimized windows have a `WindowPreviewView` which mirrors content from the
  // window. `target_bounds` may not have a matching aspect ratio to the
  // actual window (eg. in splitview overview). In this case, the contents
  // will be squashed to fit the given bounds. To get around this, stretch out
  // the contents so that it matches `unclipped_size_`, then clip the layer to
  // match `target_bounds`. This is what is done on non-minimized windows.
  auto* preview_view = overview_item_view_->preview_view();
  CHECK(preview_view);
  ui::Layer* preview_layer = preview_view->layer();
  if (unclipped_size_) {
    gfx::SizeF target_size(*unclipped_size_);
    gfx::SizeF preview_size = GetWindowTargetBoundsWithInsets().size();
    target_size.Enlarge(0, -kHeaderHeightDp);

    const float x_scale = target_size.width() / preview_size.width();
    const float y_scale = target_size.height() / preview_size.height();
    const auto transform = gfx::Transform::MakeScale(x_scale, y_scale);
    preview_layer->SetTransform(transform);

    // Transform affects clip rect so scale the clip rect so that the final
    // size is equal to the untransformed layer.
    gfx::Size clip_size(preview_layer->size());
    clip_size =
        gfx::ScaleToRoundedSize(clip_size, 1.f / x_scale, 1.f / y_scale);
    preview_layer->SetClipRect(gfx::Rect(clip_size));
  } else {
    preview_layer->SetClipRect(gfx::Rect());
    preview_layer->SetTransform(gfx::Transform());
  }

  if (!is_first_update)
    return;

  // On the first update show `item_widget_`. It's created on creation of
  // `this`, and needs to be shown as soon as its bounds have been determined
  // as it contains a mirror view of the window in its contents. The header
  // will be faded in later to match non minimized windows.
  if (!should_animate_when_entering_) {
    item_widget_->GetLayer()->SetOpacity(1.f);
    return;
  }

  if (new_animation_type == OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW) {
    PerformItemSpawnedAnimation(item_widget_->GetNativeWindow(),
                                gfx::Transform{});
    return;
  }

  // If entering from home launcher, use the home specific (fade) animation.
  OverviewAnimationType fade_animation = animation_type;
  if (fade_animation != OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER)
    fade_animation = OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN;

  FadeInWidgetToOverview(item_widget_.get(), fade_animation,
                         /*observe=*/true);

  // Update the item header visibility immediately if entering from home
  // launcher.
  if (new_animation_type == OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER) {
    overview_item_view_->SetHeaderVisibility(
        OverviewItemView::HeaderVisibility::kVisible, /*animate=*/true);
  }

  // Update the item header visibility immediately without an animation.
  if (new_animation_type == OVERVIEW_ANIMATION_NONE) {
    overview_item_view_->SetHeaderVisibility(
        OverviewItemView::HeaderVisibility::kVisible, /*animate=*/false);
  }
}

void OverviewItem::SendAccessibleSelectionEvent() {
  overview_item_view_->NotifyAccessibilityEvent(ax::mojom::Event::kSelection,
                                                true);
}

void OverviewItem::AnimateAndCloseWindow(bool up) {
  base::RecordAction(base::UserMetricsAction("WindowSelector_SwipeToClose"));

  animating_to_close_ = true;
  overview_session_->PositionWindows(/*animate=*/true);
  overview_item_view_->OnOverviewItemWindowRestoring();

  int translation_y = kSwipeToCloseCloseTranslationDp * (up ? -1 : 1);
  gfx::Transform transform;
  transform.Translate(gfx::Vector2d(0, translation_y));

  auto animate_window = [this](aura::Window* window,
                               const gfx::Transform& transform, bool observe) {
    ScopedOverviewAnimationSettings settings(
        OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM, window);
    gfx::Transform original_transform = window->transform();
    original_transform.PostConcat(transform);
    window->SetTransform(original_transform);
    if (observe) {
      settings.AddObserver(new AnimationObserver{
          base::BindOnce(&OverviewItem::OnWindowCloseAnimationCompleted,
                         weak_ptr_factory_.GetWeakPtr())});
    }
  };

  AnimateOpacity(0.0, OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM);
  if (cannot_snap_widget_)
    animate_window(cannot_snap_widget_->GetNativeWindow(), transform, false);
  if (!transform_window_.IsMinimized())
    animate_window(GetWindow(), transform, false);
  animate_window(item_widget_->GetNativeWindow(), transform, true);
}

void OverviewItem::CloseWindow() {
  SetShadowBounds(absl::nullopt);

  gfx::RectF inset_bounds(target_bounds_);
  inset_bounds.Inset(gfx::InsetsF::VH(target_bounds_.height() * kPreCloseScale,
                                      target_bounds_.width() * kPreCloseScale));
  // Scale down both the window and label.
  SetBounds(inset_bounds, OVERVIEW_ANIMATION_CLOSING_OVERVIEW_ITEM);
  // First animate opacity to an intermediate value concurrently with the
  // scaling animation.
  AnimateOpacity(kClosingItemOpacity, OVERVIEW_ANIMATION_CLOSING_OVERVIEW_ITEM);

  // Fade out the window and the label, effectively hiding them.
  AnimateOpacity(0.0, OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM);

  // |transform_window_| will delete |this| by deleting the widget associated
  // with |this|.
  transform_window_.Close();
}

void OverviewItem::UpdateCannotSnapWarningVisibility(bool animate) {
  // Windows which can snap will never show this warning. Or if the window is
  // the drop target window, also do not show this warning.
  bool visible = true;
  if (SplitViewController::Get(root_window_)
          ->ComputeSnapRatio(GetWindow())
          .has_value() ||
      overview_grid_->IsDropTargetWindow(GetWindow())) {
    visible = false;
  } else {
    const SplitViewController::State state =
        SplitViewController::Get(root_window_)->state();
    visible = state == SplitViewController::State::kPrimarySnapped ||
              state == SplitViewController::State::kSecondarySnapped;
  }

  if (!visible && !cannot_snap_widget_)
    return;

  if (!cannot_snap_widget_) {
    RoundedLabelWidget::InitParams params;
    params.horizontal_padding = kSplitviewLabelHorizontalInsetDp;
    params.vertical_padding = kSplitviewLabelVerticalInsetDp;
    params.rounding_dp = kSplitviewLabelRoundRectRadiusDp;
    params.preferred_height = kSplitviewLabelPreferredHeightDp;
    params.message_id = IDS_ASH_SPLIT_VIEW_CANNOT_SNAP;
    params.parent = GetWindow()->parent();
    cannot_snap_widget_ = std::make_unique<RoundedLabelWidget>();
    cannot_snap_widget_->Init(std::move(params));
    GetWindow()->parent()->StackChildAbove(
        cannot_snap_widget_->GetNativeWindow(), GetWindow());
  }
  if (animate) {
    DoSplitviewOpacityAnimation(
        cannot_snap_widget_->GetLayer(),
        visible ? SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_IN
                : SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_OUT);
  } else {
    cannot_snap_widget_->GetLayer()->SetOpacity(visible ? 1.f : 0.f);
  }
  const gfx::Rect bounds =
      ToStableSizeRoundedRect(GetWindowTargetBoundsWithInsets());
  cannot_snap_widget_->SetBoundsCenteredIn(bounds, /*animate=*/false);
}

void OverviewItem::HideCannotSnapWarning(bool animate) {
  if (!cannot_snap_widget_)
    return;
  if (animate) {
    DoSplitviewOpacityAnimation(cannot_snap_widget_->GetLayer(),
                                SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_OUT);
  } else {
    cannot_snap_widget_->GetLayer()->SetOpacity(0.f);
  }
}

void OverviewItem::OnSelectorItemDragStarted(OverviewItem* item) {
  is_being_dragged_ = (item == this);

  overview_item_view_->SetHeaderVisibility(
      is_being_dragged_ && !chromeos::features::IsJellyrollEnabled()
          ? OverviewItemView::HeaderVisibility::kInvisible
          : OverviewItemView::HeaderVisibility::kCloseButtonInvisibleOnly,
      /*animate=*/true);
}

void OverviewItem::OnSelectorItemDragEnded(bool snap) {
  if (snap) {
    if (!is_being_dragged_)
      overview_item_view_->HideCloseInstantlyAndThenShowItSlowly();
  } else {
    overview_item_view_->SetHeaderVisibility(
        OverviewItemView::HeaderVisibility::kVisible, /*animate=*/true);
  }
  is_being_dragged_ = false;
}

void OverviewItem::SetVisibleDuringWindowDragging(bool visible, bool animate) {
  aura::Window::Windows windows = GetWindowsForHomeGesture();
  float new_opacity = visible ? 1.f : 0.f;
  for (auto* window : windows) {
    ui::Layer* layer = window->layer();
    if (layer->GetTargetOpacity() == new_opacity)
      continue;

    if (animate) {
      ScopedOverviewAnimationSettings settings(
          OVERVIEW_ANIMATION_OPACITY_ON_WINDOW_DRAG, window);
      layer->SetOpacity(new_opacity);
    } else {
      layer->SetOpacity(new_opacity);
    }
  }
}

OverviewGridWindowFillMode OverviewItem::GetWindowDimensionsType() const {
  return transform_window_.type();
}

void OverviewItem::UpdateWindowDimensionsType() {
  transform_window_.UpdateWindowDimensionsType();
  const bool show_backdrop =
      GetWindowDimensionsType() != OverviewGridWindowFillMode::kNormal;
  overview_item_view_->SetBackdropVisibility(show_backdrop);
}

void OverviewItem::ScaleUpSelectedItem(OverviewAnimationType animation_type) {
  gfx::RectF scaled_bounds = target_bounds();
  scaled_bounds.Inset(
      gfx::InsetsF::VH(-scaled_bounds.height() * kDragWindowScale,
                       -scaled_bounds.width() * kDragWindowScale));
  if (unclipped_size_) {
    // If a clipped item is scaled up, we need to recalculate the unclipped
    // size.
    const int height = scaled_bounds.height();
    const int width =
        overview_grid_->CalculateWidthAndMaybeSetUnclippedBounds(this, height);
    DCHECK(unclipped_size_);
    const gfx::SizeF new_size(width, height);
    scaled_bounds.set_size(new_size);
    scaled_bounds.ClampToCenteredSize(new_size);
  }
  SetBounds(scaled_bounds, animation_type);
}

void OverviewItem::UpdateItemContentViewForMinimizedWindow() {
  overview_item_view_->RefreshPreviewView();
}

bool OverviewItem::IsDragItem() const {
  return overview_session_->GetCurrentDraggedOverviewItem() == this;
}

void OverviewItem::Restack() {
  aura::Window* window = GetWindow();
  aura::Window* parent_window = window->parent();
  aura::Window* stacking_target = nullptr;

  // Stack |window| below the split view window if split view is active.
  SplitViewController* split_view_controller =
      SplitViewController::Get(root_window_);
  if (split_view_controller->InSplitViewMode()) {
    aura::Window* snapped_window =
        split_view_controller->GetDefaultSnappedWindow();
    if (snapped_window->parent() == parent_window)
      stacking_target = snapped_window;
  }
  // Stack |window| below the last window in |overview_grid_| that comes before
  // |window| and has the same parent.
  for (const std::unique_ptr<OverviewItem>& overview_item :
       overview_grid_->window_list()) {
    // |Restack| is sometimes called when there is a drop target, but is never
    // used to restack an item that comes after a drop target. In other words,
    // |overview_grid_| might have a drop target, but we will break out of the
    // for loop before reaching it.
    DCHECK(!overview_grid_->IsDropTargetWindow(overview_item->GetWindow()));
    if (overview_item.get() == this)
      break;
    if (overview_item->GetWindow()->parent() == parent_window)
      stacking_target = overview_item->item_widget()->GetNativeWindow();
  }

  if (stacking_target) {
    DCHECK_EQ(parent_window, stacking_target->parent());
    parent_window->StackChildBelow(window, stacking_target);
  }
  DCHECK_EQ(parent_window, item_widget_->GetNativeWindow()->parent());
  parent_window->StackChildBelow(item_widget_->GetNativeWindow(), window);
  if (cannot_snap_widget_) {
    DCHECK_EQ(parent_window, cannot_snap_widget_->GetNativeWindow()->parent());
    parent_window->StackChildAbove(cannot_snap_widget_->GetNativeWindow(),
                                   window);
  }
}

void OverviewItem::UpdateMirrorsForDragging(bool is_touch_dragging) {
  DCHECK_GT(Shell::GetAllRootWindows().size(), 1u);
  const bool is_minimized = transform_window_.IsMinimized();

  // With Jellyroll, header is visible while dragging.
  if (is_minimized || chromeos::features::IsJellyrollEnabled()) {
    if (!item_mirror_for_dragging_) {
      item_mirror_for_dragging_ = std::make_unique<DragWindowController>(
          item_widget_->GetNativeWindow(), is_touch_dragging);
    }
    item_mirror_for_dragging_->Update();
  }

  // Minimized windows don't need to mirror the source as its already in
  // `item_widget_`.
  if (is_minimized) {
    return;
  }

  if (!window_mirror_for_dragging_) {
    window_mirror_for_dragging_ =
        std::make_unique<DragWindowController>(GetWindow(), is_touch_dragging);
  }
  window_mirror_for_dragging_->Update();
}

void OverviewItem::DestroyMirrorsForDragging() {
  item_mirror_for_dragging_.reset();
  window_mirror_for_dragging_.reset();
}

void OverviewItem::SetShadowBounds(
    absl::optional<gfx::RectF> bounds_in_screen) {
  // Shadow is normally turned off during animations and reapplied when they
  // are finished. On destruction, |shadow_| is cleaned up before
  // |transform_window_|, which may call this function, so early exit if
  // |shadow_| is nullptr.
  if (!shadow_)
    return;

  if (!bounds_in_screen) {
    shadow_->GetLayer()->SetVisible(false);
    return;
  }

  shadow_->GetLayer()->SetVisible(true);
  gfx::Rect bounds_in_item =
      gfx::Rect(item_widget_->GetNativeWindow()->GetTargetBounds().size());

  const bool is_jellyroll_enabled = chromeos::features::IsJellyrollEnabled();
  const bool continuous_scroll =
      features::IsContinuousOverviewScrollAnimationEnabled() &&
      Shell::Get()->overview_controller()->is_continuous_scroll_in_progress();
  if (!is_jellyroll_enabled || continuous_scroll) {
    bounds_in_item.Inset(gfx::Insets::TLBR(kHeaderHeightDp, 0, 0, 0));
  }

  bounds_in_item.ClampToCenteredSize(
      gfx::ToRoundedSize(bounds_in_screen->size()));
  shadow_->SetContentBounds(bounds_in_item);
  if (continuous_scroll) {
    shadow_->SetRoundedCornerRadius(/*radius=*/0.f);
  } else if (is_jellyroll_enabled) {
    shadow_->SetRoundedCornerRadius(kOverviewItemCornerRadius);
  }
}

void OverviewItem::UpdateRoundedCornersAndShadow() {
  // Do not show the rounded corners and the shadow if overview is shutting
  // down or we're currently in entering overview animation. Also don't update
  // or animate the window's frame header clip under these conditions. If the
  // feature ContinuousOverviewScrollAnimation is enabled, always show rounded
  // corners for minimized windows, and show rounded corners for non-minimized
  // windows after the continuous scroll has ended.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  const bool is_shutting_down =
      !overview_controller || !overview_controller->InOverviewSession();
  const bool continuous_scroll_in_progress =
      features::IsContinuousOverviewScrollAnimationEnabled() &&
      Shell::Get()->overview_controller()->is_continuous_scroll_in_progress();
  bool show_rounded_corners_for_start_animation = false;
  if (features::IsContinuousOverviewScrollAnimationEnabled() &&
      !Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    show_rounded_corners_for_start_animation =
        transform_window_.IsMinimized() || !continuous_scroll_in_progress;
  } else {
    show_rounded_corners_for_start_animation =
        !overview_controller->IsInStartAnimation();
  }

  const bool should_show_rounded_corners =
      !is_shutting_down && show_rounded_corners_for_start_animation;
  if (transform_window_.IsMinimized()) {
    overview_item_view_->UpdatePreviewRoundedCorners(
        should_show_rounded_corners);
  } else {
    transform_window_.UpdateRoundedCorners(should_show_rounded_corners);
  }

  // In addition, the shadow should be hidden if
  // 1) this overview item is the drop target window or
  // 2) this overview item is in animation.
  // If a continuous scroll is in progress, minimized windows have rounded
  // corners but no shadows.
  bool should_show_shadow_for_rounded_corners = false;
  if (features::IsContinuousOverviewScrollAnimationEnabled()) {
    should_show_shadow_for_rounded_corners =
        !continuous_scroll_in_progress ||
        (continuous_scroll_in_progress && !transform_window_.IsMinimized());
  } else {
    should_show_shadow_for_rounded_corners = should_show_rounded_corners;
  }

  const bool should_show_shadow =
      should_show_shadow_for_rounded_corners &&
      !overview_grid_->IsDropTargetWindow(GetWindow()) &&
      !transform_window_.GetOverviewWindow()
           ->layer()
           ->GetAnimator()
           ->is_animating();

  if (should_show_shadow) {
    // The shadow should always match the size of the item minus the border
    // instead of the transformed window or preview view, since for the window
    // which has `kPillarBoxed` or `kLetterBoxed` dimension types, it doesn't
    // occupy the whole remaining area of the overview item widget minus the
    // header view in which case, the shadow looks weird if it matches the size
    // of the transformed window or preview view.
    gfx::RectF shadow_bounds;
    if (chromeos::features::IsJellyrollEnabled()) {
      shadow_bounds = target_bounds_;
    } else {
      shadow_bounds = GetWindowTargetBoundsWithInsets();
    }
    SetShadowBounds(absl::make_optional(shadow_bounds));
  } else {
    SetShadowBounds(absl::nullopt);
  }
}

void OverviewItem::UpdateShadowTypeForDrag(bool is_dragging) {
  shadow_->SetType(is_dragging ? kDraggedShadowType : kDefaultShadowType);
}

void OverviewItem::OnStartingAnimationComplete() {
  DCHECK(item_widget_);
  if (transform_window_.IsMinimized()) {
    // Fade the title in if minimized. The rest of |item_widget_| should
    // already be shown.
    overview_item_view_->SetHeaderVisibility(
        OverviewItemView::HeaderVisibility::kVisible, /*animate=*/true);
  } else {
    FadeInWidgetToOverview(item_widget_.get(),
                           OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
                           /*observe=*/false);
    // If a continuous scroll has ended, make the header visible again.
    if (!Shell::Get()
             ->overview_controller()
             ->is_continuous_scroll_in_progress()) {
      overview_item_view()->layer()->SetOpacity(1.f);
    }
  }
  const bool show_backdrop =
      GetWindowDimensionsType() != OverviewGridWindowFillMode::kNormal;
  overview_item_view_->SetBackdropVisibility(show_backdrop);
  UpdateCannotSnapWarningVisibility(/*animate=*/true);
}

void OverviewItem::StopWidgetAnimation() {
  DCHECK(item_widget_.get());
  item_widget_->GetNativeWindow()->layer()->GetAnimator()->StopAnimating();
}

void OverviewItem::SetOpacity(float opacity) {
  item_widget_->SetOpacity(opacity);
  transform_window_.SetOpacity(opacity);
  if (cannot_snap_widget_)
    cannot_snap_widget_->SetOpacity(opacity);
}

float OverviewItem::GetOpacity() {
  return item_widget_->GetNativeWindow()->layer()->GetTargetOpacity();
}

OverviewAnimationType OverviewItem::GetExitOverviewAnimationType() const {
  if (overview_session_->enter_exit_overview_type() ==
      OverviewEnterExitType::kImmediateExit) {
    return OVERVIEW_ANIMATION_NONE;
  }

  return should_animate_when_exiting_
             ? OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_EXIT
             : OVERVIEW_ANIMATION_NONE;
}

OverviewAnimationType OverviewItem::GetExitTransformAnimationType() const {
  if (is_moving_to_another_desk_ ||
      overview_session_->enter_exit_overview_type() ==
          OverviewEnterExitType::kImmediateExit) {
    return OVERVIEW_ANIMATION_NONE;
  }

  return should_animate_when_exiting_ ? OVERVIEW_ANIMATION_RESTORE_WINDOW
                                      : OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO;
}

void OverviewItem::HandleGestureEventForTabletModeLayout(
    ui::GestureEvent* event) {
  const gfx::PointF location = event->details().bounding_box_f().CenterPoint();
  switch (event->type()) {
    case ui::ET_SCROLL_FLING_START:
      if (IsDragItem()) {
        HandleFlingStartEvent(location, event->details().velocity_x(),
                              event->details().velocity_y());
      } else {
        overview_grid()->grid_event_handler()->OnGestureEvent(event);
      }
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (std::abs(event->details().scroll_y_hint()) >
          std::abs(event->details().scroll_x_hint())) {
        HandlePressEvent(location, /*from_touch_gesture=*/true);
      } else {
        overview_grid()->grid_event_handler()->OnGestureEvent(event);
      }
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (IsDragItem())
        HandleDragEvent(location);
      else
        overview_grid()->grid_event_handler()->OnGestureEvent(event);
      break;
    case ui::ET_GESTURE_SCROLL_END:
      if (IsDragItem())
        HandleReleaseEvent(location);
      else
        overview_grid()->grid_event_handler()->OnGestureEvent(event);
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      HandlePressEvent(location, /*from_touch_gesture=*/true);
      HandleLongPressEvent(location);
      break;
    case ui::ET_GESTURE_TAP:
      overview_session_->SelectWindow(this);
      break;
    case ui::ET_GESTURE_END:
      HandleGestureEndEvent();
      break;
    default:
      overview_grid()->grid_event_handler()->OnGestureEvent(event);
      break;
  }
}

void OverviewItem::HandleMouseEvent(const ui::MouseEvent& event) {
  if (!overview_session_->CanProcessEvent(this, /*from_touch_gesture=*/false)) {
    return;
  }

  // `event.target()` will be null if we use search+space on this item with
  // chromevox on. Accessibility API will synthesize a mouse event in that case
  // without a target. We just use the centerpoint of the item so that
  // search+space will select the item, leaving overview.
  const gfx::PointF screen_location =
      event.target()
          ? event.target()->GetScreenLocationF(event)
          : gfx::PointF(overview_item_view_->GetBoundsInScreen().CenterPoint());
  switch (event.type()) {
    case ui::ET_MOUSE_PRESSED:
      HandlePressEvent(screen_location, /*from_touch_gesture=*/false);
      break;
    case ui::ET_MOUSE_RELEASED:
      HandleReleaseEvent(screen_location);
      break;
    case ui::ET_MOUSE_DRAGGED:
      HandleDragEvent(screen_location);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void OverviewItem::HandleGestureEvent(ui::GestureEvent* event) {
  if (!overview_session_->CanProcessEvent(this, /*from_touch_gesture=*/true)) {
    event->StopPropagation();
    event->SetHandled();
    return;
  }

  if (ShouldUseTabletModeGridLayout()) {
    HandleGestureEventForTabletModeLayout(event);
    return;
  }

  const gfx::PointF location = event->details().bounding_box_f().CenterPoint();
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      HandlePressEvent(location, /*from_touch_gesture=*/true);
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      HandleDragEvent(location);
      break;
    case ui::ET_SCROLL_FLING_START:
      HandleFlingStartEvent(location, event->details().velocity_x(),
                            event->details().velocity_y());
      break;
    case ui::ET_GESTURE_SCROLL_END:
      HandleReleaseEvent(location);
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      HandleLongPressEvent(location);
      break;
    case ui::ET_GESTURE_TAP:
      HandleTapEvent();
      break;
    case ui::ET_GESTURE_END:
      HandleGestureEndEvent();
      break;
    default:
      break;
  }
}

void OverviewItem::OnHighlightedViewActivated() {
  overview_session_->OnHighlightedItemActivated(this);
}

void OverviewItem::OnHighlightedViewClosed() {
  overview_session_->OnHighlightedItemClosed(this);
}

void OverviewItem::OnWindowPropertyChanged(aura::Window* window,
                                           const void* key,
                                           intptr_t old) {
  DCHECK_EQ(GetWindow(), window);

  if (!prepared_for_overview_)
    return;

  if (key != aura::client::kTopViewInset)
    return;

  if (window->GetProperty(aura::client::kTopViewInset) !=
      static_cast<int>(old)) {
    overview_grid_->PositionWindows(/*animate=*/false);
  }
}

void OverviewItem::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds,
                                         ui::PropertyChangeReason reason) {
  DCHECK_EQ(GetWindow(), window);

  // During preparation, window bounds can change. Ignore bounds change
  // notifications in this case; we'll reposition soon.
  if (!prepared_for_overview_)
    return;

  // Do not update the overview bounds if we're shutting down.
  if (!Shell::Get()->overview_controller()->InOverviewSession())
    return;

  // Do not update the overview item if the window is to be snapped into split
  // view. It will be removed from overview soon and will update overview grid
  // at that moment.
  if (SplitViewController::Get(window)->IsWindowInTransitionalState(window))
    return;

  // The drop target will get its bounds set as opposed to its transform
  // set in |SetItemBounds| so do not position windows again when that
  // particular window has its bounds changed.
  if (overview_grid_->IsDropTargetWindow(window))
    return;

  if (reason == ui::PropertyChangeReason::NOT_FROM_ANIMATION)
    overview_item_view_->RefreshPreviewView();

  // Immediately finish any active bounds animation.
  window->layer()->GetAnimator()->StopAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS);
  UpdateWindowDimensionsType();
  overview_grid_->PositionWindows(/*animate=*/false);
}

void OverviewItem::OnWindowDestroying(aura::Window* window) {
  CHECK_EQ(GetWindow(), window);

  if (is_being_dragged_) {
    CHECK_EQ(this, overview_session_->window_drag_controller()->item());
    overview_session_->window_drag_controller()->ResetGesture();
  }

  // Remove the item from the session which will remove it from the grid in
  // addition to updating accessibility states. If the session is not available
  // then remove it from the grid directly.
  if (overview_session_) {
    overview_session_->RemoveItem(this, /*item_destroying=*/true,
                                  /*reposition=*/!animating_to_close_);
  } else {
    overview_grid()->RemoveItem(this, /*item_destroying=*/true,
                                /*reposition=*/!animating_to_close_);
  }

  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
          IDS_ASH_OVERVIEW_WINDOW_CLOSING_A11Y_ALERT, window->GetTitle()));
}

void OverviewItem::OnPreWindowStateTypeChange(WindowState* window_state,
                                              WindowStateType old_type) {
  // If entering overview and PIP happen at the same time, the PIP window is
  // incorrectly listed in the overview list, which is not allowed.
  if (window_state->IsPip())
    overview_session_->RemoveItem(this);
}

void OverviewItem::OnPostWindowStateTypeChange(WindowState* window_state,
                                               WindowStateType old_type) {
  // During preparation, window state can change, e.g. updating shelf
  // visibility may show the temporarily hidden (minimized) panels.
  if (!prepared_for_overview_)
    return;

  // Minimizing an originally active window will activate and unminimize the
  // window upon exiting, and the item window will be "moved" to fade out in
  // 'RestoreWindow'.
  if (!item_widget_)
    return;

  WindowStateType new_type = window_state->GetStateType();
  if (chromeos::IsMinimizedWindowStateType(old_type) ==
      chromeos::IsMinimizedWindowStateType(new_type)) {
    return;
  }

  const bool minimized = transform_window_.IsMinimized();
  overview_item_view_->SetShowPreview(minimized);
  if (!minimized)
    EnsureVisible();

  // Ensures the item widget is visible. |item_widget_| opacity is set to 0.f
  // and shown at either |SetBounds| or |OnStartingAnimationComplete| based on
  // the minimized state. It's possible the minimized state changes in between
  // for ARC apps, so just force show it here.
  item_widget_->GetLayer()->SetOpacity(1.f);

  overview_grid_->PositionWindows(/*animate=*/false);
}

void OverviewItem::OnWindowCloseAnimationCompleted() {
  transform_window_.Close();
}

void OverviewItem::OnItemSpawnedAnimationCompleted() {
  UpdateRoundedCornersAndShadow();
  if (should_restack_on_animation_end_) {
    Restack();
    should_restack_on_animation_end_ = false;
  }
  OnStartingAnimationComplete();
}

void OverviewItem::OnItemBoundsAnimationStarted() {
  // Remove the shadow before animating because it may affect animation
  // performance. The shadow will be added back once the animation is completed.
  // Note that we can't use UpdateRoundedCornersAndShadow() since we don't want
  // to update the rounded corners.
  SetShadowBounds(absl::nullopt);
}

void OverviewItem::OnItemBoundsAnimationEnded() {
  // Do nothing if overview is shutting down. See crbug.com/1025267 for when it
  // might happen.
  if (!Shell::Get()->overview_controller()->InOverviewSession())
    return;

  if (overview_session_->IsShowingSavedDeskLibrary()) {
    HideForSavedDeskLibrary(false);
    return;
  }

  UpdateRoundedCornersAndShadow();
  if (should_restack_on_animation_end_) {
    Restack();
    should_restack_on_animation_end_ = false;
  }
}

void OverviewItem::PerformItemSpawnedAnimation(
    aura::Window* window,
    const gfx::Transform& target_transform) {
  DCHECK(should_use_spawn_animation_);
  should_use_spawn_animation_ = false;

  constexpr float kInitialScaler = 0.1f;
  constexpr float kTargetScaler = 1.0f;

  // Scale-up |window| and fade it in along with the |cannot_snap_widget_|'s
  // window.
  gfx::Transform initial_transform = target_transform;
  initial_transform.Scale(kInitialScaler, kInitialScaler);
  SetTransform(window, initial_transform);
  transform_window_.SetOpacity(kInitialScaler);

  ScopedOverviewTransformWindow::ScopedAnimationSettings animation_settings;
  for (auto* window_iter :
       window_util::GetVisibleTransientTreeIterator(window)) {
    auto settings = std::make_unique<ScopedOverviewAnimationSettings>(
        OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW, window_iter);
    settings->DeferPaint();
    animation_settings.push_back(std::move(settings));
  }

  if (!animation_settings.empty()) {
    animation_settings.front()->AddObserver(new AnimationObserver{
        base::BindOnce(&OverviewItem::OnItemSpawnedAnimationCompleted,
                       weak_ptr_factory_.GetWeakPtr())});
  }
  SetTransform(window, target_transform);
  transform_window_.SetOpacity(kTargetScaler);

  if (cannot_snap_widget_) {
    aura::Window* cannot_snap_window = cannot_snap_widget_->GetNativeWindow();
    cannot_snap_window->layer()->SetOpacity(kInitialScaler);
    ScopedOverviewAnimationSettings label_animation_settings(
        OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW, cannot_snap_window);
    cannot_snap_window->layer()->SetOpacity(kTargetScaler);
  }
}

void OverviewItem::SetItemBounds(const gfx::RectF& target_bounds,
                                 OverviewAnimationType animation_type,
                                 bool is_first_update) {
  aura::Window* window = GetWindow();
  DCHECK(root_window_ == window->GetRootWindow());
  // Do not set transform for drop target, set bounds instead.
  if (overview_grid_->IsDropTargetWindow(window)) {
    const gfx::Rect drop_target_bounds =
        ToStableSizeRoundedRect(chromeos::features::IsJellyrollEnabled()
                                    ? target_bounds_
                                    : GetWindowTargetBoundsWithInsets());
    SetWidgetBoundsAndMaybeAnimateTransform(
        overview_grid_->drop_target_widget(), drop_target_bounds,
        animation_type, /*observer=*/nullptr);
    return;
  }

  gfx::RectF screen_rect = gfx::RectF(GetTargetBoundsInScreen());

  // Avoid division by zero by ensuring screen bounds is not empty.
  gfx::SizeF screen_size(screen_rect.size());
  screen_size.SetToMax(gfx::SizeF(1.f, 1.f));
  screen_rect.set_size(screen_size);

  const int top_view_inset = transform_window_.GetTopInset();
  gfx::RectF transformed_bounds = target_bounds;

  // Update |transformed_bounds| to match the unclipped size of the window, so
  // we transform the window to the correct size.
  if (unclipped_size_)
    transformed_bounds.set_size(gfx::SizeF(*unclipped_size_));

  gfx::RectF overview_item_bounds =
      transform_window_.ShrinkRectToFitPreservingAspectRatio(
          screen_rect, transformed_bounds, top_view_inset, kHeaderHeightDp);

  if (chromeos::features::IsJellyrollEnabled()) {
    // Adjust the `overview_item_bounds` x position and width if the window has
    // normal or letter dimensions type to make sure it's aligned with overview
    // item header view after the transform.
    if (transform_window_.type() == OverviewGridWindowFillMode::kNormal ||
        transform_window_.type() == OverviewGridWindowFillMode::kLetterBoxed) {
      overview_item_bounds.set_x(transformed_bounds.x());
      overview_item_bounds.set_width(transformed_bounds.width());
    }

    // Adjust the `overview_item_bounds` y position and height if the window has
    // normal or pillar dimensions type to make sure there's no gap between the
    // header and the window and no empty space at the end of the overview item
    // container.
    if (transform_window_.type() == OverviewGridWindowFillMode::kNormal ||
        transform_window_.type() == OverviewGridWindowFillMode::kPillarBoxed) {
      // The window top bar's target height with the transform.
      float window_top_inset_target_height =
          target_bounds.height() / screen_rect.height() * top_view_inset;
      overview_item_bounds.set_y(
          overview_item_view_->header_view()->GetBoundsInScreen().bottom() -
          window_top_inset_target_height);
      overview_item_bounds.set_height(target_bounds.height() - kHeaderHeightDp +
                                      window_top_inset_target_height);
    }
  }

  const gfx::Transform transform =
      gfx::TransformBetweenRects(screen_rect, overview_item_bounds);

  // Determine the amount of clipping we should put on the window. Note that the
  // clipping goes after setting a transform, as layer transform affects layer
  // clip.
  using ClippingType = ScopedOverviewTransformWindow::ClippingType;
  ScopedOverviewTransformWindow::ClippingData clipping_data{
      ClippingType::kCustom, gfx::SizeF()};
  if (unclipped_size_)
    clipping_data.second = GetWindowTargetBoundsWithInsets().size();
  else if (is_first_update)
    clipping_data.first = ClippingType::kEnter;

  if (is_first_update &&
      animation_type == OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW) {
    PerformItemSpawnedAnimation(window, transform);
    transform_window_.SetClipping(clipping_data);
    return;
  }

  ScopedOverviewTransformWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  if (animation_type == OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW &&
      !animation_settings.empty() && !GetWindow()->is_destroying()) {
    animation_settings.front()->AddObserver(new AnimationObserver{
        base::BindOnce(&OverviewItem::OnItemBoundsAnimationStarted,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&OverviewItem::OnItemBoundsAnimationEnded,
                       weak_ptr_factory_.GetWeakPtr())});
  }
  SetTransform(window, transform);
  transform_window_.SetClipping(clipping_data);
}

void OverviewItem::CreateItemWidget() {
  TRACE_EVENT0("ui", "OverviewItem::CreateItemWidget");

  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.visible_on_all_workspaces = true;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.name = "OverviewItemWidget";
  params.activatable = views::Widget::InitParams::Activatable::kDefault;
  params.accept_events = true;
  params.parent = transform_window_.window()->parent();
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);

  item_widget_ = std::make_unique<views::Widget>();
  item_widget_->set_focus_on_creation(false);
  item_widget_->Init(std::move(params));
  aura::Window* widget_window = item_widget_->GetNativeWindow();
  widget_window->parent()->StackChildBelow(widget_window, GetWindow());

  shadow_ = SystemShadow::CreateShadowOnNinePatchLayer(kDefaultShadowType);
  auto* shadow_layer = shadow_->GetLayer();
  auto* widget_layer = item_widget_->GetLayer();
  widget_layer->Add(shadow_layer);
  widget_layer->StackAtBottom(shadow_layer);
  shadow_->ObserveColorProviderSource(item_widget_.get());

  overview_item_view_ =
      item_widget_->SetContentsView(std::make_unique<OverviewItemView>(
          this,
          base::BindRepeating(&OverviewItem::CloseButtonPressed,
                              base::Unretained(this)),
          GetWindow(), transform_window_.IsMinimized()));
  item_widget_->Show();
  item_widget_->SetOpacity(
      overview_session_ && overview_session_->ShouldEnterWithoutAnimations()
          ? 1.f
          : 0.f);
  widget_layer->SetMasksToBounds(false);
}

void OverviewItem::UpdateHeaderLayout(OverviewAnimationType animation_type) {
  if (chromeos::features::IsJellyrollEnabled()) {
    UpdateHeaderLayoutCrOSNext(animation_type);
    return;
  }

  aura::Window* widget_window = item_widget_->GetNativeWindow();
  ScopedOverviewAnimationSettings animation_settings(animation_type,
                                                     widget_window);
  // Create a start animation observer if this is an enter overview layout
  // animation.
  if (animation_type == OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER ||
      animation_type == OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER) {
    auto enter_observer = std::make_unique<EnterAnimationObserver>();
    animation_settings.AddObserver(enter_observer.get());
    Shell::Get()->overview_controller()->AddEnterAnimationObserver(
        std::move(enter_observer));
  }

  gfx::RectF item_bounds = target_bounds_;
  ::wm::TranslateRectFromScreen(root_window_, &item_bounds);
  const gfx::Point origin = gfx::ToRoundedPoint(item_bounds.origin());
  item_bounds.set_origin(gfx::PointF());
  widget_window->SetBounds(ToStableSizeRoundedRect(item_bounds));

  gfx::Transform label_transform;
  label_transform.Translate(origin.x(), origin.y());
  widget_window->SetTransform(label_transform);
}

void OverviewItem::UpdateHeaderLayoutCrOSNext(
    OverviewAnimationType animation_type) {
  gfx::RectF current_item_bounds(item_widget_->GetWindowBoundsInScreen());
  gfx::RectF target_item_bounds = target_bounds_;

  wm::TranslateRectFromScreen(root_window_, &current_item_bounds);
  wm::TranslateRectFromScreen(root_window_, &target_item_bounds);

  aura::Window* widget_window = item_widget_->GetNativeWindow();
  if (current_item_bounds.IsEmpty()) {
    widget_window->SetBounds(ToStableSizeRoundedRect(target_item_bounds));
    return;
  }

  const gfx::Transform item_bounds_transform =
      gfx::TransformBetweenRects(target_item_bounds, current_item_bounds);
  widget_window->SetBounds(ToStableSizeRoundedRect(target_item_bounds));
  widget_window->SetTransform(item_bounds_transform);

  ScopedOverviewAnimationSettings item_animation_settings(animation_type,
                                                          widget_window);
  // Create a start animation observer if this is an enter overview layout
  // animation.
  if (animation_type == OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER ||
      animation_type == OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER) {
    auto enter_observer = std::make_unique<EnterAnimationObserver>();
    item_animation_settings.AddObserver(enter_observer.get());
    Shell::Get()->overview_controller()->AddEnterAnimationObserver(
        std::move(enter_observer));
  }
  widget_window->SetTransform(gfx::Transform());

  // Since header view is a child of the overview item view, the bounds
  // animation is appled to the header as well when it's applied to the overview
  // item. However, when calculating the target bounds for the window, it's
  // always assumed that the header's height is 40, there's a gap between the
  // header and the window during the animation. In order to neutralize the gap,
  // apply the reversed vertical transform to the header separately.
  ui::Layer* header_layer = overview_item_view_->header_view()->layer();
  float vertical_scale = item_bounds_transform.To2dScale().y();
  gfx::Transform vertical_reverse_transform =
      gfx::Transform::MakeScale(1.f, 1.f / vertical_scale);
  header_layer->SetTransform(vertical_reverse_transform);
  ScopedOverviewAnimationSettings header_animation_settings(
      animation_type, header_layer->GetAnimator());
  header_layer->SetTransform(gfx::Transform());
}

OverviewAnimationType
OverviewItem::GetExitOverviewAnimationTypeForMinimizedWindow(
    OverviewEnterExitType type) {
  // We should never get here when overview mode should exit immediately. The
  // minimized window's `item_widget_` should be closed and destroyed
  // immediately.
  DCHECK_NE(type, OverviewEnterExitType::kImmediateExit);

  // If the managed window has been hidden by the saved desk library, then
  // we must avoid animating a minimized window. See http://b/260001863.
  if (ScopedOverviewHideWindows* hide_windows =
          overview_session_->hide_windows_for_saved_desks_grid()) {
    if (hide_windows->HasWindow(item_widget_->GetNativeWindow())) {
      return OVERVIEW_ANIMATION_NONE;
    }
  }

  // OverviewEnterExitType can only be set to `kWindowMinimized` in tablet mode.
  // Fade out the minimized window without animation if switch from tablet mode
  // to clamshell mode.
  if (type == OverviewEnterExitType::kFadeOutExit) {
    return Shell::Get()->tablet_mode_controller()->InTabletMode()
               ? OVERVIEW_ANIMATION_EXIT_TO_HOME_LAUNCHER
               : OVERVIEW_ANIMATION_NONE;
  }
  return should_animate_when_exiting_
             ? OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_FADE_OUT
             : OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO;
}

void OverviewItem::AnimateOpacity(float opacity,
                                  OverviewAnimationType animation_type) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);
  ScopedOverviewTransformWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  transform_window_.SetOpacity(opacity);

  ScopedOverviewAnimationSettings scoped_animation_settings(
      animation_type, item_widget_->GetNativeWindow());
  item_widget_->SetOpacity(opacity);

  if (cannot_snap_widget_) {
    aura::Window* cannot_snap_widget_window =
        cannot_snap_widget_->GetNativeWindow();
    ScopedOverviewAnimationSettings scoped_animation_settings_2(
        animation_type, cannot_snap_widget_window);
    cannot_snap_widget_window->layer()->SetOpacity(opacity);
  }
}

void OverviewItem::StartDrag() {
  // Stack the window and the widget window at the top. This is to ensure that
  // they appear above other app windows, as well as above the desks bar. Note
  // that the stacking operations are done in this order to make sure that that
  // the window appears above the widget window.
  if (aura::Window* widget_window = item_widget_->GetNativeWindow())
    widget_window->parent()->StackChildAtTop(widget_window);

  aura::Window* window = GetWindow();
  window->parent()->StackChildAtTop(window);
}

void OverviewItem::CloseButtonPressed() {
  base::RecordAction(
      base::UserMetricsAction("WindowSelector_OverviewCloseButton"));
  if (Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    base::RecordAction(
        base::UserMetricsAction("Tablet_WindowCloseFromOverviewButton"));
  }
  CloseWindow();
}

void OverviewItem::HandlePressEvent(const gfx::PointF& location_in_screen,
                                    bool from_touch_gesture) {
  // No need to start the drag again if already in a drag. This can happen if we
  // switch fingers midway through a drag.
  if (IsDragItem())
    return;

  StartDrag();
  overview_session_->InitiateDrag(this, location_in_screen,
                                  /*is_touch_dragging=*/from_touch_gesture);
}

void OverviewItem::HandleReleaseEvent(const gfx::PointF& location_in_screen) {
  if (!IsDragItem())
    return;

  overview_session_->CompleteDrag(this, location_in_screen);
}

void OverviewItem::HandleDragEvent(const gfx::PointF& location_in_screen) {
  if (!IsDragItem())
    return;

  overview_session_->Drag(this, location_in_screen);
}

void OverviewItem::HandleLongPressEvent(const gfx::PointF& location_in_screen) {
  if (!IsDragItem())
    return;

  if (ShouldAllowSplitView() || (desks_util::ShouldDesksBarBeCreated() &&
                                 overview_grid_->IsDesksBarViewActive())) {
    overview_session_->StartNormalDragMode(location_in_screen);
  }
}

void OverviewItem::HandleFlingStartEvent(const gfx::PointF& location_in_screen,
                                         float velocity_x,
                                         float velocity_y) {
  overview_session_->Fling(this, location_in_screen, velocity_x, velocity_y);
}

void OverviewItem::HandleTapEvent() {
  if (!IsDragItem())
    return;

  overview_session_->ActivateDraggedWindow();
}

void OverviewItem::HandleGestureEndEvent() {
  if (!IsDragItem())
    return;

  // Gesture end events come from a long press getting canceled. Long press
  // alters the stacking order, so on gesture end, make sure we restore the
  // stacking order on the next reposition.
  set_should_restack_on_animation_end(true);
  overview_session_->ResetDraggedWindowGesture();
}

aura::Window::Windows OverviewItem::GetWindowsForHomeGesture() {
  aura::Window::Windows windows = {item_widget_->GetNativeWindow()};
  if (!transform_window_.IsMinimized()) {
    for (auto* window : GetTransientTreeIterator(GetWindow()))
      windows.push_back(window);
  }
  if (cannot_snap_widget_)
    windows.push_back(cannot_snap_widget_->GetNativeWindow());
  return windows;
}

void OverviewItem::HideWindowInOverview() {
  ScopedOverviewHideWindows* hide_windows =
      overview_session_->hide_windows_for_saved_desks_grid();
  DCHECK(hide_windows);

  // Hide the overview item window.
  if (item_widget_ && !hide_windows->HasWindow(item_widget_->GetNativeWindow()))
    hide_windows->AddWindow(item_widget_->GetNativeWindow());
}

void OverviewItem::ShowWindowInOverview() {
  ScopedOverviewHideWindows* hide_windows =
      overview_session_->hide_windows_for_saved_desks_grid();
  DCHECK(hide_windows);

  // Show the overview item window.
  if (item_widget_ &&
      hide_windows->HasWindow(item_widget_->GetNativeWindow())) {
    hide_windows->RemoveWindow(item_widget_->GetNativeWindow(),
                               /*show_window=*/true);
  }
}

}  // namespace ash
