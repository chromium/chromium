// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/redraw_reason.h"

namespace cc {

std::string RedrawReasonToString(RedrawReason reason) {
  switch (reason) {
    case RedrawReason::kUntracked:
      return "kUntracked";
    case RedrawReason::kAnimatedImage:
      return "kAnimatedImage";
    case RedrawReason::kScrollbarFadeOutAnimation:
      return "kScrollbarFadeOutAnimation";
    case RedrawReason::kVideoLayer:
      return "kVideoLayer";
  }
}

}  // namespace cc
