// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SYSTEM_DECORATION_H_
#define ASH_STYLE_SYSTEM_DECORATION_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_source_observer.h"
#include "ui/decoration/highlight_border_value.h"
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

namespace decoration {
class ShadowWithHighlightBorder;
}  // namespace decoration
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace ash {

// SystemDecoration generates a shadow and/or highlight border with system style
// for different types of UI surfaces, painted on a single nine-patch layer.
class ASH_EXPORT SystemDecoration : public ui::ColorProviderSourceObserver {
 public:
  // Shadow types of system UI components. The shadows with different elevations
  // have different appearance.
  enum class ShadowType {
    kNone,         // no shadow.
    kElevation4,   // corresponds to cros.sys.system-elevation1.
    kElevation12,  // corresponds to cros.sys.system-elevation3.
    kElevation24,  // corresponds to cros.sys.system-elevation5.
  };

  // Highlight border mode for system UI components.
  enum class HighlightBorderMode {
    kNone,
    kHighlightBorder,
  };

  SystemDecoration(const SystemDecoration&) = delete;
  SystemDecoration& operator=(const SystemDecoration&) = delete;

  ~SystemDecoration() override;

  // Creates a system decoration on a nine-patch layer with an optional drop
  // shadow and/or highlight border. The caller is responsible for parenting
  // `GetLayer()` and updating `SetContentBounds()`.
  static std::unique_ptr<SystemDecoration> Create(
      ShadowType shadow_type,
      HighlightBorderMode border_mode = HighlightBorderMode::kNone);

  // Creates a system decoration for `view` with an optional drop shadow and/or
  // highlight border. Its layer is added to `view`'s region layers and its
  // content bounds automatically track `view`'s layer bounds. Cannot be used
  // when the decoration's content bounds do not equal the view bounds (e.g.,
  // `AppListFolderView` has a clip rect whose bounds should be the content
  // bounds; use `Create()` instead).
  static std::unique_ptr<SystemDecoration> CreateForView(
      views::View* view,
      ShadowType shadow_type,
      HighlightBorderMode border_mode = HighlightBorderMode::kNone);

  // Creates a system decoration for `window` with an optional drop shadow
  // and/or highlight border. Its layer is added to `window`'s layer and its
  // content bounds automatically track `window`'s bounds. Cannot be used when
  // the decoration's content bounds do not equal the window bounds (e.g.,
  // `OverviewItem` for wide and tall windows; use `Create()` instead).
  static std::unique_ptr<SystemDecoration> CreateForWindow(
      aura::Window* window,
      ShadowType shadow_type,
      HighlightBorderMode border_mode = HighlightBorderMode::kNone);

  // Get shadow elevation according to the given type.
  static int GetElevationFromType(ShadowType type);

  // Configures or clears (`ShadowType::kNone`) the drop shadow.
  void SetShadow(ShadowType type);
  bool HasShadow() const;

  // Configures or clears (`HighlightBorderMode::kNone`) the highlight border.
  void SetHighlightBorder(HighlightBorderMode mode);
  bool HasHighlightBorder() const;

  void SetContentBounds(const gfx::Rect& bounds);

  // Sets the radii of the corners of the content the decoration frames.
  void SetRoundedCornerRadius(int corner_radius);
  void SetRoundedCorners(const gfx::RoundedCornersF& rounded_corners);

  const gfx::Rect& GetContentBounds() const;

  // Return the layer of the decoration. The layer is commonly used for setting
  // layer hierarchy, visibility, and transformation.
  ui::Layer* GetLayer();

  // Observe the given color provider source to update the decoration colors.
  void ObserveColorProviderSource(
      ui::ColorProviderSource* color_provider_source);

  // ui::ColorProviderSourceObserver:
  void OnColorProviderChanged() override;

  // Return the shadow values or highlight border value for testing.
  const gfx::ShadowValues GetShadowValuesForTesting() const;
  std::optional<ui::decoration::HighlightBorderValue>
  GetHighlightBorderValueForTesting() const;

 protected:
  SystemDecoration();

  // Invoked after a highlight border is added or removed (not on color
  // changes). Subclasses use this to restack `GetLayer()`.
  virtual void OnHighlightBorderChanged() {}

 private:
  void ApplyHighlightBorder(SkColor highlight_color, SkColor border_color);

  // Update shadow and highlight border colors.
  void UpdateShadowColors(const ui::ColorProvider* color_provider);
  void UpdateHighlightBorder();

  std::unique_ptr<ui::Decoration> decoration_;
  raw_ptr<ui::decoration::ShadowWithHighlightBorder> source_;
  bool has_highlight_border_ = false;
};

}  // namespace ash

#endif  // ASH_STYLE_SYSTEM_DECORATION_H_
