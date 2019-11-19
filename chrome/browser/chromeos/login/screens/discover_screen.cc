// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/discover_screen.h"

#include "ash/public/cpp/tablet_mode.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/discover_screen_handler.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {
const char kFinished[] = "finished";
}

DiscoverScreen::DiscoverScreen(DiscoverScreenView* view,
                               const base::RepeatingClosure& exit_callback)
    : BaseScreen(DiscoverScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);
}

DiscoverScreen::~DiscoverScreen() {
  view_->Bind(nullptr);
}

void DiscoverScreen::Show() {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  if (chrome_user_manager_util::IsPublicSessionOrEphemeralLogin() ||
      !ash::TabletMode::Get()->InTabletMode() ||
      !chromeos::quick_unlock::IsPinEnabled(prefs) ||
      chromeos::quick_unlock::IsPinDisabledByPolicy(prefs)) {
    exit_callback_.Run();
    return;
  }
  view_->Show();
  is_shown_ = true;
}

void DiscoverScreen::Hide() {
  view_->Hide();
  is_shown_ = false;
}

void DiscoverScreen::OnUserAction(const std::string& action_id) {
  // Only honor finish if discover is currently being shown.
  if (action_id == kFinished && is_shown_) {
    exit_callback_.Run();
    return;
  }
  BaseScreen::OnUserAction(action_id);
}

}  // namespace chromeos
