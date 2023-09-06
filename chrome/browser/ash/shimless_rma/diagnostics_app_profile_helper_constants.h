// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SHIMLESS_RMA_DIAGNOSTICS_APP_PROFILE_HELPER_CONSTANTS_H_
#define CHROME_BROWSER_ASH_SHIMLESS_RMA_DIAGNOSTICS_APP_PROFILE_HELPER_CONSTANTS_H_

#include "base/time/time.h"

namespace ash::shimless_rma {

// Polling interval and the timeout to wait for the extension being ready.
inline constexpr base::TimeDelta k3pDiagExtensionReadyPollingInterval =
    base::Milliseconds(50);
inline constexpr base::TimeDelta k3pDiagExtensionReadyPollingTimeout =
    base::Seconds(3);
// Error messages which are also used in unit tests.
inline constexpr char k3pDiagErrorNotChromeOSSystemExtension[] =
    "Extension %s is not a ChromeOS system extension.";
inline constexpr char k3pDiagErrorCannotActivateExtension[] =
    "Can't activate the extension. Extension's service worker is not "
    "registered.";
inline constexpr char k3pDiagErrorIWACannotHasPermissionPolicy[] =
    "\"permissions_policy\" is not allowed for IWA for Shimless RMA "
    "diagnostics app.";

}  // namespace ash::shimless_rma

#endif  // CHROME_BROWSER_ASH_SHIMLESS_RMA_DIAGNOSTICS_APP_PROFILE_HELPER_CONSTANTS_H_
