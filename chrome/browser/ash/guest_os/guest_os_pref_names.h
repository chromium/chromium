// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_PREF_NAMES_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_PREF_NAMES_H_

class PrefRegistrySimple;

namespace guest_os::prefs {

// Dictionary of filesystem paths mapped to the list of VMs that the paths are
// shared with.
inline constexpr char kGuestOSPathsSharedToVms[] =
    "guest_os.paths_shared_to_vms";

// Mapping of file extension to mime types for each VM/Container.
// Use of root 'crostini' is for historic reasons / backwards compatibility.
inline constexpr char kGuestOsMimeTypes[] = "crostini.mime_types";

// GuestOsRegistry and GuestId
inline constexpr char kVmTypeKey[] = "vm_type";
inline constexpr char kVmNameKey[] = "vm_name";
inline constexpr char kContainerNameKey[] = "container_name";

// Registry of installed app for each VM/container.
inline constexpr char kGuestOsRegistry[] = "crostini.registry";

// Keys for the |kGuestOsRegistry| Dictionary stored in prefs for each app.
inline constexpr char kAppDesktopFileIdKey[] = "desktop_file_id";
inline constexpr char kAppExtensionsKey[] = "extensions";
inline constexpr char kAppMimeTypesKey[] = "mime_types";
inline constexpr char kAppKeywordsKey[] = "keywords";
inline constexpr char kAppExecKey[] = "exec";
inline constexpr char kAppExecutableFileNameKey[] = "executable_file_name";
inline constexpr char kAppNameKey[] = "name";
inline constexpr char kAppNoDisplayKey[] = "no_display";
inline constexpr char kAppTerminalKey[] = "terminal";
inline constexpr char kAppScaledKey[] = "scaled";
inline constexpr char kAppPackageIdKey[] = "package_id";
inline constexpr char kAppStartupWMClassKey[] = "startup_wm_class";
inline constexpr char kAppStartupNotifyKey[] = "startup_notify";
inline constexpr char kAppInstallTimeKey[] = "install_time";
inline constexpr char kAppLastLaunchTimeKey[] = "last_launch_time";

// GuestId
inline constexpr char kGuestOsContainers[] = "crostini.containers";
inline constexpr char kContainerCreateOptions[] = "crostini_create_options";
inline constexpr char kContainerOsVersionKey[] = "container_os_version";
inline constexpr char kContainerOsPrettyNameKey[] = "container_os_pretty_name";
// SkColor used to assign badges to apps associated with this container.
inline constexpr char kContainerColorKey[] = "badge_color";
// Whether or not this guest should show up in the terminal app.
inline constexpr char kTerminalSupportedKey[] = "terminal_supported";
// The display name to use in the terminal.
inline constexpr char kTerminalLabel[] = "terminal_label";
// Should the terminal show the disabled by enterprise policy icon.
inline constexpr char kTerminalPolicyDisabled[] = "terminal_policy_disabled";
inline constexpr char kContainerSharedVmDevicesKey[] =
    "container_shared_vm_devices";
inline constexpr char kBruschettaConfigId[] = "bruschetta_config_id";

// Terminal
// Dictionary of terminal UI settings such as font style, colors, etc.
inline constexpr char kGuestOsTerminalSettings[] = "crostini.terminal_settings";

inline constexpr char kGuestOsUSBNotificationEnabled[] =
    "guest_os.usb_notification_enabled";
inline constexpr char kGuestOsUSBPersistentPassthroughEnabled[] =
    "guest_os.usb_persistent_passthrough_enabled";
inline constexpr char kGuestOsUSBPersistentPassthroughDevices[] =
    "guest_os.usb_persistent_passthrough_devices";

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace guest_os::prefs

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_PREF_NAMES_H_
