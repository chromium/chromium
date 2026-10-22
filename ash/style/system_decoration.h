// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SYSTEM_DECORATION_H_
#define ASH_STYLE_SYSTEM_DECORATION_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "ui/color/color_provider_source_observer.h"
#include "ui/decoration/shadow.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/shadow_value.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class ColorProvider;
class ColorProviderSource;
class Decoration;
class Layer;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace ash {

// SystemDecoration generates a shadow with system shadow style for different
// types of UI surfaces. It is based on `ui::decoration::Shadow` which paints
// the shadow on a nine patch layer.
class ASH_EXPORT SystemDecoration : public ui::ColorProviderSourceObserver {
 public:
  // Shadow types of system UI components. The shadows with different elevations
  // have different appearance.
  enum class ShadowType {
    kElevation4,   // corresponds to cros.sys.system-elevation1.
    kElevation12,  // corresponds to cros.sys.system-elevation3.
    kElevation24,  // corresponds to cros.sys.system-elevation5.
  };

  ~SystemDecoration() override;

  // Create a system shadow based on `ui::decoration::Shadow` which paints
  // shadow on a nine patch layer. This shadow can be used for any UI surfaces.
  // Usually, when creating the shadow for a window, attach the shadow's layer
  // at the bottom of the window's layer; when creating the shadow for a view,
  // attach the shadow's layer at the bottom of the view's parent layer. The
  // layer's content bounds should be manually updated.
  static std::unique_ptr<SystemDecoration> CreateShadowOnNinePatchLayer(
      ShadowType shadow_type);

  // Create a system shadow based on `views::ViewDecoration`. This shadow is
  // used for views. The shadow's layer is added to the `layers_beneath_` of the
  // view and its content bounds are adjusted with the bounds of view's layer.
  // The shadow does not need to manually update the content bounds but cannot
  // be used when the shadow's content bounds do not equal to the view bounds.
  // For example, `AppListFolderView` has a clip rect whose bounds should be the
  // content bounds of the shadow. In this case, please use
  // `CreateShadowOnNinePatchLayer` instead.
  static std::unique_ptr<SystemDecoration> CreateShadowOnNinePatchLayerForView(
      views::View* view,
      ShadowType shadow_type);

  // Create a system shadow based on `ui::decoration::Shadow`. The shadow's
  // layer is added to the bottom of the window's layer and its contents bounds
  // are adjusted with the window bounds. The shadow does not need to manually
  // update the content bounds but cannot be used when the shadow's contents
  // bounds do not equal to the window bounds. For example, the content bounds
  // of `OverviewItem` for wide and tall windows do not equal to the item
  // bounds. In this case, please use `CreateShadowOnNinePatchLayer` instead.
  static std::unique_ptr<SystemDecoration>
  CreateShadowOnNinePatchLayerForWindow(aura::Window* window,
                                        ShadowType shadow_type);

  // Get shadow elevation according to the given type.
  static int GetElevationFromType(ShadowType type);

  // Change shadow type and update shadow elevation and appearance. Note that to
  // avoid inconsistency of shadow type and elevation. Always change system
  // shadow elevation with `SetType`.
  void SetType(ShadowType type);

  void SetContentBounds(const gfx::Rect& bounds);

  // Sets the radii of the corners of the content the shadow frames.
  void SetRoundedCorners(const gfx::RoundedCornersF& rounded_corners);

  const gfx::Rect& GetContentBounds();

  // Return the layer of the shadow. The layer is commonly used for setting
  // layer hierarchy, visibility, and transformation.
  ui::Layer* GetLayer();

  // Observe the given color provider source to update the shadow colors.
  void ObserveColorProviderSource(
      ui::ColorProviderSource* color_provider_source);

  // ui::ColorProviderSourceObserver:
  void OnColorProviderChanged() override;

  // Return the shadow values of the shadow for testing.
  const gfx::ShadowValues GetShadowValuesForTesting() const;

 protected:
  virtual ui::Decoration* decoration() = 0;
  virtual const ui::Decoration* decoration() const = 0;

 private:
  // Update shadow colors with given color provider.
  void UpdateShadowColors(const ui::ColorProvider* color_provider);
};

}  // namespace ash

#endif  // ASH_STYLE_SYSTEM_DECORATION_H_
