// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_REDRAW_REASON_H_
#define CC_SCHEDULER_REDRAW_REASON_H_

#include <string>

#include "base/containers/enum_set.h"

namespace cc {

// This is used to
// * track specific reasons a draw was triggered, and
// * compute whether any other untracked reasons besides the specific reasons
//   above also caused the draw.
enum class RedrawReason {
  kUntracked,
  kAnimatedImage,
  kScrollbarFadeOutAnimation,
  kVideoLayer,
  kMaxValue = kVideoLayer,
};

using RedrawReasonSet = base::
    EnumSet<RedrawReason, RedrawReason::kUntracked, RedrawReason::kMaxValue>;

std::string RedrawReasonToString(RedrawReason reason);

}  // namespace cc

#endif  // CC_SCHEDULER_REDRAW_REASON_H_
