// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DEFERRED_VALUE_MAP_H_
#define CC_PAINT_DEFERRED_VALUE_MAP_H_

#include "base/containers/flat_map.h"
#include "cc/paint/deferred_property_key.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

// The sampled value of each active deferred property, for raster.
struct CC_PAINT_EXPORT DeferredValueMap {
  DeferredValueMap();
  explicit DeferredValueMap(
      base::flat_map<DeferredPropertyKey, SkColor4f> colors);
  DeferredValueMap(const DeferredValueMap&) = delete;
  DeferredValueMap& operator=(const DeferredValueMap&) = delete;
  DeferredValueMap(DeferredValueMap&&) noexcept;
  DeferredValueMap& operator=(DeferredValueMap&&) noexcept;
  ~DeferredValueMap();

  DeferredValueMap Clone() const;

  friend bool operator==(const DeferredValueMap&,
                         const DeferredValueMap&) = default;

  base::flat_map<DeferredPropertyKey, SkColor4f> colors;
};

}  // namespace cc

#endif  // CC_PAINT_DEFERRED_VALUE_MAP_H_
