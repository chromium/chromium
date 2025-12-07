// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/paint_holding_commit_trigger.h"

#include "base/notreached.h"

namespace cc {

PaintHoldingCommitTrigger ReasonToTimeoutTrigger(PaintHoldingReason reason) {
  switch (reason) {
    case PaintHoldingReason::kFirstContentfulPaint:
      return PaintHoldingCommitTrigger::kTimeoutFCP;
    case PaintHoldingReason::kViewTransition:
      return PaintHoldingCommitTrigger::kTimeoutViewTransition;
  }
  NOTREACHED();
}

}  // namespace cc
