// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_PAGE_SCALE_ANIMATION_H_
#define CC_INPUT_PAGE_SCALE_ANIMATION_H_

#include <memory>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "ui/gfx/geometry/cubic_bezier.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

// Used in the CC to pass around a scale animation that hasn't yet been
// initialized.
struct PendingPageScaleAnimation {
  PendingPageScaleAnimation(const gfx::Vector2d& target_offset,
                            bool use_anchor,
                            float scale,
                            const base::TimeDelta& duration)
      : target_offset(target_offset),
        use_anchor(use_anchor),
        scale(scale),
        duration(duration) {}
  gfx::Vector2d target_offset;
  bool use_anchor;
  float scale;
  base::TimeDelta duration;
};

// A small helper class that does the math for zoom animations, primarily for
// double-tap zoom. Initialize it with starting and ending scroll/page scale
// positions and an animation length time, then call ...AtTime() at every frame
// to obtain the current interpolated position. The supplied timing function
// is used to ease the animation.
//
// All sizes and vectors in this class's public methods are in the root scroll
// layer's coordinate space.
class CC_EXPORT PageScaleAnimation {
 public:
  // Construct with the state at the beginning of the animation.
  static std::unique_ptr<PageScaleAnimation> Create(
      const gfx::Vector2dF& start_scroll_offset,
      float start_page_scale_factor,
      const gfx::SizeF& viewport_size,
      const gfx::SizeF& root_layer_size);

  PageScaleAnimation(const PageScaleAnimation&) = delete;
  ~PageScaleAnimation();

  PageScaleAnimation& operator=(const PageScaleAnimation&) = delete;

  // The following methods initialize the animation. Call one of them
  // immediately after construction to set the final scroll and page scale.

  // Zoom while explicitly specifying the top-left scroll position.
  void ZoomTo(const gfx::Vector2dF& target_scroll_offset,
              float target_page_scale_factor,
              double duration);

  // Zoom based on a specified anchor. The animator will attempt to keep it
  // at the same position on the physical display throughout the animation,
  // unless the edges of the root layer are hit. The anchor is specified
  // as an offset from the content layer.
  void ZoomWithAnchor(const gfx::Vector2dF& anchor,
                      float target_page_scale_factor,
                      double duration);

  // These should be called before the first frame of animation to initialize
  // the start time. StartAnimation should only be called once after creation.
  bool IsAnimationStarted() const;
  void StartAnimation(base::TimeTicks time);

  // Call these functions while the animation is in progress to output the
  // current state.
  gfx::Vector2dF ScrollOffsetAtTime(base::TimeTicks time) const;
  float PageScaleFactorAtTime(base::TimeTicks time) const;
  bool IsAnimationCompleteAtTime(base::TimeTicks time) const;

  // The following methods return state which is invariant throughout the
  // course of the animation.
  base::TimeTicks start_time() const { return start_time_; }
  base::TimeDelta duration() const { return duration_; }
  base::TimeTicks end_time() const { return start_time_ + duration_; }
  gfx::Vector2dF target_scroll_offset() const { return target_scroll_offset_; }
  float target_page_scale_factor() const { return target_page_scale_factor_; }

 protected:
  PageScaleAnimation(const gfx::Vector2dF& start_scroll_offset,
                     float start_page_scale_factor,
                     const gfx::SizeF& viewport_size,
                     const gfx::SizeF& root_layer_size);

 private:
  void ClampTargetScrollOffset();
  void InferTargetScrollOffsetFromStartAnchor();
  void InferTargetAnchorFromScrollOffsets();

  gfx::SizeF StartViewportSize() const;
  gfx::SizeF TargetViewportSize() const;
  float InterpAtTime(base::TimeTicks time) const;
  gfx::SizeF ViewportSizeAt(float interp) const;
  gfx::Vector2dF ScrollOffsetAt(float interp) const;
  gfx::Vector2dF AnchorAt(float interp) const;
  gfx::Vector2dF ViewportRelativeAnchorAt(float interp) const;
  float PageScaleFactorAt(float interp) const;

  float start_page_scale_factor_;
  float target_page_scale_factor_;
  gfx::Vector2dF start_scroll_offset_;
  gfx::Vector2dF target_scroll_offset_;

  gfx::Vector2dF start_anchor_;
  gfx::Vector2dF target_anchor_;

  gfx::SizeF viewport_size_;
  gfx::SizeF root_layer_size_;

  base::TimeTicks start_time_;
  base::TimeDelta duration_;

  const gfx::CubicBezier timing_function_;
};

}  // namespace cc

#endif  // CC_INPUT_PAGE_SCALE_ANIMATION_H_
