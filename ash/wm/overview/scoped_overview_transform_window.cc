// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_transform_window.h"

#include <algorithm>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/style/system_shadow.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/overview/delayed_animation_observer_impl.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/scoped_layer_tree_synchronizer.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_constants.h"
#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/scoped_window_event_targeting_blocker.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// When set to true by tests makes closing the widget synchronous.
bool immediate_close_for_tests = false;

// Delay closing window to allow it to shrink and fade out.
constexpr int kCloseWindowDelayInMilliseconds = 150;

void ClearWindowProperties(aura::Window* window) {
  window->ClearProperty(chromeos::kIsShowingInOverviewKey);
  window->ClearProperty(kHideInOverviewKey);
}

// Layer animation observer that is attached to a clip and/or rounded corners
// animation. We need this for the exit animation, where we want to animate
// properties but the overview session has been destroyed. We want to use this
// observer for animations that require an intermediate step. For example, when
// removing a clip, we want to first animate to the size of the window, and then
// set the clip rect to be empty after the animation has completed.
class UndoPropertyObserver : public ui::ImplicitAnimationObserver,
                             public aura::WindowObserver {
 public:
  explicit UndoPropertyObserver(aura::Window* window) : window_(window) {
    window_->AddObserver(this);
  }
  UndoPropertyObserver(const UndoPropertyObserver&) = delete;
  UndoPropertyObserver& operator=(const UndoPropertyObserver&) = delete;
  ~UndoPropertyObserver() override {
    StopObservingImplicitAnimations();
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

 private:
  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    window_->layer()->SetClipRect(gfx::Rect());
    delete this;
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    CHECK_EQ(window_, window);
    delete this;
  }

  // Guaranteed to be not null for the duration of `this`.
  raw_ptr<aura::Window> window_;
};

}  // namespace

class ScopedOverviewTransformWindow::LayerCachingAndFilteringObserver
    : public ui::LayerObserver {
 public:
  explicit LayerCachingAndFilteringObserver(ui::Layer* layer) : layer_(layer) {
    layer_->AddObserver(this);
    layer_->AddCacheRenderSurfaceRequest();
    layer_->AddTrilinearFilteringRequest();
  }

  LayerCachingAndFilteringObserver(const LayerCachingAndFilteringObserver&) =
      delete;
  LayerCachingAndFilteringObserver& operator=(
      const LayerCachingAndFilteringObserver&) = delete;

  ~LayerCachingAndFilteringObserver() override {
    if (layer_) {
      layer_->RemoveTrilinearFilteringRequest();
      layer_->RemoveCacheRenderSurfaceRequest();
      layer_->RemoveObserver(this);
    }
  }

  // ui::LayerObserver overrides:
  void LayerDestroyed(ui::Layer* layer) override {
    layer_->RemoveObserver(this);
    layer_ = nullptr;
  }

 private:
  raw_ptr<ui::Layer> layer_;
};

ScopedOverviewTransformWindow::ScopedOverviewTransformWindow(
    OverviewItem* overview_item,
    aura::Window* window)
    : overview_item_(overview_item),
      window_(window),
      original_opacity_(window->layer()->GetTargetOpacity()),
      original_clip_rect_(window_->layer()->GetTargetClipRect()) {
  raster_scale_observer_lock_.emplace(
      (new RasterScaleLayerObserver(window_, window_->layer(), window_))
          ->Lock());

  fill_mode_ = GetOverviewItemFillModeForWindow(window);

  std::vector<raw_ptr<aura::Window, VectorExperimental>>
      transient_children_to_hide;
  for (auto* transient : GetTransientTreeIterator(window)) {
    event_targeting_blocker_map_[transient] =
        std::make_unique<aura::ScopedWindowEventTargetingBlocker>(transient);

    if (window_util::AsBubbleDialogDelegate(transient)) {
      transient->SetProperty(kHideInOverviewKey, true);
    } else {
      transient->SetProperty(chromeos::kIsShowingInOverviewKey, true);
      // Add this as `aura::WindowObserver` for observing `kHideInOverviewKey`
      // property changes.
      window_observations_.AddObservation(transient);
    }

    // Hide transient children which have been specified to be hidden in
    // overview mode.
    if (transient != window && transient->GetProperty(kHideInOverviewKey)) {
      transient_children_to_hide.push_back(transient);
    }
  }

  if (!transient_children_to_hide.empty())
    AddHiddenTransientWindows(std::move(transient_children_to_hide));

  aura::client::GetTransientWindowClient()->AddObserver(this);

  // Tablet mode grid layout has scrolling, so all windows must be stacked under
  // the current split view window if they share the same parent so that during
  // scrolls, they get scrolled underneath the split view window. The window
  // will be returned to its proper z-order on exiting overview if it is
  // activated.
  // TODO(sammiequon): This does not handle the case if either the snapped
  // window or this window is an always on top window.
  if (auto* split_view_controller =
          SplitViewController::Get(Shell::GetPrimaryRootWindow());
      ShouldUseTabletModeGridLayout() &&
      split_view_controller->InSplitViewMode()) {
    aura::Window* snapped_window =
        split_view_controller->GetDefaultSnappedWindow();
    if (window->parent() == snapped_window->parent()) {
      // Helper to get the z order of a window in its parent.
      auto get_z_order = [](aura::Window* window) -> size_t {
        for (size_t i = 0u; i < window->parent()->children().size(); ++i) {
          if (window == window->parent()->children()[i])
            return i;
        }
        NOTREACHED();
      };

      if (get_z_order(window_) > get_z_order(snapped_window))
        window_->parent()->StackChildBelow(window_, snapped_window);
    }
  }

  // In overview, each window, along with its transient window, has the
  // display's root window as a common ancestor.
  // Note: windows in the overview belong to different containers. For instance,
  // normal windows belong to a desk container, floated windows to a float
  // container, and always-on-top windows to their respective container.
  window_tree_synchronizer_ = std::make_unique<ScopedWindowTreeSynchronizer>(
      window_->GetRootWindow(), /*restore_tree=*/true);
}

ScopedOverviewTransformWindow::~ScopedOverviewTransformWindow() {
  // Reset clipping in the case `RestoreWindow()` is not called, such as when
  // `this` is dragged to another display. Without this check, `SetClipping`
  // would override the one we called in `RestoreWindow()` which would result in
  // the same final clip but may remove the animation. See crbug.com/1140639.
  if (reset_clip_on_shutdown_) {
    SetClipping(gfx::Rect(original_clip_rect_.size()));
  }

  for (auto* transient : GetTransientTreeIterator(window_)) {
    ClearWindowProperties(transient);
    DCHECK(event_targeting_blocker_map_.contains(transient));
    event_targeting_blocker_map_.erase(transient);
  }

  UpdateRoundedCorners(/*show=*/false);
  aura::client::GetTransientWindowClient()->RemoveObserver(this);
}

// static
float ScopedOverviewTransformWindow::GetItemScale(int source_height,
                                                  int target_height,
                                                  int top_view_inset,
                                                  int title_height) {
  return std::min(2.0f, static_cast<float>(target_height - title_height) /
                            (source_height - top_view_inset));
}

void ScopedOverviewTransformWindow::RestoreWindow(bool reset_transform,
                                                  bool animate) {
  base::AutoReset<bool> restoring(&is_restoring_, true);

  // Shadow controller may be null on shutdown.
  if (auto* shadow_controller = Shell::Get()->shadow_controller()) {
    shadow_controller->UpdateShadowForWindow(window_);
  }

  // We will handle clipping here, no need to do anything in the destructor.
  reset_clip_on_shutdown_ = false;

  if (!animate || IsMinimizedOrTucked()) {
    // Minimized windows may have had their transforms altered by swiping up
    // from the shelf.
    ScopedOverviewAnimationSettings animation_settings(OVERVIEW_ANIMATION_NONE,
                                                       window_);
    window_util::SetTransform(window_, gfx::Transform());
    SetClipping(gfx::Rect(original_clip_rect_.size()));
    return;
  }

  if (reset_transform) {
    ScopedAnimationSettings animation_settings_list;
    BeginScopedAnimation(overview_item_->GetExitTransformAnimationType(),
                         &animation_settings_list);
    for (auto& settings : animation_settings_list) {
      auto exit_observer = std::make_unique<ExitAnimationObserver>();
      settings->AddObserver(exit_observer.get());
      if (window_->layer()->GetAnimator() == settings->GetAnimator())
        settings->AddObserver(new WindowTransformAnimationObserver(window_));
      OverviewController::Get()->AddExitAnimationObserver(
          std::move(exit_observer));
    }

    // Use identity transform directly to reset window's transform when exiting
    // overview.
    window_util::SetTransform(window_, gfx::Transform());

    // Add requests to cache render surface and perform trilinear filtering for
    // the exit animation of overview mode. The requests will be removed when
    // the exit animation finishes.
    if (features::IsTrilinearFilteringEnabled()) {
      for (auto& settings : animation_settings_list) {
        settings->CacheRenderSurface();
        settings->TrilinearFiltering();
      }
    }
  }

  ScopedOverviewAnimationSettings animation_settings(
      overview_item_->GetExitOverviewAnimationType(), window_);
  SetOpacity(original_opacity_);
  if (original_clip_rect_.IsEmpty()) {
    animation_settings.AddObserver(new UndoPropertyObserver(window_));
    SetClipping(gfx::Rect(window_->bounds().size()));
  } else {
    SetClipping(gfx::Rect(original_clip_rect_.size()));
  }

  window_tree_synchronizer_->Restore();
}

void ScopedOverviewTransformWindow::BeginScopedAnimation(
    OverviewAnimationType animation_type,
    ScopedAnimationSettings* animation_settings) {
  if (animation_type == OVERVIEW_ANIMATION_NONE)
    return;

  for (auto* window : window_util::GetVisibleTransientTreeIterator(window_)) {
    auto settings = std::make_unique<ScopedOverviewAnimationSettings>(
        animation_type, window);
    settings->DeferPaint();

    // Create an EnterAnimationObserver if this is an enter overview layout
    // animation.
    if (animation_type == OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER) {
      auto enter_observer = std::make_unique<EnterAnimationObserver>();
      settings->AddObserver(enter_observer.get());
      OverviewController::Get()->AddEnterAnimationObserver(
          std::move(enter_observer));
    }

    animation_settings->push_back(std::move(settings));
  }
}

bool ScopedOverviewTransformWindow::Contains(const aura::Window* target) const {
  for (auto* window : GetTransientTreeIterator(window_)) {
    if (window->Contains(target))
      return true;
  }

  if (!IsMinimizedOrTucked()) {
    return false;
  }

  // A minimized window's item_widget_ may have already been destroyed.
  const auto* item_widget = overview_item_->item_widget();
  if (!item_widget)
    return false;

  return item_widget->GetNativeWindow()->Contains(target);
}

gfx::RectF ScopedOverviewTransformWindow::GetTransformedBounds() const {
  return window_util::GetTransformedBounds(window_, GetTopInset());
}

int ScopedOverviewTransformWindow::GetTopInset() const {
  // Mirror window doesn't have insets.
  if (IsMinimizedOrTucked()) {
    return 0;
  }
  for (auto* window : window_util::GetVisibleTransientTreeIterator(window_)) {
    // If there are regular windows in the transient ancestor tree, all those
    // windows are shown in the same overview item and the header is not masked.
    if (window != window_ &&
        window->GetType() == aura::client::WINDOW_TYPE_NORMAL) {
      return 0;
    }
  }
  return window_->GetProperty(aura::client::kTopViewInset);
}

void ScopedOverviewTransformWindow::SetOpacity(float opacity) {
  for (auto* window :
       window_util::GetVisibleTransientTreeIterator(GetOverviewWindow()))
    window->layer()->SetOpacity(opacity);
}

void ScopedOverviewTransformWindow::SetClipping(const gfx::Rect& clip_rect) {
  // No need to clip `window_` if it is about to be destroyed.
  if (window_->is_destroying()) {
    return;
  }

  ui::Layer* layer = window_->layer();
  // TODO(sammiequon): Investigate why we cannot use
  // `ui::Layer::GetTargetClipRect()` here.
  if (layer->GetAnimator()->GetTargetClipRect() == clip_rect) {
    return;
  }

  layer->SetClipRect(clip_rect);
}

gfx::RectF ScopedOverviewTransformWindow::ShrinkRectToFitPreservingAspectRatio(
    const gfx::RectF& rect,
    const gfx::RectF& bounds,
    int top_view_inset,
    int title_height) const {
  DCHECK(!rect.IsEmpty());
  DCHECK_LE(top_view_inset, rect.height());
  const float scale = GetItemScale(rect.height(), bounds.height(),
                                   top_view_inset, title_height);
  const float horizontal_offset = 0.5 * (bounds.width() - scale * rect.width());
  const float width = bounds.width() - 2.f * horizontal_offset;
  const float vertical_offset = title_height - scale * top_view_inset;
  const float height =
      std::min(scale * rect.height(), bounds.height() - vertical_offset);
  gfx::RectF new_bounds(bounds.x() + horizontal_offset,
                        bounds.y() + vertical_offset, width, height);

  switch (fill_mode_) {
    case OverviewItemFillMode::kLetterBoxed:
    case OverviewItemFillMode::kPillarBoxed: {
      // Attempt to scale |rect| to fit |bounds|. Maintain the aspect ratio of
      // |rect|. Letter boxed windows' width will match |bounds|'s width and
      // pillar boxed windows' height will match |bounds|'s height.
      const bool is_pillar = fill_mode_ == OverviewItemFillMode::kPillarBoxed;
      const gfx::Rect window_bounds =
          ::wm::GetTransientRoot(window_)->GetBoundsInScreen();
      const float window_ratio =
          static_cast<float>(window_bounds.width()) / window_bounds.height();
      if (is_pillar) {
        const float new_width = height * window_ratio;
        new_bounds.set_width(new_width);
      } else {
        // For some use cases, the `new_height` is larger than the maximum
        // height should be applied to the window within the `bounds` even it's
        // letter box window type. In this case we should use maximum height
        // directly.
        float new_height = std::min(height, bounds.width() / window_ratio);

        new_bounds = bounds;
        new_bounds.Inset(gfx::InsetsF::TLBR(title_height, 0, 0, 0));
        if (top_view_inset) {
          new_bounds.set_height(new_height);
          // Calculate `scaled_top_view_inset` without considering
          // `title_height` because we have already inset the top of
          // `new_bounds` by that value. We also do not consider
          // `top_view_inset` in our calculation of `new_scale` because we want
          // to find out the height of the inset when the whole window,
          // including the inset, is scaled down to `new_bounds`.
          const float new_scale =
              GetItemScale(rect.height(), new_bounds.height(), 0, 0);
          const float scaled_top_view_inset = top_view_inset * new_scale;
          // Offset `new_bounds` to be at a point in the overview item frame
          // where it will be centered when we clip the `top_view_inset`.
          new_bounds.Offset(0, (bounds.height() - title_height) / 2 -
                                   (new_height - scaled_top_view_inset) / 2 -
                                   scaled_top_view_inset);
        } else {
          new_bounds.ClampToCenteredSize(
              gfx::SizeF(bounds.width(), new_height));
        }
      }
      break;
    }
    default:
      break;
  }

  // If we do not use whole numbers, there may be some artifacts drawn (i.e.
  // shadows, notches). This may be an effect of subpixel rendering. It's ok to
  // round it here since this is the last calculation (we don't have to worry
  // about roundoff error).
  return gfx::RectF(gfx::ToRoundedRect(new_bounds));
}

aura::Window* ScopedOverviewTransformWindow::GetOverviewWindow() {
  if (IsMinimizedOrTucked()) {
    return overview_item_->item_widget()->GetNativeWindow();
  }
  return window_;
}

void ScopedOverviewTransformWindow::Close() {
  if (immediate_close_for_tests) {
    CloseWidget();
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ScopedOverviewTransformWindow::CloseWidget,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(kCloseWindowDelayInMilliseconds));
}

bool ScopedOverviewTransformWindow::IsMinimizedOrTucked() const {
  return window_util::IsMinimizedOrTucked(window_);
}

void ScopedOverviewTransformWindow::PrepareForOverview() {
  Shell::Get()->shadow_controller()->UpdateShadowForWindow(window_);

  // Add requests to cache render surface and perform trilinear filtering. The
  // requests will be removed in dtor. So the requests will be valid during the
  // enter animation and the whole time during overview mode. For the exit
  // animation of overview mode, we need to add those requests again.
  if (features::IsTrilinearFilteringEnabled()) {
    for (auto* window :
         window_util::GetVisibleTransientTreeIterator(GetOverviewWindow())) {
      cached_and_filtered_layer_observers_.push_back(
          std::make_unique<LayerCachingAndFilteringObserver>(window->layer()));
    }
  }
}

void ScopedOverviewTransformWindow::EnsureVisible() {
  original_opacity_ = 1.f;
}

void ScopedOverviewTransformWindow::UpdateOverviewItemFillMode() {
  fill_mode_ = GetOverviewItemFillModeForWindow(window_);
}

void ScopedOverviewTransformWindow::UpdateRoundedCorners(bool show) {
  // TODO(b/274470528): Keep track of the corner radius animations.

  // Hide the corners if minimized, OverviewItemView will handle showing the
  // rounded corners on the UI.
  if (IsMinimizedOrTucked()) {
    DCHECK(!show);
  }

  ui::Layer* layer = window_->layer();
  layer->SetIsFastRoundedCorner(true);

  if (!show) {
    layer->SetRoundedCornerRadius(gfx::RoundedCornersF());
    return;
  }

  const gfx::RectF contents_bounds_in_screen = GetTransformedBounds();

  // Depending on the size of `backdrop_view`, we might not want to round the
  // window associated with `layer`.
  const bool has_rounding = window_util::ShouldRoundThumbnailWindow(
      overview_item_->GetBackDropView(), contents_bounds_in_screen);

  const float scale = layer->transform().To2dScale().x();
  layer->SetRoundedCornerRadius(
      has_rounding ? window_util::GetMiniWindowRoundedCorners(
                         window(), /*include_header_rounding=*/false, scale)
                   : gfx::RoundedCornersF(0));

  if (!chromeos::features::IsRoundedWindowsEnabled()) {
    return;
  }

  gfx::RectF contents_bounds_in_root(contents_bounds_in_screen);
  wm::TranslateRectFromScreen(window_->GetRootWindow(),
                              &contents_bounds_in_root);

  const gfx::RRectF rounded_contents_bounds(
      contents_bounds_in_root,
      window_util::GetMiniWindowRoundedCorners(
          window(), /*include_header_rounding=*/false));

  // Synchronizing the rounded corners of a window and its transient hierarchy
  // against `rounded_contents_bounds` yields two outcomes:
  // * We can apply the specified rounding without the need for a render
  //   surface.
  // * It ensures that the transient windows' corners are correctly rounded,
  //   ensuring that all four corners of the WindowMiniView appear rounded.
  //   See b/325635179.
  window_tree_synchronizer_->SynchronizeRoundedCorners(
      window(), /*consider_curvature=*/false, rounded_contents_bounds,
      /*ignore_predicate=*/base::BindRepeating([](aura::Window* window) {
        return window->GetProperty(kHideInOverviewKey) ||
               window->GetProperty(kExcludeFromTransientTreeTransformKey);
      }));
}

void ScopedOverviewTransformWindow::OnTransientChildWindowAdded(
    aura::Window* parent,
    aura::Window* transient_child) {
  if (parent != window_ && !::wm::HasTransientAncestor(parent, window_))
    return;

  DCHECK(!event_targeting_blocker_map_.contains(transient_child));
  event_targeting_blocker_map_[transient_child] =
      std::make_unique<aura::ScopedWindowEventTargetingBlocker>(
          transient_child);
  transient_child->SetProperty(chromeos::kIsShowingInOverviewKey, true);

  // Hide transient children which have been specified to be hidden in
  // overview mode.
  if (transient_child != window_ &&
      transient_child->GetProperty(kHideInOverviewKey)) {
    AddHiddenTransientWindows({transient_child});
  }

  // Add this as |aura::WindowObserver| for observing |kHideInOverviewKey|
  // property changes.
  window_observations_.AddObservation(transient_child);
}

void ScopedOverviewTransformWindow::OnTransientChildWindowRemoved(
    aura::Window* parent,
    aura::Window* transient_child) {
  if (parent != window_ && !::wm::HasTransientAncestor(parent, window_))
    return;

  ClearWindowProperties(transient_child);
  DCHECK(event_targeting_blocker_map_.contains(transient_child));
  event_targeting_blocker_map_.erase(transient_child);

  if (window_observations_.IsObservingSource(transient_child))
    window_observations_.RemoveObservation(transient_child);
}

void ScopedOverviewTransformWindow::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (window == window_ && key == chromeos::kWindowStateTypeKey) {
    const auto old_window_state = static_cast<chromeos::WindowStateType>(old);

    // During the restore process, the synchronizer attempts to restore the
    // rounded corners of the window's layer tree to the state it was in just
    // before entering overview.
    // However, this is not always be desirable. For instance, if an overview
    // item is dragged into a snapped state, the synchronizer may hold an
    // outdated original state. While the original state was for a
    // rounded window, the window is now square in the snapped state.
    if (chromeos::ShouldWindowHaveRoundedCorners(window) !=
        chromeos::ShouldWindowStateHaveRoundedCorners(old_window_state)) {
      window_tree_synchronizer_->ResetCachedLayerInfo();
    }

    return;
  }

  if (key != kHideInOverviewKey)
    return;

  const auto current_value = window->GetProperty(kHideInOverviewKey);
  if (current_value == old)
    return;

  if (current_value) {
    AddHiddenTransientWindows({window});
  } else {
    hidden_transient_children_->RemoveWindow(window, /*show_window=*/true);
  }
}

void ScopedOverviewTransformWindow::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (window == window_ || is_restoring_) {
    return;
  }

  // Transient window is repositioned. The new position within the
  // overview item needs to be recomputed. No need to recompute if the
  // transient is invisible. It will get placed properly when it reshows on
  // overview end.
  if (!window->IsVisible())
    return;

  overview_item_->SetBounds(overview_item_->target_bounds(),
                            OVERVIEW_ANIMATION_NONE);
}

void ScopedOverviewTransformWindow::OnWindowDestroying(aura::Window* window) {
  DCHECK(window_observations_.IsObservingSource(window));
  window_observations_.RemoveObservation(window);
}

// static
void ScopedOverviewTransformWindow::SetImmediateCloseForTests(bool immediate) {
  immediate_close_for_tests = immediate;
}

void ScopedOverviewTransformWindow::CloseWidget() {
  if (aura::Window* parent_window = wm::GetTransientRoot(window_)) {
    window_util::CloseWidgetForWindow(parent_window);
  }
}

void ScopedOverviewTransformWindow::AddHiddenTransientWindows(
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>&
        transient_windows) {
  if (!hidden_transient_children_) {
    hidden_transient_children_ = std::make_unique<ScopedOverviewHideWindows>(
        std::move(transient_windows), /*forced_hidden=*/true);
  } else {
    for (aura::Window* window : transient_windows) {
      hidden_transient_children_->AddWindow(window);
    }
  }
}

}  // namespace ash
