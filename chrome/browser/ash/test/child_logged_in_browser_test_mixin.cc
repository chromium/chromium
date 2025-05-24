// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/test/child_logged_in_browser_test_mixin.h"

#include "ash/constants/ash_switches.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/test_helper.h"

namespace ash {

ChildLoggedInBrowserTestMixin::ChildLoggedInBrowserTestMixin(
    InProcessBrowserTestMixinHost* host,
    const AccountId& account_id)
    : InProcessBrowserTestMixin(host), account_id_(account_id) {
  CHECK(account_id_.is_valid());
}

ChildLoggedInBrowserTestMixin::~ChildLoggedInBrowserTestMixin() = default;

void ChildLoggedInBrowserTestMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                  account_id_.GetUserEmail());
  command_line->AppendSwitchASCII(
      ash::switches::kLoginProfile,
      user_manager::TestHelper::GetFakeUsernameHash(account_id_));

  // Skip checking policy for browser_tests.
  command_line->AppendSwitchASCII(ash::switches::kProfileRequiresPolicy,
                                  "false");
}

void ChildLoggedInBrowserTestMixin::SetUpLocalStatePrefService(
    PrefService* local_state) {
  user_manager::TestHelper::RegisterPersistedChildUser(*local_state,
                                                       account_id_);
}

}  // namespace ash
