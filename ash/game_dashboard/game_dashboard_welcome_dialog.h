// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_WELCOME_DIALOG_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_WELCOME_DIALOG_H_

#include "ash/ash_export.h"
#include "base/timer/timer.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

class SystemShadow;

// `GameDashboardWelcomeDialog` is a View displayed for a set duration of time
// when first opening any game. It can be disabled via the Game Dashboard
// Settings.
class ASH_EXPORT GameDashboardWelcomeDialog : public views::FlexLayoutView {
  METADATA_HEADER(GameDashboardWelcomeDialog, views::FlexLayoutView)

 public:
  GameDashboardWelcomeDialog();
  GameDashboardWelcomeDialog(const GameDashboardWelcomeDialog&) = delete;
  GameDashboardWelcomeDialog& operator=(const GameDashboardWelcomeDialog) =
      delete;
  ~GameDashboardWelcomeDialog() override;

  // Starts the `timer_`, which will run the given `on_complete` once the time
  // specified by `kDialogDuration` has elapsed.
  void StartTimer(base::OnceClosure on_complete);

  // Sends an accessibility specific announcement for the screen reader. If the
  // screen reader isn't enabled, the announcement is automatically ignored.
  void AnnounceForAccessibility();

 private:
  // Adds a stacked title/sub-label and an icon as a row to the welcome dialog.
  void AddTitleAndIconRow();

  // Adds a row displaying how to open the dashboard.
  void AddShortcutInfoRow();

  // Timer for how long to show the welcome dialog.
  base::OneShotTimer timer_;

  std::unique_ptr<SystemShadow> shadow_;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_WELCOME_DIALOG_H_
