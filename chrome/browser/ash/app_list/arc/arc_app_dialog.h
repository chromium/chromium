// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_DIALOG_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_DIALOG_H_

#include <string>

#include "base/functional/callback.h"

class Profile;

namespace arc {

using ArcUsbConfirmCallback = base::OnceCallback<void(bool)>;

// Shows permission request dialog for scan USB device list.
void ShowUsbScanDeviceListPermissionDialog(Profile* profile,
                                           const std::string& app_id,
                                           ArcUsbConfirmCallback callback);

// Shows permission request dialog for targeting device name.
void ShowUsbAccessPermissionDialog(Profile* profile,
                                   const std::string& app_id,
                                   const std::u16string& device_name,
                                   ArcUsbConfirmCallback callback);

// Test purpose methods.
bool IsArcAppDialogViewAliveForTest();

bool CloseAppDialogViewAndConfirmForTest(bool confirm);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_DIALOG_H_
