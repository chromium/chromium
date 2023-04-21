// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/oobe_quick_start_pref_names.h"

namespace ash::quick_start::prefs {

// *************** OOBE LOCAL STATE PREFS ***************

// Boolean pref whether Quick Start should automatically resume after the
// Chromebook reboots. This is true when a user initiates Quick Start, then
// installs an update before setting up their account.
const char kShouldResumeQuickStartAfterReboot[] =
    "oobe.should_resume_quick_start_after_reboot";

// Dict pref containing all info required to resume a Quick Start connection
// with the source device after the target device reboots.
const char kResumeQuickStartAfterRebootInfo[] =
    "oobe.resume_quick_start_after_reboot_info";

}  // namespace ash::quick_start::prefs
