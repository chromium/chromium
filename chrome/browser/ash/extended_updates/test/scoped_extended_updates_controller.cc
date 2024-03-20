// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extended_updates/test/scoped_extended_updates_controller.h"

namespace ash {

ScopedExtendedUpdatesController::ScopedExtendedUpdatesController(
    ExtendedUpdatesController* controller) {
  original_controller_ =
      ExtendedUpdatesController::SetInstanceForTesting(controller);
}

ScopedExtendedUpdatesController::~ScopedExtendedUpdatesController() {
  ExtendedUpdatesController::SetInstanceForTesting(original_controller_);
}

}  // namespace ash
