// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_item.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desks_util.h"
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
#include "ash/wm/overview/rounded_label_widget.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/overview/scoped_overview_transform_window.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/user_metrics.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Opacity for fading out during closing a window.
constexpr float kClosingItemOpacity = 0.8f;

// Before closing a window animate both the window and the caption to shrink by
// this fraction of size.
constexpr float kPreCloseScale = 0.02f;

constexpr int kShadowElevation = 16;

// The amount of translation an item animates by when it is closed by using
// swipe to close.
constexpr int kSwipeToCloseCloseTranslationDp = 96;

// When an item is being dragged, the bounds are outset horizontally by this
// fraction of the width, and vertically by this fraction of the height. The
// outset in each dimension is on both sides, for a total of twice this much
// change in the size of the item along that dimension.
constexpr float kDragWindowScale = 0.05f;

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

  DISALLOW_COPY_AND_ASSIGN(AnimationObserver);
};

OverviewAnimationType GetExitOverviewAnimationTypeForMinimizedWindow(
    OverviewEnterExitType type,
    bool should_animate_when_exiting) {
  // We should never get here when overview mode should exit immediately. The
  // minimized window's |item_widget_| should be closed and destroyed
  // immediately.
  DCHECK_NE(type, OverviewEnterExitType::kImmediateExit);

  // OverviewEnterExitType can only be set to kWindowMinimized in talbet mode.
  // Fade out the minimized window without animation if switch from tablet mode
  // to clamshell mode.
  if (type == OverviewEnterExitType::kSlideOutExit ||
      type == OverviewEnterExitType::kFadeOutExit) {
    return Shell::Get()->tablet_mode_controller()->InTabletMode()
               ? OVERVIEW_ANIMATION_EXIT_TO_HOME_LAUNCHER
               : OVERVIEW_ANIMATION_NONE;
  }
  return should_animate_when_exiting
             ? OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_FADE_OUT
             : OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO;
}

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
      overview_grid_(overview_grid) {
  CreateItemWidget();
  for (auto* window_iter : WindowTransientDescendantIteratorRange(
           WindowTransientDescendantIterator(window))) {
    window_iter->AddObserver(this);
  }
  WindowState::Get(window)->AddObserver(this);
}

OverviewItem::~OverviewItem() {
  aura::Window* window = GetWindow();
  WindowState::Get(window)->RemoveObserver(this);
  for (auto* window_iter : WindowTransientDescendantIteratorRange(
           WindowTransientDescendantIterator(window))) {
    window_iter->RemoveObserver(this);
  }
}

aura::Window* OverviewItem::GetWindow() {
  return transform_window_.window();
}

bool OverviewItem::Contains(const aura::Window* target) const {
  return transform_window_.Contains(target);
}

void OverviewItem::OnMovingWindowToAnotherDesk() {
  is_moving_to_another_desk_ = true;
  // Restore the dragged item window, so that its transform is reset to
  // identity.
  RestoreWindow(/*reset_transform=*/true);
}

void OverviewItem::RestoreWindow(bool reset_transform) {
  // TODO(oshima): SplitViewController has its own logic to adjust the
  // target state in |SplitViewController::OnOverviewModeEnding|.
  // Unify the mechanism to control it and remove ifs.
  if (Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      !SplitViewController::Get(root_window_)->InSplitViewMode() &&
      reset_transform) {
    MaximizeIfSnapped(GetWindow());
  }

  overview_item_view_->OnOverviewItemWindowRestoring();
  transform_window_.RestoreWindow(reset_transform);

  if (transform_window_.IsMinimized()) {
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
        GetExitOverviewAnimationTypeForMinimizedWindow(
            enter_exit_type, should_animate_when_exiting_);
    FadeOutWidgetAndMaybeSlideOnExit(
        std::move(item_widget_), animation_type,
        animation_type == OVERVIEW_ANIMATION_EXIT_TO_HOME_LAUNCHER &&
            enter_exit_type == OverviewEnterExitType::kSlideOutExit);
  }
}

void OverviewItem::EnsureVisible() {
  transform_window_.EnsureVisible();
}

void OverviewItem::Shutdown() {
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

gfx::RectF OverviewItem::GetTargetBoundsInScreen() const {
  return ::ash::GetTargetBoundsInScreen(transform_window_.window());
}

gfx::RectF OverviewItem::GetTransformedBounds() const {
  return transform_window_.GetTransformedBounds();
}

void OverviewItem::SetBounds(const gfx::RectF& target_bounds,
                             OverviewAnimationType animation_type) {
  if (in_bounds_update_ ||
      !Shell::Get()->overview_controller()->InOverviewSession()) {
    return;
  }

  // Do not animate if the resulting bounds does not change. The original
  // window may change bounds so we still need to call SetItemBounds to update
  // the window transform.
  OverviewAnimationType new_animation_type = animation_type;
  if (target_bounds == target_bounds_ &&
      !GetWindow()->layer()->GetAnimator()->is_animating()) {
    new_animation_type = OVERVIEW_ANIMATION_NONE;
  }

  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  // If |target_bounds_| is empty, this is the first update. Let
  // UpdateHeaderLayout know, as we do not want |item_widget_| to be animated
  // with the window.
  const bool is_first_update = target_bounds_.IsEmpty();
  target_bounds_ = target_bounds;

  // If the window is minimized we can avoid applying transforms on the original
  // window.
  if (transform_window_.IsMinimized()) {
    item_widget_->GetNativeWindow()->layer()->GetAnimator()->StopAnimating();

    gfx::Rect minimized_bounds = ToStableSizeRoundedRect(target_bounds);
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
                                        &OverviewItem::
                                            OnItemBoundsAnimationEnded,
                                        weak_ptr_factory_.GetWeakPtr())}
            : nullptr);

    // Minimized windows have a WindowPreviewView which mirrors content from the
    // window. |target_bounds| may not have a matching aspect ratio to the
    // actual window (eg. in splitview overview). In this case, the contents
    // will be squashed to fit the given bounds. To get around this, stretch out
    // the contents so that it matches |unclipped_size_|, then clip the layer to
    // match |target_bounds|. This is what is done on non-minimized windows.
    ui::Layer* preview_layer = overview_item_view_->preview_view()->layer();
    DCHECK(preview_layer);
    if (unclipped_size_) {
      gfx::SizeF target_size(*unclipped_size_);
      gfx::SizeF preview_size = GetWindowTargetBoundsWithInsets().size();
      target_size.Enlarge(0, -kHeaderHeightDp);

      const float x_scale = target_size.width() / preview_size.width();
      const float y_scale = target_size.height() / preview_size.height();
      gfx::Transform transform;
      transform.Scale(x_scale, y_scale);
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

    // On the first update show |item_widget_|. It's created on creation of
    // |this|, and needs to be shown as soon as its bounds have been determined
    // as it contains a mirror view of the window in its contents. The header
    // will be faded in later to match non minimized windows.
    if (is_first_update) {
      if (!should_animate_when_entering_) {
        item_widget_->GetNativeWindow()->layer()->SetOpacity(1.f);
      } else {
        if (new_animation_type == OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW) {
          PerformItemSpawnedAnimation(item_widget_->GetNativeWindow(),
                                      gfx::Transform{});
        } else {
          // Items that are slide in already have their slide in animations
          // handled in |SlideWindowIn|.
          const bool slide_in = overview_session_->enter_exit_overview_type() ==
                                OverviewEnterExitType::kSlideInEnter;
          if (!slide_in) {
            // If entering from home launcher, use the home specific (fade)
            // animation.
            OverviewAnimationType fade_animation =
                animation_type == OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER
                    ? animation_type
                    : OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN;

            FadeInWidgetAndMaybeSlideOnEnter(item_widget_.get(), fade_animation,
                                             /*slide=*/false,
                                             /*observe=*/true);
          }

          // Update the item header visibility immediately if entering from home
          // launcher.
          if (new_animation_type ==
              OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER) {
            overview_item_view_->SetHeaderVisibility(
                OverviewItemView::HeaderVisibility::kVisible);
          }
        }
      }
    }
  } else {
    gfx::RectF inset_bounds(target_bounds);
    SetItemBounds(inset_bounds, new_animation_type, is_first_update);
    UpdateHeaderLayout(is_first_update ? OVERVIEW_ANIMATION_NONE
                                       : new_animation_type);
  }

  // Shadow is normally set after an animation is finished. In the case of no
  // animations, manually set the shadow. Shadow relies on both the window
  // transform and |item_widget_|'s new bounds so set it after SetItemBounds
  // and UpdateHeaderLayout. Do not apply the shadow for drop target.
  if (new_animation_type == OVERVIEW_ANIMATION_NONE)
    UpdateRoundedCornersAndShadow();

  if (cannot_snap_widget_) {
    SetWidgetBoundsAndMaybeAnimateTransform(
        cannot_snap_widget_.get(),
        cannot_snap_widget_->GetBoundsCenteredIn(
            ToStableSizeRoundedRect(GetWindowTargetBoundsWithInsets())),
        new_animation_type, nullptr);
  }

  translation_y_map_.clear();
  aura::Window::Windows windows = GetWindowsForHomeGesture();
  for (auto* window : windows) {
    // Cache the original y translation when setting bounds. They will be
    // possibly used later when swiping up from the shelf to close overview. Use
    // the target transform as some windows may still be animating.
    translation_y_map_[window] =
        window->layer()->GetTargetTransform().To2dTranslation().y();
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
    original_transform.ConcatTransform(transform);
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
  SetShadowBounds(base::nullopt);

  gfx::RectF inset_bounds(target_bounds_);
  inset_bounds.Inset(target_bounds_.width() * kPreCloseScale,
                     target_bounds_.height() * kPreCloseScale);
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

void OverviewItem::UpdateCannotSnapWarningVisibility() {
  // Windows which can snap will never show this warning. Or if the window is
  // the drop target window, also do not show this warning.
  bool visible = true;
  if (SplitViewController::Get(root_window_)->CanSnapWindow(GetWindow()) ||
      overview_grid_->IsDropTargetWindow(GetWindow())) {
    visible = false;
  } else {
    const SplitViewController::State state =
        SplitViewController::Get(root_window_)->state();
    visible = state == SplitViewController::State::kLeftSnapped ||
              state == SplitViewController::State::kRightSnapped;
  }

  if (!visible && !cannot_snap_widget_)
    return;

  if (!cannot_snap_widget_) {
    RoundedLabelWidget::InitParams params;
    params.horizontal_padding = kSplitviewLabelHorizontalInsetDp;
    params.vertical_padding = kSplitviewLabelVerticalInsetDp;
    params.background_color = kSplitviewLabelBackgroundColor;
    params.foreground_color = kSplitviewLabelEnabledColor;
    params.rounding_dp = kSplitviewLabelRoundRectRadiusDp;
    params.preferred_height = kSplitviewLabelPreferredHeightDp;
    params.message_id = IDS_ASH_SPLIT_VIEW_CANNOT_SNAP;
    params.parent = GetWindow()->parent();
    params.hide_in_mini_view = true;
    cannot_snap_widget_ = std::make_unique<RoundedLabelWidget>();
    cannot_snap_widget_->Init(std::move(params));
    GetWindow()->parent()->StackChildAbove(
        cannot_snap_widget_->GetNativeWindow(), GetWindow());
  }

  DoSplitviewOpacityAnimation(cannot_snap_widget_->GetNativeWindow()->layer(),
                              visible
                                  ? SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_IN
                                  : SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_OUT);
  const gfx::Rect bounds =
      ToStableSizeRoundedRect(GetWindowTargetBoundsWithInsets());
  cannot_snap_widget_->SetBoundsCenteredIn(bounds, /*animate=*/false);
}

void OverviewItem::HideCannotSnapWarning() {
  if (!cannot_snap_widget_)
    return;
  DoSplitviewOpacityAnimation(cannot_snap_widget_->GetNativeWindow()->layer(),
                              SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_OUT);
}

void OverviewItem::OnSelectorItemDragStarted(OverviewItem* item) {
  is_being_dragged_ = (item == this);
  overview_item_view_->SetHeaderVisibility(
      is_being_dragged_
          ? OverviewItemView::HeaderVisibility::kInvisible
          : OverviewItemView::HeaderVisibility::kCloseButtonInvisibleOnly);
}

void OverviewItem::OnSelectorItemDragEnded(bool snap) {
  if (snap) {
    if (!is_being_dragged_)
      overview_item_view_->HideCloseInstantlyAndThenShowItSlowly();
  } else {
    overview_item_view_->SetHeaderVisibility(
        OverviewItemView::HeaderVisibility::kVisible);
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

gfx::Rect OverviewItem::GetBoundsOfSelectedItem() {
  gfx::RectF original_bounds = target_bounds();
  ScaleUpSelectedItem(OVERVIEW_ANIMATION_NONE);
  gfx::RectF selected_bounds = transform_window_.GetTransformedBounds();
  SetBounds(original_bounds, OVERVIEW_ANIMATION_NONE);
  return ToStableSizeRoundedRect(selected_bounds);
}

void OverviewItem::ScaleUpSelectedItem(OverviewAnimationType animation_type) {
  gfx::RectF scaled_bounds = target_bounds();
  scaled_bounds.Inset(-scaled_bounds.width() * kDragWindowScale,
                      -scaled_bounds.height() * kDragWindowScale);
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

void OverviewItem::SlideWindowIn() {
  // This only gets called if we see the home launcher on enter (all windows are
  // minimized).
  DCHECK(transform_window_.IsMinimized());

  // The mask and shadow will be shown when animation ends. Update the mask
  // after starting the animation since starting the animation lets the
  // controller know we are in starting animation.
  FadeInWidgetAndMaybeSlideOnEnter(item_widget_.get(),
                                   OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER,
                                   /*slide=*/true, /*observe=*/true);
  UpdateRoundedCornersAndShadow();
}

std::unique_ptr<ui::ScopedLayerAnimationSettings>
OverviewItem::UpdateYPositionAndOpacity(
    float new_grid_y,
    float opacity,
    OverviewSession::UpdateAnimationSettingsCallback callback) {
  aura::Window::Windows windows = GetWindowsForHomeGesture();
  std::unique_ptr<ui::ScopedLayerAnimationSettings> settings_to_observe;
  for (auto* window : windows) {
    ui::Layer* layer = window->layer();
    std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
    if (!callback.is_null()) {
      settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
          layer->GetAnimator());
      callback.Run(settings.get());
    }
    layer->SetOpacity(opacity);

    float initial_y = 0.f;
    if (translation_y_map_.contains(window))
      initial_y = translation_y_map_[window];

    // Alter the y-translation. Offset by the window location relative to the
    // grid.
    gfx::Transform transform = layer->transform();
    transform.matrix().setFloat(1, 3, initial_y - new_grid_y);
    layer->SetTransform(transform);

    if (settings)
      settings_to_observe = std::move(settings);
  }

  return settings_to_observe;
}

void OverviewItem::UpdateItemContentViewForMinimizedWindow() {
  overview_item_view_->RefreshPreviewView();
}

bool OverviewItem::IsDragItem() {
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

void OverviewItem::UpdatePhantomsForDragging(bool is_touch_dragging) {
  DCHECK(AreMultiDisplayOverviewAndSplitViewEnabled());
  DCHECK_GT(Shell::GetAllRootWindows().size(), 1u);
  if (!phantoms_for_dragging_) {
    phantoms_for_dragging_ =
        transform_window_.IsMinimized()
            ? std::make_unique<DragWindowController>(
                  item_widget_->GetNativeWindow(), is_touch_dragging,
                  base::make_optional(shadow_->content_bounds()))
            : std::make_unique<DragWindowController>(GetWindow(),
                                                     is_touch_dragging);
  }
  phantoms_for_dragging_->Update();
}

void OverviewItem::DestroyPhantomsForDragging() {
  DCHECK(AreMultiDisplayOverviewAndSplitViewEnabled());
  phantoms_for_dragging_.reset();
}

void OverviewItem::SetShadowBounds(
    base::Optional<gfx::RectF> bounds_in_screen) {
  // Shadow is normally turned off during animations and reapplied when they
  // are finished. On destruction, |shadow_| is cleaned up before
  // |transform_window_|, which may call this function, so early exit if
  // |shadow_| is nullptr.
  if (!shadow_)
    return;

  if (!bounds_in_screen) {
    shadow_->layer()->SetVisible(false);
    return;
  }

  shadow_->layer()->SetVisible(true);
  gfx::Rect bounds_in_item =
      gfx::Rect(item_widget_->GetNativeWindow()->GetTargetBounds().size());
  bounds_in_item.Inset(0, kHeaderHeightDp, 0, 0);
  bounds_in_item.ClampToCenteredSize(
      gfx::ToRoundedSize(bounds_in_screen->size()));
  shadow_->SetContentBounds(bounds_in_item);
}

void OverviewItem::UpdateRoundedCornersAndShadow() {
  // Do not show the rounded corners and the shadow if overview is shutting
  // down or we're currently in entering overview animation. Also don't update
  // or animate the window's frame header clip under these conditions.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  const bool is_shutting_down =
      !overview_controller || !overview_controller->InOverviewSession();
  const bool should_show_rounded_corners =
      !disable_mask_ && !is_shutting_down &&
      !overview_controller->IsInStartAnimation();

  if (transform_window_.IsMinimized()) {
    overview_item_view_->UpdatePreviewRoundedCorners(
        should_show_rounded_corners);
  } else {
    transform_window_.UpdateRoundedCorners(should_show_rounded_corners);
  }

  // In addition, the shadow should be hidden if
  // 1) this overview item is the drop target window or
  // 2) this overview item is in animation.
  const bool should_show_shadow =
      should_show_rounded_corners &&
      !overview_grid_->IsDropTargetWindow(GetWindow()) &&
      !transform_window_.GetOverviewWindow()
           ->layer()
           ->GetAnimator()
           ->is_animating();
  if (should_show_shadow) {
    // The shadow should match the size of the transformed window or preview
    // window if unclipped. If clipped, the shadow should match the size of the
    // item minus the border and header.
    const gfx::RectF shadow_bounds = unclipped_size_
                                         ? GetWindowTargetBoundsWithInsets()
                                         : GetUnclippedShadowBounds();
    SetShadowBounds(base::make_optional(shadow_bounds));
  } else {
    SetShadowBounds(base::nullopt);
  }
}

void OverviewItem::OnStartingAnimationComplete() {
  DCHECK(item_widget_);
  if (transform_window_.IsMinimized()) {
    // Fade the title in if minimized. The rest of |item_widget_| should
    // already be shown.
    overview_item_view_->SetHeaderVisibility(
        OverviewItemView::HeaderVisibility::kVisible);
  } else {
    FadeInWidgetAndMaybeSlideOnEnter(
        item_widget_.get(), OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
        /*slide=*/false, /*observe=*/false);
  }
  const bool show_backdrop =
      GetWindowDimensionsType() != OverviewGridWindowFillMode::kNormal;
  overview_item_view_->SetBackdropVisibility(show_backdrop);
  UpdateCannotSnapWarningVisibility();
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

OverviewAnimationType OverviewItem::GetExitOverviewAnimationType() {
  if (overview_session_->enter_exit_overview_type() ==
      OverviewEnterExitType::kImmediateExit) {
    return OVERVIEW_ANIMATION_NONE;
  }

  return should_animate_when_exiting_
             ? OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_EXIT
             : OVERVIEW_ANIMATION_NONE;
}

OverviewAnimationType OverviewItem::GetExitTransformAnimationType() {
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
  if (!overview_session_->CanProcessEvent(this, /*from_touch_gesture=*/false))
    return;

  const gfx::PointF screen_location = event.target()->GetScreenLocationF(event);
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

bool OverviewItem::ShouldIgnoreGestureEvents() {
  return IsSlidingOutOverviewFromShelf();
}

void OverviewItem::OnHighlightedViewActivated() {
  overview_session_->OnHighlightedItemActivated(this);
}

void OverviewItem::OnHighlightedViewClosed() {
  overview_session_->OnHighlightedItemClosed(this);
}

void OverviewItem::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  DCHECK_EQ(sender, overview_item_view_->close_button());
  if (IsSlidingOutOverviewFromShelf())
    return;

  base::RecordAction(
      base::UserMetricsAction("WindowSelector_OverviewCloseButton"));
  if (Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    base::RecordAction(
        base::UserMetricsAction("Tablet_WindowCloseFromOverviewButton"));
  }
  CloseWindow();
}

void OverviewItem::OnWindowPropertyChanged(aura::Window* window,
                                           const void* key,
                                           intptr_t old) {
  if (prepared_for_overview_ && window == GetWindow() &&
      key == aura::client::kTopViewInset &&
      window->GetProperty(aura::client::kTopViewInset) !=
          static_cast<int>(old)) {
    overview_grid_->PositionWindows(/*animate=*/false);
  }
}

void OverviewItem::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds,
                                         ui::PropertyChangeReason reason) {
  // During preparation, window bounds can change. Ignore bounds change
  // notifications in this case; we'll reposition soon.
  if (!prepared_for_overview_)
    return;

  // Do not keep the overview bounds if we're shutting down.
  if (!Shell::Get()->overview_controller()->InOverviewSession())
    return;

  // The drop target will get its bounds set as opposed to its transform
  // set in |SetItemBounds| so do not position windows again when that
  // particular window has its bounds changed.
  aura::Window* main_window = GetWindow();
  if (overview_grid_->IsDropTargetWindow(main_window))
    return;

  if (reason == ui::PropertyChangeReason::NOT_FROM_ANIMATION) {
    if (window == main_window) {
      overview_item_view_->RefreshPreviewView();
    } else {
      // Transient window is repositioned. The new position within the
      // overview item needs to be recomputed. No need to recompute if the
      // transient is invisible. It will get placed properly when it reshows on
      // overview end.
      if (window->IsVisible())
        SetBounds(target_bounds_, OVERVIEW_ANIMATION_NONE);
    }
  }

  if (window != main_window)
    return;

  // Immediately finish any active bounds animation.
  window->layer()->GetAnimator()->StopAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS);
  UpdateWindowDimensionsType();
  overview_grid_->PositionWindows(/*animate=*/false);
}

void OverviewItem::OnWindowDestroying(aura::Window* window) {
  // Stops observing the window and all of its transient descendents.
  for (auto* window_iter : WindowTransientDescendantIteratorRange(
           WindowTransientDescendantIterator(window))) {
    window_iter->RemoveObserver(this);
  }

  if (window != GetWindow())
    return;

  if (is_being_dragged_) {
    DCHECK_EQ(this, overview_session_->window_drag_controller()->item());
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

  // When swiping away overview mode via shelf, windows will get minimized, but
  // we do not want show a mirrored view in this case.
  if (overview_session_->enter_exit_overview_type() ==
      OverviewEnterExitType::kSwipeFromShelf) {
    return;
  }

  WindowStateType new_type = window_state->GetStateType();
  if (IsMinimizedWindowStateType(old_type) ==
      IsMinimizedWindowStateType(new_type)) {
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

gfx::Rect OverviewItem::GetShadowBoundsForTesting() {
  if (!shadow_ || !shadow_->layer()->visible())
    return gfx::Rect();

  return shadow_->content_bounds();
}

gfx::RectF OverviewItem::GetWindowTargetBoundsWithInsets() const {
  gfx::RectF window_target_bounds = target_bounds_;
  window_target_bounds.Inset(kWindowMargin, kWindowMargin);
  window_target_bounds.Inset(0, kHeaderHeightDp, 0, 0);
  return window_target_bounds;
}

gfx::RectF OverviewItem::GetUnclippedShadowBounds() const {
  return transform_window_.IsMinimized()
             ? gfx::RectF(
                   overview_item_view_->preview_view()->GetBoundsInScreen())
             : transform_window_.GetTransformedBounds();
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
  SetShadowBounds(base::nullopt);
}

void OverviewItem::OnItemBoundsAnimationEnded() {
  // Do nothing if overview is shutting down. See crbug.com/1025267 for when it
  // might happen.
  if (!Shell::Get()->overview_controller()->InOverviewSession())
    return;

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
        ToStableSizeRoundedRect(GetWindowTargetBoundsWithInsets());
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

  // |target_bounds| are the bounds of the |item_widget|, which include a
  // border.
  transformed_bounds.Inset(kWindowMargin, kWindowMargin);

  // Update |transformed_bounds| to match the unclipped size of the window, so
  // we transform the window to the correct size.
  if (unclipped_size_)
    transformed_bounds.set_size(gfx::SizeF(*unclipped_size_));

  gfx::RectF overview_item_bounds =
      transform_window_.ShrinkRectToFitPreservingAspectRatio(
          screen_rect, transformed_bounds, top_view_inset, kHeaderHeightDp);

  const gfx::Transform transform =
      gfx::TransformBetweenRects(screen_rect, overview_item_bounds);

  if (is_first_update &&
      animation_type == OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW) {
    PerformItemSpawnedAnimation(window, transform);
    return;
  }

  ScopedOverviewTransformWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  if (animation_type == OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW &&
      !animation_settings.empty()) {
    animation_settings.front()->AddObserver(new AnimationObserver{
        base::BindOnce(&OverviewItem::OnItemBoundsAnimationStarted,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&OverviewItem::OnItemBoundsAnimationEnded,
                       weak_ptr_factory_.GetWeakPtr())});
  }
  SetTransform(window, transform);

  using ClippingType = ScopedOverviewTransformWindow::ClippingType;
  ScopedOverviewTransformWindow::ClippingData clipping_data{
      ClippingType::kCustom, gfx::SizeF()};
  if (unclipped_size_)
    clipping_data.second = GetWindowTargetBoundsWithInsets().size();
  else if (is_first_update)
    clipping_data.first = ClippingType::kEnter;
  transform_window_.SetClipping(clipping_data);
}

void OverviewItem::CreateItemWidget() {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.visible_on_all_workspaces = true;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.name = "OverviewModeLabel";
  params.activatable =
      views::Widget::InitParams::Activatable::ACTIVATABLE_DEFAULT;
  params.accept_events = true;
  params.parent = transform_window_.window()->parent();
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);

  item_widget_ = std::make_unique<views::Widget>();
  item_widget_->set_focus_on_creation(false);
  item_widget_->Init(std::move(params));
  aura::Window* widget_window = item_widget_->GetNativeWindow();
  widget_window->parent()->StackChildBelow(widget_window, GetWindow());

  shadow_ = std::make_unique<ui::Shadow>();
  shadow_->Init(kShadowElevation);
  item_widget_->GetLayer()->Add(shadow_->layer());

  overview_item_view_ =
      item_widget_->SetContentsView(std::make_unique<OverviewItemView>(
          this, GetWindow(), transform_window_.IsMinimized()));
  item_widget_->Show();
  item_widget_->SetOpacity(0.f);
  item_widget_->GetLayer()->SetMasksToBounds(false);
}

void OverviewItem::UpdateHeaderLayout(OverviewAnimationType animation_type) {
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

void OverviewItem::AnimateOpacity(float opacity,
                                  OverviewAnimationType animation_type) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);
  ScopedOverviewTransformWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  transform_window_.SetOpacity(opacity);

  ScopedOverviewAnimationSettings animation_settings_label(
      animation_type, item_widget_->GetNativeWindow());
  item_widget_->SetOpacity(opacity);

  if (cannot_snap_widget_) {
    aura::Window* cannot_snap_widget_window =
        cannot_snap_widget_->GetNativeWindow();
    ScopedOverviewAnimationSettings animation_settings_label(
        animation_type, cannot_snap_widget_window);
    cannot_snap_widget_window->layer()->SetOpacity(opacity);
  }
}

void OverviewItem::StartDrag() {
  aura::Window* widget_window = item_widget_->GetNativeWindow();
  aura::Window* window = GetWindow();
  if (widget_window && widget_window->parent() == window->parent()) {
    // TODO(xdai): This might not work if there is an always on top window.
    // See crbug.com/733760.
    widget_window->parent()->StackChildAtTop(window);
    widget_window->parent()->StackChildBelow(widget_window, window);
  }
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

}  // namespace ash
