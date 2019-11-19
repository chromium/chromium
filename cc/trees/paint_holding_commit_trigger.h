// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PAINT_HOLDING_COMMIT_TRIGGER_H_
#define CC_TREES_PAINT_HOLDING_COMMIT_TRIGGER_H_

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
  kTimeout = 3,
  // The timeout was never set, probably due to non-main frame
  kNotDeferred = 4,
  // Required for UMA enum
  kMaxValue = kNotDeferred
};

}  // namespace cc

#endif  // CC_TREES_PAINT_HOLDING_COMMIT_TRIGGER_H_
