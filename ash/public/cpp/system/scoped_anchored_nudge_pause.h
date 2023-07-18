// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_SCOPED_ANCHORED_NUDGE_PAUSE_H_
#define ASH_PUBLIC_CPP_SYSTEM_SCOPED_ANCHORED_NUDGE_PAUSE_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// An object that pauses anchored nudges for its lifetime.
class ASH_PUBLIC_EXPORT ScopedAnchoredNudgePause {
 public:
  ScopedAnchoredNudgePause();
  ScopedAnchoredNudgePause(const ScopedAnchoredNudgePause&) = delete;
  ScopedAnchoredNudgePause& operator=(const ScopedAnchoredNudgePause&) = delete;
  ~ScopedAnchoredNudgePause();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_SCOPED_ANCHORED_NUDGE_PAUSE_H_
