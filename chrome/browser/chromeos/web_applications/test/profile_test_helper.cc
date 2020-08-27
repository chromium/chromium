// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/test/profile_test_helper.h"

#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"

std::string TestProfileTypeToString(
    const ::testing::TestParamInfo<TestProfileType>& info) {
  switch (info.param) {
    case TestProfileType::kRegular:
      return "Regular";
    case TestProfileType::kIncognito:
      return "Incognito";
    case TestProfileType::kGuest:
      return "Guest";
  }
}

void ConfigureCommandLineForGuestMode(base::CommandLine* command_line) {
  command_line->AppendSwitch(chromeos::switches::kGuestSession);
  command_line->AppendSwitch(::switches::kIncognito);
  command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "hash");
  command_line->AppendSwitchASCII(
      chromeos::switches::kLoginUser,
      user_manager::GuestAccountId().GetUserEmail());
}
