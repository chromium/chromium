// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/lacros_data_migration_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/lifetime/application_lifetime.h"

namespace ash {

LacrosDataMigrationScreen::LacrosDataMigrationScreen(
    LacrosDataMigrationScreenView* view)
    : BaseScreen(LacrosDataMigrationScreenView::kScreenId,
                 OobeScreenPriority::SCREEN_DEVICE_DEVELOPER_MODIFICATION),
      view_(view) {
  DCHECK(view_);
  if (view_)
    view_->Bind(this);
}

LacrosDataMigrationScreen::~LacrosDataMigrationScreen() {
  if (view_)
    view_->Unbind();
}

void LacrosDataMigrationScreen::OnViewDestroyed(
    LacrosDataMigrationScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void LacrosDataMigrationScreen::ShowImpl() {
  if (!view_)
    return;

  const std::string user_id_hash =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kBrowserDataMigrationForUser);
  // Start browser data migration.
  BrowserDataMigrator::Migrate(user_id_hash,
                               base::BindOnce(&chrome::AttemptRestart));

  // Show the screen.
  view_->Show();
}

void LacrosDataMigrationScreen::HideImpl() {}

}  // namespace ash
