// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_transform_window.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/overview/delayed_animation_observer_impl.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/scoped_window_event_targeting_blocker.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_observer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/layout/layout_provider.h"
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

ScopedOverviewTransformWindow::GridWindowFillMode GetWindowDimensionsType(
    aura::Window* window) {
  if (window->bounds().width() >
      window->bounds().height() *
          ScopedOverviewTransformWindow::kExtremeWindowRatioThreshold) {
    return ScopedOverviewTransformWindow::GridWindowFillMode::kLetterBoxed;
  }

  if (window->bounds().height() >
      window->bounds().width() *
          ScopedOverviewTransformWindow::kExtremeWindowRatioThreshold) {
    return ScopedOverviewTransformWindow::GridWindowFillMode::kPillarBoxed;
  }

  return ScopedOverviewTransformWindow::GridWindowFillMode::kNormal;
}

}  // namespace

class ScopedOverviewTransformWindow::LayerCachingAndFilteringObserver
    : public ui::LayerObserver {
 public:
  explicit LayerCachingAndFilteringObserver(ui::Layer* layer) : layer_(layer) {
    layer_->AddObserver(this);
    layer_->AddCacheRenderSurfaceRequest();
    layer_->AddTrilinearFilteringRequest();
  }
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
  ui::Layer* layer_;

  DISALLOW_COPY_AND_ASSIGN(LayerCachingAndFilteringObserver);
};

ScopedOverviewTransformWindow::ScopedOverviewTransformWindow(
    OverviewItem* overview_item,
    aura::Window* window)
    : overview_item_(overview_item),
      window_(window),
      original_opacity_(window->layer()->GetTargetOpacity()),
      original_mask_layer_(window_->layer()->layer_mask_layer()),
      original_clip_rect_(window_->layer()->clip_rect()) {
  type_ = GetWindowDimensionsType(window);

  std::vector<aura::Window*> transient_children_to_hide;
  for (auto* transient : GetTransientTreeIterator(window)) {
    event_targeting_blocker_map_[transient] =
        std::make_unique<aura::ScopedWindowEventTargetingBlocker>(transient);

    transient->SetProperty(kIsShowingInOverviewKey, true);

    // Hide transient children which have been specified to be hidden in
    // overview mode.
    if (transient != window && transient->GetProperty(kHideInOverviewKey))
      transient_children_to_hide.push_back(transient);
  }

  if (!transient_children_to_hide.empty()) {
    hidden_transient_children_ = std::make_unique<ScopedOverviewHideWindows>(
        std::move(transient_children_to_hide), /*forced_hidden=*/true);
  }

  aura::client::GetTransientWindowClient()->AddObserver(this);

  // Tablet mode grid layout has scrolling, so all windows must be stacked under
  // the current split view window if they share the same parent so that during
  // scrolls, they get scrolled underneath the split view window. The window
  // will be returned to its proper z-order on exiting overview if it is
  // activated.
  // TODO(sammiequon): This does not handle the case if either the snapped
  // window or this window is an always on top window.
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  if (ShouldUseTabletModeGridLayout() &&
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
        return 0u;
      };

      if (get_z_order(window_) > get_z_order(snapped_window))
        window_->parent()->StackChildBelow(window_, snapped_window);
    }
  }
}

ScopedOverviewTransformWindow::~ScopedOverviewTransformWindow() {
  for (auto* transient : GetTransientTreeIterator(window_)) {
    transient->ClearProperty(kIsShowingInOverviewKey);
    DCHECK(event_targeting_blocker_map_.contains(transient));
    event_targeting_blocker_map_.erase(transient);
  }

  // No need to update the clip since we're about to restore it to
  // `original_clip_rect_`.
  UpdateRoundedCorners(/*show=*/false, /*update_clip=*/false);
  aura::client::GetTransientWindowClient()->RemoveObserver(this);
  window_->layer()->SetClipRect(original_clip_rect_);
}

// static
float ScopedOverviewTransformWindow::GetItemScale(const gfx::SizeF& source,
                                                  const gfx::SizeF& target,
                                                  int top_view_inset,
                                                  int title_height) {
  return std::min(2.0f, (target.height() - title_height) /
                            (source.height() - top_view_inset));
}

void ScopedOverviewTransformWindow::RestoreWindow(bool reset_transform) {
  // Shadow controller may be null on shutdown.
  if (Shell::Get()->shadow_controller())
    Shell::Get()->shadow_controller()->UpdateShadowForWindow(window_);

  if (IsMinimized()) {
    // Minimized windows may have had their transforms altered by swiping up
    // from the shelf.
    SetTransform(window_, gfx::Transform());
    return;
  }

  if (reset_transform) {
    ScopedAnimationSettings animation_settings_list;
    BeginScopedAnimation(overview_item_->GetExitTransformAnimationType(),
                         &animation_settings_list);
    for (auto& settings : animation_settings_list) {
      auto exit_observer = std::make_unique<ExitAnimationObserver>();
      settings->AddObserver(exit_observer.get());
      Shell::Get()->overview_controller()->AddExitAnimationObserver(
          std::move(exit_observer));
    }

    // Use identity transform directly to reset window's transform when exiting
    // overview.
    SetTransform(window_, gfx::Transform());
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
  window_->layer()->SetMaskLayer(original_mask_layer_);
}

void ScopedOverviewTransformWindow::BeginScopedAnimation(
    OverviewAnimationType animation_type,
    ScopedAnimationSettings* animation_settings) {
  if (animation_type == OVERVIEW_ANIMATION_NONE)
    return;

  for (auto* window : GetVisibleTransientTreeIterator(window_)) {
    auto settings = std::make_unique<ScopedOverviewAnimationSettings>(
        animation_type, window);
    settings->DeferPaint();

    // Create an EnterAnimationObserver if this is an enter overview layout
    // animation.
    if (animation_type == OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER) {
      auto enter_observer = std::make_unique<EnterAnimationObserver>();
      settings->AddObserver(enter_observer.get());
      Shell::Get()->overview_controller()->AddEnterAnimationObserver(
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

  if (!IsMinimized())
    return false;
  return overview_item_->item_widget()->GetNativeWindow()->Contains(target);
}

gfx::RectF ScopedOverviewTransformWindow::GetTransformedBounds() const {
  return ::ash::GetTransformedBounds(window_, GetTopInset());
}

int ScopedOverviewTransformWindow::GetTopInset() const {
  // Mirror window doesn't have insets.
  if (IsMinimized())
    return 0;
  for (auto* window : GetVisibleTransientTreeIterator(window_)) {
    // If there are regular windows in the transient ancestor tree, all those
    // windows are shown in the same overview item and the header is not masked.
    if (window != window_ &&
        window->type() == aura::client::WINDOW_TYPE_NORMAL) {
      return 0;
    }
  }
  return window_->GetProperty(aura::client::kTopViewInset);
}

void ScopedOverviewTransformWindow::SetOpacity(float opacity) {
  for (auto* window : GetVisibleTransientTreeIterator(GetOverviewWindow()))
    window->layer()->SetOpacity(opacity);
}

void ScopedOverviewTransformWindow::SetClipping(const gfx::SizeF& size) {
  // If width or height are 0, restore the overview clipping.
  if (size.IsEmpty()) {
    window_->layer()->SetClipRect(overview_clip_rect_);
    return;
  }

  // Compute the clip rect. Transform affects the clip rect, so take that into
  // account.
  gfx::Rect clip_rect;
  const gfx::Vector2dF scale = window_->layer()->GetTargetTransform().Scale2d();
  clip_rect.set_width(size.width() / scale.x());
  clip_rect.set_height(size.height() / scale.y());
  window_->layer()->SetClipRect(clip_rect);
}

gfx::RectF ScopedOverviewTransformWindow::ShrinkRectToFitPreservingAspectRatio(
    const gfx::RectF& rect,
    const gfx::RectF& bounds,
    int top_view_inset,
    int title_height) {
  DCHECK(!rect.IsEmpty());
  DCHECK_LE(top_view_inset, rect.height());
  const float scale =
      GetItemScale(rect.size(), bounds.size(), top_view_inset, title_height);
  const float horizontal_offset = 0.5 * (bounds.width() - scale * rect.width());
  const float width = bounds.width() - 2.f * horizontal_offset;
  const float vertical_offset = title_height - scale * top_view_inset;
  const float height =
      std::min(scale * rect.height(), bounds.height() - vertical_offset);
  gfx::RectF new_bounds(bounds.x() + horizontal_offset,
                        bounds.y() + vertical_offset, width, height);

  switch (type()) {
    case ScopedOverviewTransformWindow::GridWindowFillMode::kLetterBoxed:
    case ScopedOverviewTransformWindow::GridWindowFillMode::kPillarBoxed: {
      // Attempt to scale |rect| to fit |bounds|. Maintain the aspect ratio of
      // |rect|. Letter boxed windows' width will match |bounds|'s height and
      // pillar boxed windows' height will match |bounds|'s height.
      const bool is_pillar =
          type() ==
          ScopedOverviewTransformWindow::GridWindowFillMode::kPillarBoxed;
      gfx::RectF src = rect;
      new_bounds = bounds;
      src.Inset(0, top_view_inset, 0, 0);
      new_bounds.Inset(0, title_height, 0, 0);
      float scale = is_pillar ? new_bounds.height() / src.height()
                              : new_bounds.width() / src.width();
      gfx::SizeF size(is_pillar ? src.width() * scale : new_bounds.width(),
                      is_pillar ? new_bounds.height() : src.height() * scale);
      new_bounds.ClampToCenteredSize(size);

      // Extend |new_bounds| in the vertical direction to account for the header
      // that will be hidden.
      if (top_view_inset > 0)
        new_bounds.Inset(0, -(scale * top_view_inset), 0, 0);

      // Save the original bounds minus the title into |overview_bounds_|
      // so a larger backdrop can be drawn behind the window after.
      overview_bounds_ = bounds;
      overview_bounds_->Inset(0, title_height, 0, 0);
      break;
    }
    default:
      break;
  }

  return new_bounds;
}

aura::Window* ScopedOverviewTransformWindow::GetOverviewWindow() const {
  if (IsMinimized())
    return overview_item_->item_widget()->GetNativeWindow();
  return window_;
}

void ScopedOverviewTransformWindow::Close() {
  if (immediate_close_for_tests) {
    CloseWidget();
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ScopedOverviewTransformWindow::CloseWidget,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kCloseWindowDelayInMilliseconds));
}

bool ScopedOverviewTransformWindow::IsMinimized() const {
  return WindowState::Get(window_)->IsMinimized();
}

void ScopedOverviewTransformWindow::PrepareForOverview() {
  Shell::Get()->shadow_controller()->UpdateShadowForWindow(window_);

  DCHECK(!overview_started_);
  overview_started_ = true;

  // Add requests to cache render surface and perform trilinear filtering. The
  // requests will be removed in dtor. So the requests will be valid during the
  // enter animation and the whole time during overview mode. For the exit
  // animation of overview mode, we need to add those requests again.
  if (features::IsTrilinearFilteringEnabled()) {
    for (auto* window : GetVisibleTransientTreeIterator(GetOverviewWindow())) {
      cached_and_filtered_layer_observers_.push_back(
          std::make_unique<LayerCachingAndFilteringObserver>(window->layer()));
    }
  }
}

void ScopedOverviewTransformWindow::EnsureVisible() {
  original_opacity_ = 1.f;
}

void ScopedOverviewTransformWindow::UpdateWindowDimensionsType() {
  type_ = GetWindowDimensionsType(window_);
  overview_bounds_.reset();
}

void ScopedOverviewTransformWindow::UpdateRoundedCorners(bool show,
                                                         bool update_clip) {
  // Hide the corners if minimized, OverviewItemView will handle showing the
  // rounded corners on the UI.
  const bool show_corners = show && !IsMinimized();
  // Add the mask which gives the overview item rounded corners, and add the
  // shadow around the window.
  ui::Layer* layer = window_->layer();
  const float scale = layer->transform().Scale2d().x();
  const int radius =
      views::LayoutProvider::Get()->GetCornerRadiusMetric(views::EMPHASIS_LOW);
  const gfx::RoundedCornersF radii(show_corners ? (radius / scale) : 0.0f);
  layer->SetRoundedCornerRadius(radii);
  layer->SetIsFastRoundedCorner(true);

  if (!update_clip || layer->GetAnimator()->is_animating() || IsMinimized())
    return;

  const int top_inset = GetTopInset();
  if (top_inset > 0) {
    gfx::Rect clip_rect(window_->bounds().size());
    // We add 1 to the top_inset, because in some cases, the header is not
    // clipped fully due to what seems to be a rounding error.
    // TODO(afakhry|sammiequon): Investigate a proper fix for this.
    clip_rect.Inset(0, top_inset + 1, 0, 0);
    ScopedOverviewAnimationSettings settings(
        OVERVIEW_ANIMATION_FRAME_HEADER_CLIP, window_);
    layer->SetClipRect(clip_rect);
    overview_clip_rect_ = clip_rect;
  }
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
  transient_child->SetProperty(kIsShowingInOverviewKey, true);
}

void ScopedOverviewTransformWindow::OnTransientChildWindowRemoved(
    aura::Window* parent,
    aura::Window* transient_child) {
  if (parent != window_ && !::wm::HasTransientAncestor(parent, window_))
    return;

  transient_child->ClearProperty(kIsShowingInOverviewKey);
  DCHECK(event_targeting_blocker_map_.contains(transient_child));
  event_targeting_blocker_map_.erase(transient_child);
}

void ScopedOverviewTransformWindow::CloseWidget() {
  aura::Window* parent_window = ::wm::GetTransientRoot(window_);
  if (parent_window)
    window_util::CloseWidgetForWindow(parent_window);
}

// static
void ScopedOverviewTransformWindow::SetImmediateCloseForTests() {
  immediate_close_for_tests = true;
}

}  // namespace ash
