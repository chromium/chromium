// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_COMMIT_EARLYOUT_REASON_H_
#define CC_SCHEDULER_COMMIT_EARLYOUT_REASON_H_

#include "base/notreached.h"
#include "cc/cc_export.h"

namespace cc {

enum class CommitEarlyOutReason {
  ABORTED_NOT_VISIBLE,
  ABORTED_DEFERRED_MAIN_FRAME_UPDATE,
  ABORTED_DEFERRED_COMMIT,
  FINISHED_NO_UPDATES,
};

inline const char* CommitEarlyOutReasonToString(CommitEarlyOutReason reason) {
  switch (reason) {
    case CommitEarlyOutReason::ABORTED_NOT_VISIBLE:
      return "CommitEarlyOutReason::ABORTED_NOT_VISIBLE";
    case CommitEarlyOutReason::ABORTED_DEFERRED_MAIN_FRAME_UPDATE:
      return "CommitEarlyOutReason::ABORTED_DEFERRED_MAIN_FRAME_UPDATE";
    case CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT:
      return "CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT";
    case CommitEarlyOutReason::FINISHED_NO_UPDATES:
      return "CommitEarlyOutReason::FINISHED_NO_UPDATES";
  }
  NOTREACHED();
  return "???";
}

inline bool MainFrameAppliedDeltas(CommitEarlyOutReason reason) {
  return reason == CommitEarlyOutReason::FINISHED_NO_UPDATES;
}

}  // namespace cc

#endif  // CC_SCHEDULER_COMMIT_EARLYOUT_REASON_H_
