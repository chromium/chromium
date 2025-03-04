// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/test/kiosk_logged_in_browser_test_mixin.h"

#include "ash/constants/ash_switches.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/test_helper.h"

namespace ash {

KioskLoggedInBrowserTestMixin::KioskLoggedInBrowserTestMixin(
    InProcessBrowserTestMixinHost* host,
    std::string_view user_id)
    : InProcessBrowserTestMixin(host), user_id_(user_id) {}

KioskLoggedInBrowserTestMixin::~KioskLoggedInBrowserTestMixin() = default;

void KioskLoggedInBrowserTestMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(ash::switches::kLoginUser, user_id_);
  command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                  user_manager::TestHelper::GetFakeUsernameHash(
                                      AccountId::FromUserEmail(user_id_)));

  // Do not automatically start the kiosk app.
  command_line->AppendSwitch(ash::switches::kPreventKioskAutolaunchForTesting);
}

void KioskLoggedInBrowserTestMixin::SetUpLocalStatePrefService(
    PrefService* local_state) {
  user_manager::TestHelper::RegisterKioskAppUser(*local_state, user_id_);
}

}  // namespace ash
