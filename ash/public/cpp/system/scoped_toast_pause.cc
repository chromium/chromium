// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system/scoped_toast_pause.h"

#include "ash/public/cpp/system/toast_manager.h"

namespace ash {

ScopedToastPause::ScopedToastPause() {
  ToastManager::Get()->Pause();
}

ScopedToastPause::~ScopedToastPause() {
  ToastManager::Get()->Resume();
}

}  // namespace ash
