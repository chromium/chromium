// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SYSTEM_SHADOW_ON_TEXTURE_LAYER_H_
#define ASH_STYLE_SYSTEM_SHADOW_ON_TEXTURE_LAYER_H_

#include "ash/style/system_shadow.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/shadow_value.h"

namespace ash {

// An implementation of `SystemShadow` that paints shadow on a texture layer,
// which is used to resolve geometry limitations of the
// SystemShadowOnNinePatchLayer. For SystemShadowOnNinePatchLayer that paints
// the shadow on a nine patch layer will limit the shadow elevation to
// (smaller dimension - 2 * corner radius) / 4 (refer to the implementation of
// `ui::Shadow::UpdateLayerBounds`). This will cause the shadow elevation of the
// UI with a large rounded radius decrease, even be 0 if the corner radius is
// half of the UI surface’s height or width. Use the SystemShadowOnTextureLayer
// in this case.
class SystemShadowOnTextureLayer : public SystemShadow,
                                   public ui::LayerDelegate {
 public:
  explicit SystemShadowOnTextureLayer(SystemShadow::Type type);
  SystemShadowOnTextureLayer(const SystemShadowOnTextureLayer&) = delete;
  SystemShadowOnTextureLayer& operator=(const SystemShadowOnTextureLayer&) =
      delete;
  ~SystemShadowOnTextureLayer() override;

  // SystemShadow:
  void SetType(SystemShadow::Type type) override;
  void SetContentBounds(const gfx::Rect& bounds) override;
  void SetRoundedCornerRadius(int corner_radius) override;
  const gfx::Rect& GetContentBounds() override;
  ui::Layer* GetLayer() override;
  ui::Layer* GetNinePatchLayer() override;

 private:
  // Calculate layer bounds according to the content bounds and shadow values.
  gfx::Rect GetLayerBounds() const;

  // Repaint the layer after the shadow attributes change.
  void UpdateLayer();

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // The texture layer on which the shadow is painted.
  ui::Layer layer_;
  // Shadow values generated according to the shadow type.
  gfx::ShadowValues shadow_values_;
  // The bounds of the content area.
  gfx::Rect content_bounds_;
  int corner_radius_ = 0;
};

}  // namespace ash

#endif  // ASH_STYLE_SYSTEM_SHADOW_ON_TEXTURE_LAYER_H_
