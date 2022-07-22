// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system_display/system_display_serialization.h"

#include "extensions/common/api/system_display.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {
namespace api {
namespace system_display {

namespace {

// Mojo does not support optional primitive types. Serializing these require
// an ancillary |has_*| bool.
template <class T>
void SerializeOptionalPrimitive(const std::unique_ptr<T>& src_val,
                                bool* dst_has_val,
                                T* dst_val) {
  if (src_val.get()) {
    *dst_has_val = true;
    *dst_val = *src_val;
  } else {
    *dst_has_val = false;
    *dst_val = {};
  }
}

template <class T>
void DeserializeOptionalPrimitive(bool src_has_val,
                                  T src_val,
                                  std::unique_ptr<T>* dst_val) {
  DCHECK(!dst_val->get());
  if (src_has_val)
    *dst_val = std::make_unique<T>(src_val);
}

// (int, int) <==> gfx::Size

gfx::Size SerializeSize(int src_width, int src_height) {
  return gfx::Size(src_width, src_height);
}

void DeserializeSize(const gfx::Size& src, int* dst_width, int* dst_height) {
  *dst_width = src.width();
  *dst_height = src.height();
}

// extensions::api::system_display::Bounds <==> gfx::Rect.

gfx::Rect SerializeBoundsAsRect(
    const extensions::api::system_display::Bounds& src) {
  gfx::Rect dst;
  dst.set_x(src.left);
  dst.set_y(src.top);
  dst.set_width(src.width);
  dst.set_height(src.height);
  return dst;
}

void DeserializeRectToBounds(const gfx::Rect& src,
                             extensions::api::system_display::Bounds* dst) {
  dst->left = src.x();
  dst->top = src.y();
  dst->width = src.width();
  dst->height = src.height();
}

// extensions::api::system_display::Insets <==> gfx::Insets.

gfx::Insets SerializeInsets(
    const extensions::api::system_display::Insets& src) {
  gfx::Insets dst;
  // Follow element order in gfx::Insets.
  dst.set_top(src.top);
  dst.set_left(src.left);
  dst.set_bottom(src.bottom);
  dst.set_right(src.right);
  return dst;
}

void DeserializeInsets(const gfx::Insets& src,
                       extensions::api::system_display::Insets* dst) {
  // Follow element order in extensions::api::system_display::Insets.
  dst->left = src.left();
  dst->top = src.top();
  dst->right = src.right();
  dst->bottom = src.bottom();
}

}  // namespace

// extensions::api::system_display::DisplayMode <==>
//     crosapi::mojom::SysDisplayMode.

crosapi::mojom::SysDisplayModePtr SerializeDisplayMode(
    const extensions::api::system_display::DisplayMode& src) {
  auto dst = crosapi::mojom::SysDisplayMode::New();
  dst->size = SerializeSize(src.width, src.height);
  dst->size_in_native_pixels =
      SerializeSize(src.width_in_native_pixels, src.height_in_native_pixels);
  dst->device_scale_factor = src.device_scale_factor;
  dst->refresh_rate = src.refresh_rate;
  dst->is_native = src.is_native;
  dst->is_selected = src.is_selected;
  SerializeOptionalPrimitive<bool>(src.is_interlaced, &dst->has_is_interlaced,
                                   &dst->is_interlaced);
  return dst;
}

void DeserializeDisplayMode(const crosapi::mojom::SysDisplayMode& src,
                            extensions::api::system_display::DisplayMode* dst) {
  DeserializeSize(src.size, &dst->width, &dst->height);
  DeserializeSize(src.size_in_native_pixels, &dst->width_in_native_pixels,
                  &dst->height_in_native_pixels);
  dst->device_scale_factor = src.device_scale_factor;
  dst->refresh_rate = src.refresh_rate;
  dst->is_native = src.is_native;
  dst->is_selected = src.is_selected;
  DeserializeOptionalPrimitive<bool>(src.has_is_interlaced, src.is_interlaced,
                                     &dst->is_interlaced);
}

// extensions::api::system_display::Edid <==> crosapi::mojom::SysDisplayEdid.

crosapi::mojom::SysDisplayEdidPtr SerializeEdid(
    const extensions::api::system_display::Edid& src) {
  auto dst = crosapi::mojom::SysDisplayEdid::New();
  dst->manufacturer_id = src.manufacturer_id;
  dst->product_id = src.product_id;
  dst->year_of_manufacture = src.year_of_manufacture;
  return dst;
}

void DeserializeEdid(const crosapi::mojom::SysDisplayEdid& src,
                     extensions::api::system_display::Edid* dst) {
  dst->manufacturer_id = src.manufacturer_id;
  dst->product_id = src.product_id;
  dst->year_of_manufacture = src.year_of_manufacture;
}

// extensions::api::system_display::DisplayUnitInfo <==>
//     crosapi::mojom::SysDisplayUnitInfo.

crosapi::mojom::SysDisplayUnitInfoPtr SerializeDisplayUnitInfo(
    const extensions::api::system_display::DisplayUnitInfo& src) {
  auto dst = crosapi::mojom::SysDisplayUnitInfo::New();
  dst->id = src.id;
  dst->name = src.name;
  if (src.edid)
    dst->edid = SerializeEdid(*src.edid);
  dst->mirroring_source_id = src.mirroring_source_id;
  dst->mirroring_destination_ids = src.mirroring_destination_ids;
  dst->is_primary = src.is_primary;
  dst->is_internal = src.is_internal;
  dst->is_enabled = src.is_enabled;
  dst->is_unified = src.is_unified;
  SerializeOptionalPrimitive<bool>(src.is_auto_rotation_allowed,
                                   &dst->has_is_auto_rotation_allowed,
                                   &dst->is_auto_rotation_allowed);
  dst->dpi_x = src.dpi_x;
  dst->dpi_y = src.dpi_y;
  dst->rotation = src.rotation;
  dst->bounds_as_rect = SerializeBoundsAsRect(src.bounds);
  dst->overscan = SerializeInsets(src.overscan);
  dst->work_area_as_rect = SerializeBoundsAsRect(src.work_area);
  dst->display_zoom_factor = src.display_zoom_factor;
  for (const auto& src_mode : src.modes) {
    dst->modes.emplace_back(SerializeDisplayMode(src_mode));
  }
  dst->has_touch_support = src.has_touch_support;
  dst->has_accelerometer_support = src.has_accelerometer_support;
  dst->available_display_zoom_factors = src.available_display_zoom_factors;
  dst->display_zoom_factor = src.display_zoom_factor;
  return dst;
}

void DeserializeDisplayUnitInfo(
    const crosapi::mojom::SysDisplayUnitInfo& src,
    extensions::api::system_display::DisplayUnitInfo* dst) {
  dst->id = src.id;
  dst->name = src.name;
  if (src.edid) {
    dst->edid = std::make_unique<extensions::api::system_display::Edid>();
    DeserializeEdid(*src.edid, dst->edid.get());
  }
  dst->mirroring_source_id = src.mirroring_source_id;
  dst->mirroring_destination_ids = src.mirroring_destination_ids;
  dst->is_primary = src.is_primary;
  dst->is_internal = src.is_internal;
  dst->is_enabled = src.is_enabled;
  dst->is_unified = src.is_unified;
  DeserializeOptionalPrimitive<bool>(src.has_is_auto_rotation_allowed,
                                     src.is_auto_rotation_allowed,
                                     &dst->is_auto_rotation_allowed);
  dst->dpi_x = src.dpi_x;
  dst->dpi_y = src.dpi_y;
  dst->rotation = src.rotation;
  DeserializeRectToBounds(src.bounds_as_rect, &dst->bounds);
  DeserializeInsets(src.overscan, &dst->overscan);
  DeserializeRectToBounds(src.work_area_as_rect, &dst->work_area);
  dst->display_zoom_factor = src.display_zoom_factor;
  dst->modes.resize(src.modes.size());
  for (size_t i = 0; i < src.modes.size(); ++i) {
    DeserializeDisplayMode(*src.modes[i], &dst->modes[i]);
  }
  dst->has_touch_support = src.has_touch_support;
  dst->has_accelerometer_support = src.has_accelerometer_support;
  dst->available_display_zoom_factors = src.available_display_zoom_factors;
  dst->display_zoom_factor = src.display_zoom_factor;
}

}  // namespace system_display
}  // namespace api
}  // namespace extensions
