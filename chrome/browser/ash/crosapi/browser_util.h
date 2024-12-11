// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_

#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
#include "chromeos/ash/components/standalone_browser/lacros_selection.h"

namespace aura {
class Window;
}  // namespace aura

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace component_updater {
class ComponentUpdateService;
}  // namespace component_updater

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace policy {
class PolicyMap;
}  // namespace policy

// These methods are used by ash-chrome.
namespace crosapi::browser_util {

// Indicates how the decision for the usage of Lacros has been made.
enum class LacrosLaunchSwitchSource {
  // It is unknown yet if and how Lacros will be used.
  kUnknown = 0,
  // Either there were no policies, or the system had a special condition in
  // which the policy got ignored and the user could have set the mode.
  kPossiblySetByUser = 1,
  // The Lacros usage was enforced by the user via #lacros-availability-ignore
  // flag override.
  kForcedByUser = 2,
  // The Lacros usage was enforced using the policy. Note that in this case
  // the policy might still not be used, but it is programmatically overridden
  // and not by the user (e.g. special Googler user case).
  kForcedByPolicy = 3
};

// Returns the user directory for lacros-chrome.
base::FilePath GetUserDataDir();

// Returns true if the Lacros feature is enabled for the primary user.
bool IsLacrosEnabled();

// Returns true if Ash browser is enabled. Returns false iff Lacros is
// enabled and is the only browser.
// DEPRECATED. Please use !IsLacrosEnabled().
bool IsAshWebBrowserEnabled();

// Returns true if |window| is an exo ShellSurface window representing a Lacros
// browser.
bool IsLacrosWindow(const aura::Window* window);

// Gets the version of the rootfs lacros-chrome. By reading the metadata json
// file in the correct format.
base::Version GetRootfsLacrosVersionMayBlock(
    const base::FilePath& version_file_path);

// To be called at primary user login, to cache the policy value for lacros
// availability.
void CacheLacrosAvailability(const policy::PolicyMap& map);

// Returns the update channel associated with the given loaded lacros selection.
version_info::Channel GetLacrosSelectionUpdateChannel(
    ash::standalone_browser::LacrosSelection selection);

// Returns the currently installed version of lacros-chrome managed by the
// component updater. Will return an empty / invalid version if no lacros
// component is found.
base::Version GetInstalledLacrosComponentVersion(
    const component_updater::ComponentUpdateService* component_update_service);

// Clears the cached values for lacros availability policy.
void ClearLacrosAvailabilityCacheForTest();

// Returns who decided how Lacros should be used - or not: The User, the policy
// or another edge case.
LacrosLaunchSwitchSource GetLacrosLaunchSwitchSource();

}  // namespace crosapi::browser_util

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_UTIL_H_
