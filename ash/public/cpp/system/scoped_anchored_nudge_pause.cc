// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system/scoped_anchored_nudge_pause.h"

#include "ash/public/cpp/system/anchored_nudge_manager.h"

namespace ash {

ScopedAnchoredNudgePause::ScopedAnchoredNudgePause() {
  AnchoredNudgeManager::Get()->Pause();
}

ScopedAnchoredNudgePause::~ScopedAnchoredNudgePause() {
  AnchoredNudgeManager::Get()->Resume();
}

}  // namespace ash
