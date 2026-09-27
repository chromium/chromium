// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_window_preview_bubble.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/shelf_config.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/window_preview_view.h"
#include "base/i18n/rtl.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Returns the bubble arrow position for `anchor` inside a shelf application
// menu, placing the preview bubble horizontally beside the menu toward the
// center of the display (`LEFT_CENTER` when the menu is on the left half of
// the screen, or `RIGHT_CENTER` when on the right half).
views::BubbleBorder::Arrow GetArrowForAnchor(views::View* anchor) {
  CHECK(anchor);
  const gfx::Rect anchor_bounds = anchor->GetBoundsInScreen();
  const display::Display display =
      display::Screen::Get()->GetDisplayNearestWindow(
          anchor->GetWidget()->GetNativeWindow());
  views::BubbleBorder::Arrow arrow =
      anchor_bounds.CenterPoint().x() > display.work_area().CenterPoint().x()
          ? views::BubbleBorder::RIGHT_CENTER
          : views::BubbleBorder::LEFT_CENTER;
  if (base::i18n::IsRTL()) {
    arrow = views::BubbleBorder::horizontal_mirror(arrow);
  }
  return arrow;
}

// Calculates the preferred size for the preview bubble of |window| based on its
// aspect ratio and the height and width limits configured in ShelfConfig.
gfx::Size GetPreviewSize(aura::Window* window) {
  CHECK(window);
  float ratio = 1.0f;
  if (!window->bounds().IsEmpty()) {
    ratio = static_cast<float>(window->bounds().width()) /
            window->bounds().height();
  }
  const ShelfConfig* config = ShelfConfig::Get();
  ratio = std::clamp(ratio, config->shelf_preview_min_ratio(),
                     config->shelf_preview_max_ratio());
  const int preview_height = config->shelf_preview_height();
  const int preview_max_width = config->shelf_preview_max_width();
  int width =
      std::min(static_cast<int>(preview_height * ratio), preview_max_width);
  return gfx::Size(width, preview_height);
}

// An observer that watches the detached layer fade-out animation and deletes
// itself and the detached layer tree upon completion or abortion.
class FadeOutAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit FadeOutAnimationObserver(
      std::unique_ptr<ui::LayerTreeOwner> layer_tree)
      : layer_tree_(std::move(layer_tree)) {}

  FadeOutAnimationObserver(const FadeOutAnimationObserver&) = delete;
  FadeOutAnimationObserver& operator=(const FadeOutAnimationObserver&) = delete;

  ~FadeOutAnimationObserver() override = default;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override { delete this; }

 private:
  std::unique_ptr<ui::LayerTreeOwner> layer_tree_;
};

// Duration of the fade-in and fade-out animations for the preview bubble.
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(150);

}  // namespace

ShelfWindowPreviewBubble::ShelfWindowPreviewBubble(views::View* anchor,
                                                   aura::Window* window)
    : ShelfBubble(anchor,
                  ShelfAlignment::kBottom,
                  /*for_tooltip=*/false,
                  GetArrowForAnchor(anchor)),
      window_(window) {
  CHECK(anchor);
  CHECK(window_);
  set_close_on_deactivate(false);
  SetCanActivate(false);
  set_accept_events(false);
  set_border_radius(views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_margins(gfx::Insets(0));

  set_available_screen_bounds_callback(base::BindRepeating(
      [](views::View* anchor, const gfx::Rect& rect) {
        if (anchor && anchor->GetWidget() &&
            anchor->GetWidget()->GetNativeWindow()) {
          return display::Screen::Get()
              ->GetDisplayNearestWindow(anchor->GetWidget()->GetNativeWindow())
              .work_area();
        }
        return display::Screen::Get()
            ->GetDisplayNearestPoint(rect.CenterPoint())
            .work_area();
      },
      anchor));

  window_observation_.Observe(window_);
  CreatePreviewView();

  CreateBubble();

  aura::Window* native_window = GetWidget()->GetNativeWindow();
  CollisionDetectionUtils::IgnoreWindowForCollisionDetection(native_window);
  ::wm::SetWindowVisibilityAnimationType(
      native_window, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  ::wm::SetWindowVisibilityAnimationTransition(native_window,
                                               ::wm::ANIMATE_NONE);

  ui::Layer* layer = GetWidget()->GetLayer();
  layer->SetOpacity(0.0f);
  GetWidget()->Show();
  {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(kAnimationDuration);
    settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer->SetOpacity(1.0f);
  }
}

ShelfWindowPreviewBubble::~ShelfWindowPreviewBubble() = default;

bool ShelfWindowPreviewBubble::ShouldCloseOnPressDown() {
  return true;
}

bool ShelfWindowPreviewBubble::ShouldCloseOnMouseExit() {
  return true;
}

void ShelfWindowPreviewBubble::OnWindowDestroying(aura::Window* window) {
  if (window_ == window) {
    window_observation_.Reset();
    window_ = nullptr;
    RemovePreviewView();
    if (GetWidget()) {
      GetWidget()->CloseNow();
    }
  }
}

void ShelfWindowPreviewBubble::UpdateAnchorAndWindow(views::View* anchor,
                                                     aura::Window* window) {
  CHECK(anchor);
  CHECK(window);

  if (window_ == window && GetAnchorView() == anchor) {
    return;
  }

  window_observation_.Reset();
  window_ = window;
  window_observation_.Observe(window_);

  SetArrow(GetArrowForAnchor(anchor));
  SetAnchorView(anchor);

  RemovePreviewView();
  CreatePreviewView();

  SizeToContents();
}

void ShelfWindowPreviewBubble::FadeOutAndClose() {
  views::Widget* widget = GetWidget();
  CHECK(widget);

  std::unique_ptr<ui::LayerTreeOwner> layer_tree =
      ::wm::RecreateLayers(widget->GetNativeWindow());
  ui::Layer* animating_layer = layer_tree->root();

  widget->CloseNow();

  auto* observer = new FadeOutAnimationObserver(std::move(layer_tree));
  ui::ScopedLayerAnimationSettings settings(animating_layer->GetAnimator());
  settings.AddObserver(observer);
  settings.SetTransitionDuration(kAnimationDuration);
  settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animating_layer->SetOpacity(0.0f);
}

void ShelfWindowPreviewBubble::CreatePreviewView() {
  CHECK(!preview_view_);
  CHECK(window_);
  auto preview_view =
      std::make_unique<WindowPreviewView>(window_, /*exclude_shadow=*/true);
  preview_view->SetPreferredSize(GetPreviewSize(window_));
  preview_view_ = AddChildView(std::move(preview_view));
}

void ShelfWindowPreviewBubble::RemovePreviewView() {
  if (preview_view_) {
    WindowPreviewView* to_remove = preview_view_.ExtractAsDangling();
    RemoveChildViewT(to_remove);
  }
}

BEGIN_METADATA(ShelfWindowPreviewBubble)
END_METADATA

}  // namespace ash
