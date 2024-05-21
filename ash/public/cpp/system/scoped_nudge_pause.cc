// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system/scoped_nudge_pause.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/public/cpp/system/system_nudge_pause_manager.h"

namespace ash {

ScopedNudgePause::ScopedNudgePause() {
  AnchoredNudgeManager::Get()->Pause();
}

ScopedNudgePause::~ScopedNudgePause() {
  AnchoredNudgeManager::Get()->Resume();
}

}  // namespace ash
