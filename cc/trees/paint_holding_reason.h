// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PAINT_HOLDING_REASON_H_
#define CC_TREES_PAINT_HOLDING_REASON_H_

namespace cc {

enum class PaintHoldingReason {
  // Deferred to allow a frame with contentful paint.
  kFirstContentfulPaint,

  // Deferred to allow the document to be updated asynchronously for a
  // transition.
  kViewTransition,
};

}  // namespace cc

#endif  // CC_TREES_PAINT_HOLDING_REASON_H_
