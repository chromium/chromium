// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_TARGET_H_
#define CC_ANIMATION_ANIMATION_TARGET_H_

#include "cc/animation/animation_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class ScrollOffset;
class SizeF;
}  // namespace gfx

namespace cc {

class FilterOperations;
class KeyframeModel;
class TransformOperations;

// An AnimationTarget is an entity that can be affected by a ticking
// cc:KeyframeModel. Any object that expects to have an opacity update, for
// example, should derive from this class.
class CC_ANIMATION_EXPORT AnimationTarget {
 public:
  virtual ~AnimationTarget() {}
  virtual void NotifyClientFloatAnimated(float value,
                                         int target_property_id,
                                         KeyframeModel* keyframe_model) = 0;
  virtual void NotifyClientFilterAnimated(const FilterOperations& filter,
                                          int target_property_id,
                                          KeyframeModel* keyframe_model) = 0;
  virtual void NotifyClientSizeAnimated(const gfx::SizeF& size,
                                        int target_property_id,
                                        KeyframeModel* keyframe_model) = 0;
  virtual void NotifyClientColorAnimated(SkColor color,
                                         int target_property_id,
                                         KeyframeModel* keyframe_model) = 0;
  virtual void NotifyClientTransformOperationsAnimated(
      const TransformOperations& operations,
      int target_property_id,
      KeyframeModel* keyframe_model) = 0;
  virtual void NotifyClientScrollOffsetAnimated(
      const gfx::ScrollOffset& scroll_offset,
      int target_property_id,
      KeyframeModel* keyframe_model) = 0;
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_TARGET_H_
