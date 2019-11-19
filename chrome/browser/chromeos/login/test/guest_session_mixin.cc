// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/guest_session_mixin.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"

namespace chromeos {

GuestSessionMixin::GuestSessionMixin(InProcessBrowserTestMixinHost* mixin_host)
    : InProcessBrowserTestMixin(mixin_host) {}

GuestSessionMixin::~GuestSessionMixin() = default;

void GuestSessionMixin::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kGuestSession);
  command_line->AppendSwitch(::switches::kIncognito);
  command_line->AppendSwitchASCII(switches::kLoginProfile, "hash");
  command_line->AppendSwitchASCII(
      switches::kLoginUser, user_manager::GuestAccountId().GetUserEmail());
}

}  // namespace chromeos
