// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_REDRAW_REASON_H_
#define CC_SCHEDULER_REDRAW_REASON_H_

#include <string>

namespace cc {

// This is used to
// * track specific reasons a draw was triggered, and
// * compute whether any other untracked reasons besides the specific reasons
//   above also caused the draw.
// TODO(crbug.com/346732738): This is currently unused and only plumbed
// everywhere. Actually use it.
enum class RedrawReason {
  kUntracked,
};

std::string RedrawReasonToString(RedrawReason reason);

}  // namespace cc

#endif  // CC_SCHEDULER_REDRAW_REASON_H_
