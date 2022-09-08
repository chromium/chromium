// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_DRAW_RESULT_H_
#define CC_SCHEDULER_DRAW_RESULT_H_

namespace cc {

// Note that these values are reported in UMA. So entries should never be
// renumbered, and numeric values should never be reused.
enum DrawResult {
  INVALID_RESULT,
  DRAW_SUCCESS,
  DRAW_ABORTED_CHECKERBOARD_ANIMATIONS,
  DRAW_ABORTED_MISSING_HIGH_RES_CONTENT,
  DRAW_ABORTED_CANT_DRAW,
  DRAW_ABORTED_DRAINING_PIPELINE,
  // Magic constant used by the histogram macros.
  kMaxValue = DRAW_ABORTED_DRAINING_PIPELINE,
};

}  // namespace cc

#endif  // CC_SCHEDULER_DRAW_RESULT_H_
