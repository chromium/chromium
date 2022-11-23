// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_PREF_NAMES_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_PREF_NAMES_H_

class PrefRegistrySimple;

namespace guest_os {
namespace prefs {

// GuestOsSharedPath
extern const char kGuestOSPathsSharedToVms[];

// GuestOsMimeTypes
extern const char kGuestOsMimeTypes[];

// GuestOsRegistry and GuestId
extern const char kVmTypeKey[];
extern const char kVmNameKey[];
extern const char kContainerNameKey[];

// GuestOsRegistry
extern const char kGuestOsRegistry[];
extern const char kAppDesktopFileIdKey[];
extern const char kAppCommentKey[];
extern const char kAppExtensionsKey[];
extern const char kAppMimeTypesKey[];
extern const char kAppKeywordsKey[];
extern const char kAppExecKey[];
extern const char kAppExecutableFileNameKey[];
extern const char kAppNameKey[];
extern const char kAppNoDisplayKey[];
extern const char kAppScaledKey[];
extern const char kAppPackageIdKey[];
extern const char kAppStartupWMClassKey[];
extern const char kAppStartupNotifyKey[];
extern const char kAppInstallTimeKey[];
extern const char kAppLastLaunchTimeKey[];

// GuestId
extern const char kGuestOsContainers[];
extern const char kContainerCreateOptions[];
extern const char kContainerOsVersionKey[];
extern const char kContainerOsPrettyNameKey[];
extern const char kContainerColorKey[];
// Whether or not this guest should show up in the terminal app.
extern const char kTerminalSupportedKey[];
// The display name to use in the terminal.
extern const char kTerminalLabel[];
extern const char kContainerSharedVmDevicesKey[];
extern const char kBruschettaConfigId[];

// Terminal
// Dictionary of terminal UI settings such as font style, colors, etc.
extern const char kGuestOsTerminalSettings[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_PREF_NAMES_H_
