// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/extensions/background_mode_manager.h"

void BackgroundModeManager::DisplayClientInstalledNotification(
    const std::u16string& name) {
  // Mac does not display a notification when a background mode is
  // entered.  The associated bug for implementing this was closed as OBSOLETE,
  // and the feature was never implemented. See https://crbug.com/74970 for
  // more details.
}
