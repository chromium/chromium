// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system/anchored_nudge_manager.h"

#include "ash/constants/ash_features.h"
#include "base/check_op.h"

namespace ash {

namespace {

AnchoredNudgeManager* g_instance = nullptr;

}  // namespace

// static
AnchoredNudgeManager* AnchoredNudgeManager::Get() {
  DCHECK(features::IsSystemNudgeV2Enabled());
  return g_instance;
}

AnchoredNudgeManager::AnchoredNudgeManager() {
  CHECK(!g_instance);
  g_instance = this;
}

AnchoredNudgeManager::~AnchoredNudgeManager() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
