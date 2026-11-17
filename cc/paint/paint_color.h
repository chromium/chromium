// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_COLOR_H_
#define CC_PAINT_PAINT_COLOR_H_

#include "cc/paint/deferred_property_key.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

// A color that may be replaced at raster. When `has_deferred_color()` is true,
// raster can use the key's value instead of `color`.
struct PaintColor {
  constexpr PaintColor() = default;
  constexpr PaintColor(SkColor4f color,
                       DeferredPropertyKey key = {},
                       bool needs_main_thread_repaint = false)
      : color(color),
        deferred_id(key.element_id),
        deferred_type(key.deferred_property_type),
        needs_main_thread_repaint(has_deferred_color() &&
                                  needs_main_thread_repaint) {}

  bool operator==(const PaintColor&) const = default;

  constexpr DeferredPropertyKey deferred_key() const {
    return {deferred_id, deferred_type};
  }
  constexpr bool has_deferred_color() const {
    return deferred_type != DeferredPropertyType::kInvalid;
  }

  SkColor4f color = SkColors::kBlack;
  // Ordered to keep the one-byte fields adjacent and avoid extra padding.
  ElementId deferred_id;
  DeferredPropertyType deferred_type = DeferredPropertyType::kInvalid;
  bool needs_main_thread_repaint = false;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_COLOR_H_
