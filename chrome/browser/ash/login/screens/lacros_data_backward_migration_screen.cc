// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/lacros_data_backward_migration_screen.h"

#include "base/memory/weak_ptr.h"

namespace ash {

LacrosDataBackwardMigrationScreen::LacrosDataBackwardMigrationScreen(
    base::WeakPtr<LacrosDataBackwardMigrationScreenView> view)
    : BaseScreen(LacrosDataBackwardMigrationScreenView::kScreenId,
                 OobeScreenPriority::SCREEN_DEVICE_DEVELOPER_MODIFICATION),
      view_(std::move(view)) {
  DCHECK(view_);
}

LacrosDataBackwardMigrationScreen::~LacrosDataBackwardMigrationScreen() =
    default;

void LacrosDataBackwardMigrationScreen::ShowImpl() {
  if (!view_)
    return;

  // Show the screen.
  view_->Show();
}

void LacrosDataBackwardMigrationScreen::HideImpl() {}

}  // namespace ash
