// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_PREF_NAMES_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_PREF_NAMES_H_

class PrefRegistrySimple;

namespace guest_os {
namespace prefs {

extern const char kCrostiniSharedPaths[];
extern const char kGuestOSPathsSharedToVms[];

extern const char kGuestOsRegistry[];
extern const char kAppDesktopFileIdKey[];
extern const char kAppVmTypeKey[];
extern const char kAppVmNameKey[];
extern const char kAppContainerNameKey[];
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

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_PREF_NAMES_H_
