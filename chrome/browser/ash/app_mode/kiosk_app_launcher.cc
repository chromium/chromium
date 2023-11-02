// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"

namespace ash {

KioskAppLauncher::KioskAppLauncher() = default;

KioskAppLauncher::KioskAppLauncher(KioskAppLauncher::Delegate* delegate)
    : delegate_(delegate) {}

void KioskAppLauncher::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

}  // namespace ash
