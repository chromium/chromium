// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_DRAW_RESULT_H_
#define CC_SCHEDULER_DRAW_RESULT_H_

namespace cc {

// Note that these values are reported in UMA. So entries should never be
// renumbered, and numeric values should never be reused.
enum class DrawResult {
  kInvalidResult,
  kSuccess,
  kAbortedCheckerboardAnimations,
  kAbortedMissingHighResContent,
  kAbortedCantDraw,
  kAbortedDrainingPipeline,
  // Magic constant used by the histogram macros.
  kMaxValue = kAbortedDrainingPipeline,
};

}  // namespace cc

#endif  // CC_SCHEDULER_DRAW_RESULT_H_
