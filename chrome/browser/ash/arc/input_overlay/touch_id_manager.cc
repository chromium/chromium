// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/touch_id_manager.h"

#include <cmath>

#include "base/check.h"
#include "base/logging.h"
#include "base/no_destructor.h"

namespace arc::input_overlay {
namespace {
// 32 should be enough for touch IDs as `kNumTouchEvdevSlots` is 20.
constexpr int kMaxTouchIDs = 32;
}  // namespace

TouchIdManager* TouchIdManager::GetInstance() {
  static base::NoDestructor<TouchIdManager> instance;
  return instance.get();
}

TouchIdManager::TouchIdManager() {}
TouchIdManager::~TouchIdManager() = default;

int TouchIdManager::ObtainTouchID() {
  // In this use case, it shouldn't happen that all bits are set.
  DCHECK(touch_ids_ != ~0);
  int first_unset_pos = std::log2(~touch_ids_ & -(~touch_ids_));
  touch_ids_ |= 1 << first_unset_pos;
  return first_unset_pos;
}

void TouchIdManager::ReleaseTouchID(int touch_id) {
  DCHECK(touch_id >= 0 && touch_id < kMaxTouchIDs);
  touch_ids_ &= ~(1 << touch_id);
}

}  // namespace arc::input_overlay
