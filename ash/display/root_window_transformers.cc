// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/root_window_transformers.h"

#include <cmath>

#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/display/display_util.h"
#include "ash/host/root_window_transformer.h"
#include "ash/shell.h"
#include "ash/utility/transformer_util.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/system/sys_info.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace ash {
namespace {

// TODO(oshima): Transformers should be able to adjust itself
// when the device scale factor is changed, instead of
// precalculating the transform using fixed value.

// Creates rotation transform for |root_window| to |new_rotation|. This will
// call |CreateRotationTransform()|, the |old_rotation| will implicitly be
// |display::Display::ROTATE_0|.
gfx::Transform CreateRootWindowRotationTransform(
    const display::Display& display) {
  gfx::SizeF size(display.GetSizeInPixel());

  // Use SizeF so that the origin of translated layer will be
  // aligned when scaled back at pixels.
  size.Scale(1.f / display.device_scale_factor());
  return CreateRotationTransform(display::Display::ROTATE_0,
                                 display.panel_rotation(), size);
}

gfx::Transform CreateInsetsTransform(const gfx::Insets& insets,
                                     float device_scale_factor) {
  gfx::Transform transform;
  if (insets.top() != 0 || insets.left() != 0) {
    float x_offset = insets.left() / device_scale_factor;
    float y_offset = insets.top() / device_scale_factor;
    transform.Translate(x_offset, y_offset);
  }
  return transform;
}

// Returns a transform with rotation adjusted |insets_in_pixel|. The transform
// is applied to the root window so that |insets_in_pixel| looks correct after
// the rotation applied at the output.
gfx::Transform CreateReverseRotatedInsetsTransform(
    display::Display::Rotation rotation,
    const gfx::Insets& insets_in_pixel,
    float device_scale_factor) {
  float x_offset = 0;
  float y_offset = 0;

  switch (rotation) {
    case display::Display::ROTATE_0:
      x_offset = insets_in_pixel.left();
      y_offset = insets_in_pixel.top();
      break;
    case display::Display::ROTATE_90:
      x_offset = insets_in_pixel.top();
      y_offset = insets_in_pixel.right();
      break;
    case display::Display::ROTATE_180:
      x_offset = insets_in_pixel.right();
      y_offset = insets_in_pixel.bottom();
      break;
    case display::Display::ROTATE_270:
      x_offset = insets_in_pixel.bottom();
      y_offset = insets_in_pixel.left();
      break;
  }

  gfx::Transform transform;
  if (x_offset != 0 || y_offset != 0) {
    x_offset /= device_scale_factor;
    y_offset /= device_scale_factor;
    transform.Translate(x_offset, y_offset);
  }
  return transform;
}

// RootWindowTransformer for ash environment.
class AshRootWindowTransformer : public RootWindowTransformer {
 public:
  AshRootWindowTransformer(const display::Display& display) {
    initial_root_bounds_ = gfx::Rect(display.size());
    display::DisplayManager* display_manager = Shell::Get()->display_manager();
    display::ManagedDisplayInfo info =
        display_manager->GetDisplayInfo(display.id());
    host_insets_ = info.GetOverscanInsetsInPixel();
    gfx::Transform insets_and_rotation_transform =
        CreateInsetsTransform(host_insets_, display.device_scale_factor()) *
        CreateRootWindowRotationTransform(display);
    transform_ = insets_and_rotation_transform;
    insets_and_scale_transform_ = CreateReverseRotatedInsetsTransform(
        display.panel_rotation(), host_insets_, display.device_scale_factor());
    FullscreenMagnifierController* magnifier =
        Shell::Get()->fullscreen_magnifier_controller();
    if (magnifier) {
      gfx::Transform magnifier_scale = magnifier->GetMagnifierTransform();
      transform_ *= magnifier_scale;
      insets_and_scale_transform_ *= magnifier_scale;
    }

    CHECK(transform_.GetInverse(&invert_transform_));
    CHECK(insets_and_rotation_transform.GetInverse(
        &root_window_bounds_transform_));

    root_window_bounds_transform_.Scale(1.f / display.device_scale_factor(),
                                        1.f / display.device_scale_factor());

    initial_host_size_ = info.bounds_in_native().size();
  }

  AshRootWindowTransformer(const AshRootWindowTransformer&) = delete;
  AshRootWindowTransformer& operator=(const AshRootWindowTransformer&) = delete;

  // aura::RootWindowTransformer overrides:
  gfx::Transform GetTransform() const override { return transform_; }
  gfx::Transform GetInverseTransform() const override {
    return invert_transform_;
  }
  gfx::Rect GetRootWindowBounds(const gfx::Size& host_size) const override {
    if (base::SysInfo::IsRunningOnChromeOS())
      return initial_root_bounds_;

    // If we're running on linux desktop for dev purpose, the host window
    // may be updated to new size. Recompute the root window bounds based
    // on the host size if the host size changed.
    if (initial_host_size_ == host_size)
      return initial_root_bounds_;

    gfx::RectF new_bounds = gfx::RectF(gfx::SizeF(host_size));
    new_bounds.Inset(gfx::InsetsF(host_insets_));
    new_bounds = root_window_bounds_transform_.MapRect(new_bounds);

    // Root window origin will be (0,0) except during bounds changes.
    // Set to exactly zero to avoid rounding issues.
    // Floor the size because the bounds is no longer aligned to
    // backing pixel when |root_window_scale_| is specified
    // (850 height at 1.25 scale becomes 1062.5 for example.)
    return gfx::Rect(gfx::ToFlooredSize(new_bounds.size()));
  }

  gfx::Insets GetHostInsets() const override { return host_insets_; }
  gfx::Transform GetInsetsAndScaleTransform() const override {
    return insets_and_scale_transform_;
  }

 private:
  ~AshRootWindowTransformer() override = default;

  gfx::Transform transform_;

  // The accurate representation of the inverse of the |transform_|.
  // This is used to avoid computation error caused by
  // |gfx::Transform::GetInverse|.
  gfx::Transform invert_transform_;

  // The transform of the root window bounds. This is used to calculate the size
  // of the root window. It is the composition of the following transforms
  //   - inverse of insets. Insets position the content area within the display.
  //   - inverse of rotation. Rotation changes orientation of the content area.
  //   - inverse of device scale. Scaling up content shrinks the content area.
  //
  // Insets also shrink the content area but this happens prior to applying the
  // transformation in GetRootWindowBounds().
  //
  // Magnification does not affect the window size. Content is clipped in this
  // case, but the magnifier allows panning to reach clipped areas.
  //
  // The transforms are inverted because GetTransform() is the transform from
  // root window coordinates to host coordinates, but this transform is used in
  // the reverse direction (derives root window bounds from display bounds).
  gfx::Transform root_window_bounds_transform_;

  gfx::Insets host_insets_;
  gfx::Transform insets_and_scale_transform_;
  gfx::Rect initial_root_bounds_;
  gfx::Size initial_host_size_;
};

// RootWindowTransformer for mirror root window. We simply copy the
// texture (bitmap) of the source display into the mirror window, so
// the root window bounds is the same as the source display's
// pixel size (excluding overscan insets).
class MirrorRootWindowTransformer : public RootWindowTransformer {
 public:
  MirrorRootWindowTransformer(
      const display::ManagedDisplayInfo& source_display_info,
      const display::ManagedDisplayInfo& mirror_display_info) {
    root_bounds_ =
        gfx::Rect(source_display_info.GetSizeInPixelWithPanelOrientation());
    display::Display::Rotation active_root_rotation =
        source_display_info.GetActiveRotation();

    const bool should_undo_rotation = ShouldUndoRotationForMirror();
    gfx::Transform rotation_transform;
    if (should_undo_rotation) {
      // Calculate the transform to undo the rotation and apply it to the
      // source display. Use GetActiveRotation() because |source_bounds_|
      // includes panel rotation and we only need to revert active rotation.
      rotation_transform = CreateRotationTransform(
          source_display_info.GetActiveRotation(), display::Display::ROTATE_0,
          gfx::SizeF(root_bounds_.size()));
      root_bounds_ = gfx::ToNearestRect(
          rotation_transform.MapRect(gfx::RectF(root_bounds_)));
      active_root_rotation = display::Display::ROTATE_0;
    }

    gfx::Rect mirror_display_rect =
        gfx::Rect(mirror_display_info.bounds_in_native().size());

    // When logical rotation is 90 or 270 degree, transpose is needed to apply
    // reverse rotation to `root_bounds_` and `mirror_display_rect` to exclude
    // the rotation. This is because the rotation happens at viz output and
    // `transform_` needs to be calculated without the rotation.
    // E.g. host native size 1600x1200. Rotation 90 degree. `transform_` needs
    // to fit 1200x1600 rather than 1600x1200.
    const bool need_transpose =
        active_root_rotation == display::Display::ROTATE_90 ||
        active_root_rotation == display::Display::ROTATE_270;
    if (need_transpose) {
      root_bounds_.Transpose();
      mirror_display_rect.Transpose();
    }

    bool letterbox = root_bounds_.width() * mirror_display_rect.height() >
                     root_bounds_.height() * mirror_display_rect.width();
    if (letterbox) {
      float mirror_scale_ratio =
          (static_cast<float>(root_bounds_.width()) /
           static_cast<float>(mirror_display_rect.width()));
      float inverted_scale = 1.0f / mirror_scale_ratio;
      int margin = static_cast<int>((mirror_display_rect.height() -
                                     root_bounds_.height() * inverted_scale) /
                                    2);
      insets_ = gfx::Insets::TLBR(0, margin, 0, margin);

      transform_.Translate(0, margin);
      transform_.Scale(inverted_scale, inverted_scale);
    } else {
      float mirror_scale_ratio =
          (static_cast<float>(root_bounds_.height()) /
           static_cast<float>(mirror_display_rect.height()));
      float inverted_scale = 1.0f / mirror_scale_ratio;
      int margin = static_cast<int>((mirror_display_rect.width() -
                                     root_bounds_.width() * inverted_scale) /
                                    2);
      insets_ = gfx::Insets::TLBR(margin, 0, margin, 0);

      transform_.Translate(margin, 0);
      transform_.Scale(inverted_scale, inverted_scale);
    }
  }

  MirrorRootWindowTransformer(const MirrorRootWindowTransformer&) = delete;
  MirrorRootWindowTransformer& operator=(const MirrorRootWindowTransformer&) =
      delete;

  // aura::RootWindowTransformer overrides:
  gfx::Transform GetTransform() const override { return transform_; }
  gfx::Transform GetInverseTransform() const override {
    gfx::Transform invert;
    CHECK(transform_.GetInverse(&invert));
    return invert;
  }
  gfx::Rect GetRootWindowBounds(const gfx::Size& host_size) const override {
    return root_bounds_;
  }
  gfx::Insets GetHostInsets() const override { return insets_; }
  gfx::Transform GetInsetsAndScaleTransform() const override {
    return transform_;
  }

 private:
  ~MirrorRootWindowTransformer() override = default;

  gfx::Transform transform_;
  gfx::Rect root_bounds_;
  gfx::Insets insets_;
};

class PartialBoundsRootWindowTransformer : public RootWindowTransformer {
 public:
  PartialBoundsRootWindowTransformer(const gfx::Rect& screen_bounds,
                                     const display::Display& display) {
    const display::DisplayManager* display_manager =
        Shell::Get()->display_manager();
    display::ManagedDisplayInfo display_info =
        display_manager->GetDisplayInfo(display.id());
    // Physical root bounds.
    root_bounds_ = gfx::Rect(display_info.bounds_in_native().size());

    display::Display::Rotation active_root_rotation =
        display_info.GetActiveRotation();
    const bool need_transpose =
        active_root_rotation == display::Display::ROTATE_90 ||
        active_root_rotation == display::Display::ROTATE_270;
    if (need_transpose)
      root_bounds_.Transpose();

    // |screen_bounds| is the unified desktop logical bounds.
    // Calculate the unified height scale value, and apply the same scale on the
    // row physical height to get the row logical height.
    display::Display unified_display =
        display::Screen::GetScreen()->GetPrimaryDisplay();
    const int unified_physical_height =
        unified_display.GetSizeInPixel().height();
    const int unified_logical_height = screen_bounds.height();
    const float unified_height_scale =
        static_cast<float>(unified_logical_height) / unified_physical_height;

    const int row_index =
        display_manager->GetMirroringDisplayRowIndexInUnifiedMatrix(
            display.id());
    const int row_physical_height =
        display_manager->GetUnifiedDesktopRowMaxHeight(row_index);
    const int row_logical_height = row_physical_height * unified_height_scale;
    const float dsf = unified_display.device_scale_factor();
    const float scale = root_bounds_.height() / (dsf * row_logical_height);

    transform_.Scale(scale, scale);
    transform_.Translate(-SkIntToScalar(display.bounds().x()),
                         -SkIntToScalar(display.bounds().y()));
    // Scaling using physical root bounds here, because rotation is applied
    // before device_scale_factor is applied.
    gfx::Transform rotation = CreateRotationTransform(
        display::Display::ROTATE_0, display.panel_rotation(),
        gfx::SizeF(root_bounds_.size()));
    CHECK((rotation * transform_).GetInverse(&invert_transform_));
  }

  PartialBoundsRootWindowTransformer(
      const PartialBoundsRootWindowTransformer&) = delete;
  PartialBoundsRootWindowTransformer& operator=(
      const PartialBoundsRootWindowTransformer&) = delete;

  // RootWindowTransformer:
  gfx::Transform GetTransform() const override { return transform_; }
  gfx::Transform GetInverseTransform() const override {
    return invert_transform_;
  }
  gfx::Rect GetRootWindowBounds(const gfx::Size& host_size) const override {
    return root_bounds_;
  }
  gfx::Insets GetHostInsets() const override { return gfx::Insets(); }
  gfx::Transform GetInsetsAndScaleTransform() const override {
    return transform_;
  }

 private:
  ~PartialBoundsRootWindowTransformer() override = default;

  gfx::Transform transform_;
  gfx::Transform invert_transform_;
  gfx::Rect root_bounds_;
};

}  // namespace

std::unique_ptr<RootWindowTransformer> CreateRootWindowTransformerForDisplay(
    const display::Display& display) {
  return base::WrapUnique<RootWindowTransformer>(
      new AshRootWindowTransformer(display));
}

std::unique_ptr<RootWindowTransformer>
CreateRootWindowTransformerForMirroredDisplay(
    const display::ManagedDisplayInfo& source_display_info,
    const display::ManagedDisplayInfo& mirror_display_info) {
  return base::WrapUnique<RootWindowTransformer>(
      new MirrorRootWindowTransformer(source_display_info,
                                      mirror_display_info));
}

std::unique_ptr<RootWindowTransformer>
CreateRootWindowTransformerForUnifiedDesktop(const gfx::Rect& screen_bounds,
                                             const display::Display& display) {
  return base::WrapUnique<RootWindowTransformer>(
      new PartialBoundsRootWindowTransformer(screen_bounds, display));
}

}  // namespace ash
