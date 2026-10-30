// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_ANIMATION_CURVE_PROXY_H_
#define CC_PAINT_ANIMATION_CURVE_PROXY_H_

namespace cc {

// Turns animation progress into a value of type `T`. Implemented by the
// embedder (e.g. from Blink's keyframes), and sampled on the compositor thread.
template <typename T>
class AnimationCurveProxy {
 public:
  // Writes the value at `progress` (0 to 1).
  virtual void Sample(float progress, T* value) const = 0;

  // Called by scoped_refptr, so cc can keep the curve alive. Implementations
  // forward these to the ref count they already have, so cc and the embedder
  // share one count.
  virtual void AddRef() const = 0;
  virtual void Release() const = 0;

 protected:
  virtual ~AnimationCurveProxy() = default;
};

}  // namespace cc

#endif  // CC_PAINT_ANIMATION_CURVE_PROXY_H_
