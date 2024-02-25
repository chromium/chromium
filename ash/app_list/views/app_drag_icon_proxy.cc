// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_drag_icon_proxy.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/drag_drop/drag_image_view.h"
#include "ash/public/cpp/style/color_provider.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/widget/widget.h"

namespace ash {

constexpr SystemShadow::Type kShadowType = SystemShadow::Type::kElevation12;

constexpr base::TimeDelta kProxyAnimationDuration = base::Milliseconds(200);

// For all app icons, there is an intended transparent ring around the visible
// icon that makes the icon looks smaller than its actual size. The shadow is
// needed to resize to align with the visual icon. Note that this constant is
// the same as `kBackgroundCircleScale` in
// chrome/browser/apps/icon_standardizer.cc
constexpr float kShadowScaleFactor = 176.f / 192.f;

AppDragIconProxy::AppDragIconProxy(
    aura::Window* root_window,
    const gfx::ImageSkia& icon,
    const gfx::ImageSkia& badge_icon,
    const gfx::Point& pointer_location_in_screen,
    const gfx::Vector2d& pointer_offset_from_center,
    float scale_factor,
    bool is_folder_icon,
    const gfx::Size& shadow_size) {
  drag_image_widget_ =
      DragImageView::Create(root_window, ui::mojom::DragEventSource::kMouse);

  DragImageView* drag_image =
      static_cast<DragImageView*>(drag_image_widget_->GetContentsView());

  if (badge_icon.isNull()) {
    drag_image->SetImage(icon);
  } else {
    drag_image->SetImage(
        gfx::ImageSkiaOperations::CreateIconWithBadge(icon, badge_icon));
  }
  gfx::Size size = drag_image->GetPreferredSize();

  // Create the drag image layer.
  size = gfx::ScaleToRoundedSize(size, scale_factor);
  drag_image_offset_ = gfx::Vector2d(size.width() / 2, size.height() / 2) +
                       pointer_offset_from_center;
  const gfx::Rect drag_image_bounds(
      pointer_location_in_screen - drag_image_offset_, size);
  drag_image->SetBoundsInScreen(drag_image_bounds);

  // Add a layer in order to ensure the icon properly animates when
  // `AnimateToBoundsAndCloseWidget()` gets called. Layer is also required when
  // setting blur radius.
  drag_image->SetPaintToLayer();
  drag_image->layer()->SetFillsBoundsOpaquely(false);

  // Create the shadow layer.
  const float shadow_scale_factor =
      is_folder_icon ? scale_factor : scale_factor * kShadowScaleFactor;
  const gfx::Size scaled_shadow_size =
      gfx::ScaleToRoundedSize(shadow_size, shadow_scale_factor);
  const gfx::Point shadow_offset(
      (size.width() - scaled_shadow_size.width()) / 2,
      (size.height() - scaled_shadow_size.height()) / 2);
  shadow_ = SystemShadow::CreateShadowOnTextureLayer(kShadowType);
  shadow_->SetRoundedCornerRadius(scaled_shadow_size.width() / 2);
  drag_image->AddLayerToRegion(shadow_->GetLayer(), views::LayerRegion::kBelow);

  shadow_->SetContentBounds(gfx::Rect(shadow_offset, scaled_shadow_size));
  shadow_->ObserveColorProviderSource(drag_image_widget_.get());

  if (is_folder_icon) {
    // The blur should be only added on the background circle, where none of
    // any existing layer is bounded to that area.
    // Therefore, the `blurred_background_layer_` is needed here to explicitly
    // blur the background of the icon.
    blurred_background_layer_ =
        std::make_unique<ui::LayerOwner>(std::make_unique<ui::Layer>());
    ui::Layer* const blurred_layer = blurred_background_layer_->layer();
    drag_image->AddLayerToRegion(blurred_layer, views::LayerRegion::kBelow);
    blurred_layer->SetBounds(shadow_->GetContentBounds());
    const float corner_radius = shadow_->GetContentBounds().width() / 2.0f;

    blurred_layer->SetRoundedCornerRadius(
        {corner_radius, corner_radius, corner_radius, corner_radius});
    blurred_layer->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    blurred_layer->SetBackdropFilterQuality(
        ColorProvider::kBackgroundBlurQuality);
  }

  drag_image_widget_->SetVisibilityAnimationTransition(
      views::Widget::ANIMATE_NONE);
  drag_image->SetWidgetVisible(true);
}

AppDragIconProxy::~AppDragIconProxy() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (animation_completion_callback_) {
    std::move(animation_completion_callback_).Run();
  }
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

  const gfx::Transform transform = gfx::TransformBetweenRects(
      gfx::RectF(GetBoundsInScreen()), gfx::RectF(bounds_in_screen));

  views::AnimationBuilder builder;
  builder.SetPreemptionStrategy(ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET)
      .OnEnded(base::BindOnce(&AppDragIconProxy::OnProxyAnimationCompleted,
                              weak_ptr_factory_.GetWeakPtr()))
      .OnAborted(base::BindOnce(&AppDragIconProxy::OnProxyAnimationCompleted,
                                weak_ptr_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(kProxyAnimationDuration)
      .SetTransform(shadow_->GetLayer(), transform,
                    gfx::Tween::FAST_OUT_LINEAR_IN)
      .SetTransform(target_layer, transform, gfx::Tween::FAST_OUT_LINEAR_IN);
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

ui::Layer* AppDragIconProxy::GetBlurredLayerForTesting() {
  return blurred_background_layer_->layer();
}

void AppDragIconProxy::OnProxyAnimationCompleted() {
  drag_image_widget_.reset();
  if (animation_completion_callback_) {
    std::move(animation_completion_callback_).Run();
  }
}

}  // namespace ash
