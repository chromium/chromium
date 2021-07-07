// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_CLIP_EXPANDER_H_
#define CC_TREES_CLIP_EXPANDER_H_

#include "cc/cc_export.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class PropertyTrees;

class CC_EXPORT ClipExpander {
 public:
  explicit ClipExpander(int filter_effect_id);
  ClipExpander(const ClipExpander& other);
  ClipExpander& operator=(const ClipExpander& other);

  bool operator==(const ClipExpander& other) const;

  bool operator!=(const ClipExpander& other) const { return !(*this == other); }

  // Maps "forward" to determine which pixels in a destination rect are affected
  // by pixels in the given source rect.
  gfx::Rect MapRect(const gfx::Rect& rect,
                    const PropertyTrees* property_trees) const;

  // Maps "backward" to determine which pixels in the source affect the pixels
  // in the given destination rect.
  gfx::Rect MapRectReverse(const gfx::Rect& rect,
                           const PropertyTrees* property_trees) const;

  // The id of the effect node in whose transform space the expansion happens.
  int target_effect_id() const { return target_effect_id_; }

 private:
  int target_effect_id_;
};

}  // namespace cc

#endif  // CC_TREES_CLIP_EXPANDER_H_
