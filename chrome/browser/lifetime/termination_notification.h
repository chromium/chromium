// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_TERMINATION_NOTIFICATION_H_
#define CHROME_BROWSER_LIFETIME_TERMINATION_NOTIFICATION_H_

#include "build/chromeos_buildflags.h"

namespace browser_shutdown {

// Emits APP_TERMINATING notification. It is guaranteed that the
// notification is sent only once.
void NotifyAppTerminating();

// Send out notifications.
// For ChromeOS, also request session manager to end the session.
// |reboot_policy| is used to signal whether or not a reboot should be forced.
// By default, the functions only reboot the system if an update is available.
// When a component flash update is present, but not a system update, the
// kForceReboot flag is passed.
enum class RebootPolicy { kForceReboot, kOptionalReboot };
void NotifyAndTerminate(bool fast_path);
void NotifyAndTerminate(bool fast_path, RebootPolicy reboot_policy);

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool UpdatePending();
#endif

}  // namespace browser_shutdown

#endif  // CHROME_BROWSER_LIFETIME_TERMINATION_NOTIFICATION_H_
