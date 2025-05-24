// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_COMMIT_EARLYOUT_REASON_H_
#define CC_SCHEDULER_COMMIT_EARLYOUT_REASON_H_

#include "base/notreached.h"
#include "cc/cc_export.h"

namespace cc {

enum class CommitEarlyOutReason {
  kAbortedNotVisible,
  kAbortedDeferredMainFrameUpdate,
  kAbortedDeferredCommit,
  kFinishedNoUpdates,
};

inline const char* CommitEarlyOutReasonToString(CommitEarlyOutReason reason) {
  switch (reason) {
    case CommitEarlyOutReason::kAbortedNotVisible:
      return "CommitEarlyOutReason::kAbortedNotVisible";
    case CommitEarlyOutReason::kAbortedDeferredMainFrameUpdate:
      return "CommitEarlyOutReason::kAbortedDeferredMainFrameUpdate";
    case CommitEarlyOutReason::kAbortedDeferredCommit:
      return "CommitEarlyOutReason::kAbortedDeferredCommit";
    case CommitEarlyOutReason::kFinishedNoUpdates:
      return "CommitEarlyOutReason::kFinishedNoUpdates";
  }
  NOTREACHED();
}

inline bool MainFrameAppliedDeltas(CommitEarlyOutReason reason) {
  return reason == CommitEarlyOutReason::kFinishedNoUpdates;
}

}  // namespace cc

#endif  // CC_SCHEDULER_COMMIT_EARLYOUT_REASON_H_
