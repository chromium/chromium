// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_drag_icon_proxy.h"

#include <utility>

#include "ash/drag_drop/drag_image_view.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/system_shadow.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/widget/widget.h"

namespace ash {

constexpr SystemShadow::Type kShadowType = SystemShadow::Type::kElevation12;

// For all app icons, there is an intended transparent ring around the visible
// icon that makes the icon looks smaller than its actual size. The shadow is
// needed to resize to align with the visual icon. Note that this constant is
// the same as `kBackgroundCircleScale` in
// chrome/browser/apps/icon_standardizer.cc
constexpr float kShadowScaleFactor = 176.f / 192.f;

AppDragIconProxy::AppDragIconProxy(
    aura::Window* root_window,
    const gfx::ImageSkia& icon,
    const gfx::Point& pointer_location_in_screen,
    const gfx::Vector2d& pointer_offset_from_center,
    float scale_factor,
    bool use_blurred_background) {
  drag_image_widget_ =
      DragImageView::Create(root_window, ui::mojom::DragEventSource::kMouse);

  DragImageView* drag_image =
      static_cast<DragImageView*>(drag_image_widget_->GetContentsView());
  drag_image->SetImage(icon);

  gfx::Size size = drag_image->GetPreferredSize();

  size.set_width(std::round(size.width() * scale_factor));
  size.set_height(std::round(size.height() * scale_factor));

  drag_image_offset_ = gfx::Vector2d(size.width() / 2, size.height() / 2) +
                       pointer_offset_from_center;

  gfx::Rect drag_image_bounds(pointer_location_in_screen - drag_image_offset_,
                              size);
  drag_image->SetBoundsInScreen(drag_image_bounds);

  // Add a layer in order to ensure the icon properly animates when
  // `AnimateToBoundsAndCloseWidget()` gets called. Layer is also required when
  // setting blur radius.
  drag_image->SetPaintToLayer();
  drag_image->layer()->SetFillsBoundsOpaquely(false);

  // Create the shadow layer.
  gfx::Size shadow_size = gfx::ScaleToFlooredSize(size, kShadowScaleFactor);
  gfx::Point shadow_offset((size.width() - shadow_size.width()) / 2,
                           (size.height() - shadow_size.height()) / 2);
  shadow_ = SystemShadow::CreateShadowOnTextureLayer(kShadowType);
  shadow_->SetRoundedCornerRadius(shadow_size.width() / 2);
  auto* shadow_layer = shadow_->GetLayer();
  auto* image_layer = drag_image->layer();

  image_layer->Add(shadow_layer);
  image_layer->StackAtBottom(shadow_layer);
  shadow_->SetContentBounds(gfx::Rect(shadow_offset, shadow_size));

  if (use_blurred_background) {
    const float radius = size.width() / 2.0f;
    drag_image->layer()->SetRoundedCornerRadius(
        {radius, radius, radius, radius});
    drag_image->layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    drag_image->layer()->SetBackdropFilterQuality(
        ColorProvider::kBackgroundBlurQuality);
  }

  drag_image_widget_->SetVisibilityAnimationTransition(
      views::Widget::ANIMATE_NONE);
  drag_image->SetWidgetVisible(true);
}

AppDragIconProxy::~AppDragIconProxy() {
  StopObservingImplicitAnimations();

  if (animation_completion_callback_)
    std::move(animation_completion_callback_).Run();
}

void AppDragIconProxy::UpdatePosition(
    const gfx::Point& pointer_location_in_screen) {
  // TODO(jennyz): Investigate why drag_image_widget_ becomes null at this point
  // per crbug.com/34722, while the app list item is still being dragged around.
  if (drag_image_widget_ && !closing_widget_) {
    static_cast<DragImageView*>(drag_image_widget_->GetContentsView())
        ->SetScreenPosition(pointer_location_in_screen - drag_image_offset_);
  }
}

void AppDragIconProxy::AnimateToBoundsAndCloseWidget(
    const gfx::Rect& bounds_in_screen,
    base::OnceClosure animation_completion_callback) {
  DCHECK(!closing_widget_);
  DCHECK(!animation_completion_callback_);

  animation_completion_callback_ = std::move(animation_completion_callback);
  closing_widget_ = true;

  // Prevent any in progress animations from interfering with the timing on the
  // animation completion callback.
  ui::Layer* target_layer = drag_image_widget_->GetContentsView()->layer();
  target_layer->GetAnimator()->AbortAllAnimations();

  gfx::Rect current_bounds = GetBoundsInScreen();
  if (current_bounds.IsEmpty()) {
    drag_image_widget_.reset();
    if (animation_completion_callback_)
      std::move(animation_completion_callback_).Run();
    return;
  }

  ui::ScopedLayerAnimationSettings animation_settings(
      target_layer->GetAnimator());
  animation_settings.SetTweenType(gfx::Tween::FAST_OUT_LINEAR_IN);
  animation_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  animation_settings.AddObserver(this);

  target_layer->SetTransform(gfx::TransformBetweenRects(
      gfx::RectF(GetBoundsInScreen()), gfx::RectF(bounds_in_screen)));
}

void AppDragIconProxy::OnImplicitAnimationsCompleted() {
  StopObserving();
  drag_image_widget_.reset();
  if (animation_completion_callback_)
    std::move(animation_completion_callback_).Run();
}

gfx::Rect AppDragIconProxy::GetBoundsInScreen() const {
  if (!drag_image_widget_)
    return gfx::Rect();
  return drag_image_widget_->GetContentsView()->GetBoundsInScreen();
}

void AppDragIconProxy::SetOpacity(float opacity) {
  if (drag_image_widget_ && !closing_widget_)
    drag_image_widget_->GetContentsView()->layer()->SetOpacity(opacity);
}

ui::Layer* AppDragIconProxy::GetImageLayerForTesting() {
  return drag_image_widget_->GetContentsView()->layer();
}

views::Widget* AppDragIconProxy::GetWidgetForTesting() {
  return drag_image_widget_.get();
}

}  // namespace ash
