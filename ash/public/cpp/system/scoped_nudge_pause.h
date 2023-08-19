// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_SCOPED_NUDGE_PAUSE_H_
#define ASH_PUBLIC_CPP_SYSTEM_SCOPED_NUDGE_PAUSE_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// An object that pauses both anchored nudge and system nudge for its lifetime.
// TODO(b/295378782): Move this class to `AnchoredNudgeManager` once complete
// migrating all nudges to `AnchoredNudgeManager`.
class ASH_PUBLIC_EXPORT ScopedNudgePause {
 public:
  ScopedNudgePause();
  ScopedNudgePause(const ScopedNudgePause&) = delete;
  ScopedNudgePause& operator=(const ScopedNudgePause&) = delete;
  ~ScopedNudgePause();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_SCOPED_NUDGE_PAUSE_H_
