// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PAINT_HOLDING_COMMIT_TRIGGER_H_
#define CC_TREES_PAINT_HOLDING_COMMIT_TRIGGER_H_

#include "cc/trees/paint_holding_reason.h"

namespace cc {

enum class PaintHoldingCommitTrigger {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // The Paint Holding flag is not enabled
  kFeatureDisabled = 0,
  // Paint Holding is not allowed due to different origin or wrong protocol
  kDisallowed = 1,
  // The commit was triggered by first contentful paint (FCP)
  kFirstContentfulPaint = 2,
  // The commit was triggered by a timeout waiting for FCP
  kTimeoutFCP = 3,
  // The timeout was never set, probably due to non-main frame
  kNotDeferred = 4,
  // The commit was triggered by a view transition start
  kViewTransition = 5,
  // The commit was triggered by a timeout waiting for view transition start
  kTimeoutViewTransition = 6,
  // The commit was triggered because a new blink widget was attached to the
  // compositor.
  kWidgetSwapped = 7,
  // Required for UMA enum
  kMaxValue = kWidgetSwapped
};

PaintHoldingCommitTrigger ReasonToTimeoutTrigger(PaintHoldingReason reason);

}  // namespace cc

#endif  // CC_TREES_PAINT_HOLDING_COMMIT_TRIGGER_H_
