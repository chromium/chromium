// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SYSTEM_SHADOW_ON_NINE_PATCH_LAYER_H_
#define ASH_STYLE_SYSTEM_SHADOW_ON_NINE_PATCH_LAYER_H_

#include "ash/public/cpp/view_shadow.h"
#include "ash/style/system_shadow.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor_extra/shadow.h"

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

// SystemShadowOnNinePatchLayer is an interface for the shadows based on
// ui::Shadow which paints shadow on a nine patch layer. The shadow attributes
// are set and get from ui::Shadow's functions. The child classes need to expose
// their ui::Shadow pointer in `shadow()`.
class SystemShadowOnNinePatchLayer : public SystemShadow {
 public:
  ~SystemShadowOnNinePatchLayer() override;

  // SystemShadow:
  void SetType(SystemShadow::Type type) override;
  void SetContentBounds(const gfx::Rect& bounds) override;
  void SetRoundedCornerRadius(int corner_radius) override;
  const gfx::Rect& GetContentBounds() override;
  ui::Layer* GetLayer() override;
  ui::Layer* GetNinePatchLayer() override;

 protected:
  virtual ui::Shadow* shadow() = 0;
};

// An implementation of `SystemShadowOnNinePatchLayer`. It is directly based on
// the ui::Shadow.
class SystemShadowOnNinePatchLayerImpl : public SystemShadowOnNinePatchLayer {
 public:
  explicit SystemShadowOnNinePatchLayerImpl(SystemShadow::Type type);
  SystemShadowOnNinePatchLayerImpl(const SystemShadowOnNinePatchLayerImpl&) =
      delete;
  SystemShadowOnNinePatchLayerImpl& operator=(
      const SystemShadowOnNinePatchLayerImpl&) = delete;
  ~SystemShadowOnNinePatchLayerImpl() override;

 private:
  // SystemShadowOnNinePatchLayer:
  ui::Shadow* shadow() override;

  ui::Shadow shadow_;
};

// An implementation of `SystemShadowOnNinePatchLayer`. It is based on
// ViewShadow. The ViewShadow is added in the layers beneath the view and
// adjusts its content bounds with the view's bounds. Do not manually set the
// content bounds.
class SystemViewShadowOnNinePatchLayer : public SystemShadowOnNinePatchLayer {
 public:
  SystemViewShadowOnNinePatchLayer(views::View* view, SystemShadow::Type type);
  SystemViewShadowOnNinePatchLayer(const SystemViewShadowOnNinePatchLayer&) =
      delete;
  SystemViewShadowOnNinePatchLayer& operator=(
      const SystemViewShadowOnNinePatchLayer&) = delete;
  ~SystemViewShadowOnNinePatchLayer() override;

  // SystemShadow:
  void SetRoundedCornerRadius(int corner_radius) override;

 private:
  // SystemShadowOnNinePatchLayer:
  void SetContentBounds(const gfx::Rect& content_bounds) override;
  ui::Shadow* shadow() override;

  ViewShadow view_shadow_;
};

// An extension of SystemShadowOnNinePatchLayerImpl. The shadow is added at the
// bottom of a window's layer and adjusts its content bounds with the window's
// bounds. Do not manually set the content bounds.
class SystemWindowShadowOnNinePatchLayer
    : public SystemShadowOnNinePatchLayerImpl,
      public aura::WindowObserver {
 public:
  SystemWindowShadowOnNinePatchLayer(aura::Window* window,
                                     SystemShadow::Type type);
  SystemWindowShadowOnNinePatchLayer(
      const SystemWindowShadowOnNinePatchLayer&) = delete;
  SystemWindowShadowOnNinePatchLayer& operator=(
      const SystemWindowShadowOnNinePatchLayer&) = delete;
  ~SystemWindowShadowOnNinePatchLayer() override;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroyed(aura::Window* window) override;

 private:
  // SystemShadowOnNinePatchLayerImpl:
  void SetContentBounds(const gfx::Rect& content_bounds) override;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace ash

#endif  // ASH_STYLE_SYSTEM_SHADOW_ON_NINE_PATCH_LAYER_H_
