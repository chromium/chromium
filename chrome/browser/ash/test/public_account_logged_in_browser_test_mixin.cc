// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/test/public_account_logged_in_browser_test_mixin.h"

#include "ash/constants/ash_switches.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/test_helper.h"

namespace ash {

PublicAccountLoggedInBrowserTestMixin::PublicAccountLoggedInBrowserTestMixin(
    InProcessBrowserTestMixinHost* host,
    std::string user_id)
    : InProcessBrowserTestMixin(host), user_id_(std::move(user_id)) {}

PublicAccountLoggedInBrowserTestMixin::
    ~PublicAccountLoggedInBrowserTestMixin() = default;

void PublicAccountLoggedInBrowserTestMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(ash::switches::kLoginUser, user_id_);
  command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                  user_manager::TestHelper::GetFakeUsernameHash(
                                      AccountId::FromUserEmail(user_id_)));
}

void PublicAccountLoggedInBrowserTestMixin::SetUpLocalStatePrefService(
    PrefService* local_state) {
  user_manager::TestHelper::RegisterPublicAccountUser(*local_state, user_id_);
}

}  // namespace ash
