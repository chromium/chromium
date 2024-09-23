// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_DAMAGE_REASON_H_
#define CC_TREES_DAMAGE_REASON_H_

#include "base/containers/enum_set.h"

namespace cc {

// This is used to
// * track specific reasons that contributed to damage, and
// * compute whether any other untracked reasons besides the specific reasons
//   above also contributed to damage.
enum class DamageReason {
  kUntracked,
  kAnimatedImage,
  kScrollbarFadeOutAnimation,
  kVideoLayer,
  kMaxValue = kVideoLayer,
};

using DamageReasonSet = base::
    EnumSet<DamageReason, DamageReason::kUntracked, DamageReason::kMaxValue>;

}  // namespace cc

#endif  // CC_TREES_DAMAGE_REASON_H_
