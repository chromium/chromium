// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/marketing_opt_in_screen.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/marketing_opt_in_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

MarketingOptInScreen::MarketingOptInScreen(
    MarketingOptInScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(MarketingOptInScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);
}

MarketingOptInScreen::~MarketingOptInScreen() {
  view_->Bind(nullptr);
}

void MarketingOptInScreen::Show() {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  // Skip the screen if:
  //   1) the feature is disabled, or
  //   2) the screen has been shown for this user, or
  //   3) it is public session or non-regular ephemeral user login.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableMarketingOptInScreen) ||
      prefs->GetBoolean(prefs::kOobeMarketingOptInScreenFinished) ||
      chrome_user_manager_util::IsPublicSessionOrEphemeralLogin()) {
    exit_callback_.Run();
    return;
  }
  view_->Show();
  prefs->SetBoolean(prefs::kOobeMarketingOptInScreenFinished, true);
}

void MarketingOptInScreen::Hide() {
  view_->Hide();
}
void MarketingOptInScreen::OnAllSet(bool play_communications_opt_in,
                                    bool tips_communications_opt_in) {
  // TODO(https://crbug.com/852557)
  exit_callback_.Run();
}

}  // namespace chromeos
