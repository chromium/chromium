// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_OCCLUSION_TRACKER_H_
#define CC_TEST_TEST_OCCLUSION_TRACKER_H_

#include <stddef.h>

#include "cc/layers/render_surface_impl.h"
#include "cc/trees/occlusion_tracker.h"

namespace cc {

// A subclass to expose the total current occlusion.
class TestOcclusionTracker : public OcclusionTracker {
 public:
  explicit TestOcclusionTracker(const gfx::Rect& screen_scissor_rect)
      : OcclusionTracker(screen_scissor_rect) {}

  SimpleEnclosedRegion occlusion_from_inside_target() const {
    return stack_.back().occlusion_from_inside_target;
  }
  SimpleEnclosedRegion occlusion_from_outside_target() const {
    return stack_.back().occlusion_from_outside_target;
  }

  SimpleEnclosedRegion occlusion_on_contributing_surface_from_inside_target()
      const {
    size_t stack_size = stack_.size();
    if (stack_size < 2)
      return SimpleEnclosedRegion();
    return stack_[stack_size - 2].occlusion_from_inside_target;
  }
  SimpleEnclosedRegion occlusion_on_contributing_surface_from_outside_target()
      const {
    size_t stack_size = stack_.size();
    if (stack_size < 2)
      return SimpleEnclosedRegion();
    return stack_[stack_size - 2].occlusion_from_outside_target;
  }

  void set_occlusion_from_outside_target(const SimpleEnclosedRegion& region) {
    stack_.back().occlusion_from_outside_target = region;
  }
  void set_occlusion_from_inside_target(const SimpleEnclosedRegion& region) {
    stack_.back().occlusion_from_inside_target = region;
  }

  void set_occlusion_on_contributing_surface_from_outside_target(
      const SimpleEnclosedRegion& region) {
    size_t stack_size = stack_.size();
    DCHECK_GE(stack_size, 2u);
    stack_[stack_size - 2].occlusion_from_outside_target = region;
  }
  void set_occlusion_on_contributing_surface_from_inside_target(
      const SimpleEnclosedRegion& region) {
    size_t stack_size = stack_.size();
    DCHECK_GE(stack_size, 2u);
    stack_[stack_size - 2].occlusion_from_inside_target = region;
  }
};

}  // namespace cc

#endif  // CC_TEST_TEST_OCCLUSION_TRACKER_H_
