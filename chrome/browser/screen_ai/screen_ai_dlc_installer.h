// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_SCREEN_AI_DLC_INSTALLER_H_
#define CHROME_BROWSER_SCREEN_AI_SCREEN_AI_DLC_INSTALLER_H_

#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

class PrefService;

namespace screen_ai::dlc_installer {

// If Screen AI library is needed, registers for installation of its DLC.
// If not and after some delay, uninstalls the DLC.
void ManageInstallation(PrefService* local_state);

// Requests installation of Screen AI DLC.
void Install();

}  // namespace screen_ai::dlc_installer

#endif  // CHROME_BROWSER_SCREEN_AI_SCREEN_AI_DLC_INSTALLER_H_
