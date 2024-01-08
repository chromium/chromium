// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_RECOVERY_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_RECOVERY_COMPONENT_INSTALLER_H_

class PrefRegistrySimple;
class PrefService;

namespace component_updater {

class ComponentUpdateService;

// Component update registration for the recovery component. The job of the
// recovery component is to repair the chrome installation or repair the Google
// update installation. This is a last resort safety mechanism.
void RegisterRecoveryComponent(ComponentUpdateService* cus, PrefService* prefs);

// Registers user preferences related to the recovery component.
void RegisterPrefsForRecoveryComponent(PrefRegistrySimple* registry);

// Notifies the recovery component that the user has accepted the elevation
// prompt. Clears the state of prefs::kRecoveryComponentNeedsElevation after the
// notification.
void AcceptedElevatedRecoveryInstall(PrefService* prefs);

// Notifies recovery component that the elevated install has been declined.
// Clears the flag prefs::kRecoveryComponentNeedsElevation.
void DeclinedElevatedRecoveryInstall(PrefService* prefs);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_RECOVERY_COMPONENT_INSTALLER_H_
