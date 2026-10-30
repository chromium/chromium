// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DEFERRED_PROPERTY_KEY_H_
#define CC_PAINT_DEFERRED_PROPERTY_KEY_H_

#include <compare>
#include <cstdint>

#include "cc/paint/element_id.h"

namespace cc {

// Properties whose animated values are resolved at raster time.
enum class DeferredPropertyType : uint8_t {
  kInvalid,
  kBackgroundColor,
};

// Identifies an animated property on a source element for raster lookup.
struct DeferredPropertyKey {
  constexpr DeferredPropertyKey() = default;
  constexpr DeferredPropertyKey(ElementId element_id,
                                DeferredPropertyType deferred_property_type)
      : element_id(element_id),
        deferred_property_type(deferred_property_type) {}

  constexpr explicit operator bool() const {
    return static_cast<bool>(element_id);
  }
  friend bool operator==(const DeferredPropertyKey&,
                         const DeferredPropertyKey&) = default;
  friend std::strong_ordering operator<=>(const DeferredPropertyKey&,
                                          const DeferredPropertyKey&) = default;

  ElementId element_id;
  DeferredPropertyType deferred_property_type = DeferredPropertyType::kInvalid;
};

}  // namespace cc

#endif  // CC_PAINT_DEFERRED_PROPERTY_KEY_H_
