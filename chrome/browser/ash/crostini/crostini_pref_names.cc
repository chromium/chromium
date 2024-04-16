// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_pref_names.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/values.h"
#include "components/guest_os/guest_os_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace crostini {
namespace prefs {

// A boolean preference representing whether a user has opted in to use
// Crostini (Called "Linux Apps" in UI).
const char kCrostiniEnabled[] = "crostini.enabled";
// A list representing the intent to create containers with
// certain specifications. This will also store whether or not it's been
// successfully run. If so, it will only remain as a historical record.

// Keys for the kCrostiniCreateOptions Dictionary stored for each container
// CreateOptions.
const char kCrostiniCreateOptionsSharePathsKey[] = "share_paths";
const char kCrostiniCreateOptionsContainerUsernameKey[] = "container_username";
const char kCrostiniCreateOptionsDiskSizeBytesKey[] = "disk_size_bytes";
const char kCrostiniCreateOptionsImageServerUrlKey[] = "image_server_url";
const char kCrostiniCreateOptionsImageAliasKey[] = "image_alias";
const char kCrostiniCreateOptionsAnsiblePlaybookKey[] = "ansible_playbook";
const char kCrostiniCreateOptionsUsedKey[] = "used";

// List of USB devices with their system guid, a name/description and their
// enabled state for use with Crostini.
const char kCrostiniSharedUsbDevices[] = "crostini.shared_usb_devices";
// Boolean preferences indicating whether Crostini is allowed to use mic.
const char kCrostiniMicAllowed[] = "crostini.mic_allowed";

// A boolean preference representing a user level enterprise policy to enable
// Crostini use.
const char kUserCrostiniAllowedByPolicy[] = "crostini.user_allowed_by_policy";
// A boolean preference representing a user level enterprise policy to enable
// the crostini export / import UI.
const char kUserCrostiniExportImportUIAllowedByPolicy[] =
    "crostini.user_export_import_ui_allowed_by_policy";
// A boolean preference representing a user level enterprise policy to enable
// VM management CLI.
const char kVmManagementCliAllowedByPolicy[] =
    "crostini.vm_management_cli_allowed_by_policy";
// A boolean preference representing a user level enterprise policy to allow
// Crostini root access in the default Crostini VM.
// TODO(crbug.com/41470758): The features that have to be implemented.
const char kUserCrostiniRootAccessAllowedByPolicy[] =
    "crostini.user_root_access_allowed_by_policy";
// A file path preference representing a user level enterprise policy that
// specifies Ansible playbook to be applied to the default Crostini container.
// Value is empty when there is no playbook to be applied specified though
// policy.
const char kCrostiniAnsiblePlaybookFilePath[] =
    "crostini.ansible_playbook_file_path";
// A boolean preference representing whether default Crostini container is
// successfully configured by kCrostiniAnsiblePlaybook user policy.
const char kCrostiniDefaultContainerConfigured[] =
    "crostini.default_container_configured";
// A boolean preference representing a user level enterprise policy to allow
// port forwarding into Crostini.
const char kCrostiniPortForwardingAllowedByPolicy[] =
    "crostini.port_forwarding_allowed_by_policy";
// A boolean preference representing a user level enterprise policy to allow
// SSH in Terminal System App.
const char kTerminalSshAllowedByPolicy[] =
    "crostini.terminal_ssh_allowed_by_policy";

// A boolean preference controlling Crostini usage reporting.
const char kReportCrostiniUsageEnabled[] = "crostini.usage_reporting_enabled";
// Preferences used to store last launch information for reporting:
// Last launch Termina component version.
const char kCrostiniLastLaunchTerminaComponentVersion[] =
    "crostini.last_launch.version";
// Last launch Termina kernel version.
const char kCrostiniLastLaunchTerminaKernelVersion[] =
    "crostini.last_launch.vm_kernel_version";
// The start of a three day window of the last app launch
// stored as Java time (ms since epoch).
const char kCrostiniLastLaunchTimeWindowStart[] =
    "crostini.last_launch.time_window_start";
// The value of the last sample of the disk space used by Crostini.
const char kCrostiniLastDiskSize[] = "crostini.last_disk_size";
// A dictionary preference representing a user's settings of forwarded ports
// to Crostini.
const char kCrostiniPortForwarding[] = "crostini.port_forwarding.ports";

// An integer preference indicating the allowance policy for ADB sideloading,
// with 0 meaning disallowed and 1 meaning allowed
const char kCrostiniArcAdbSideloadingUserPref[] =
    "crostini.arc_adb_sideloading.user_pref";

// Prefix for storing engagement time data, per GuestOSEngagementMetrics.
const char kEngagementPrefsPrefix[] = "crostini.metrics";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kCrostiniEnabled, false);
  registry->RegisterListPref(kCrostiniPortForwarding);
  registry->RegisterListPref(kCrostiniSharedUsbDevices);
  registry->RegisterBooleanPref(kCrostiniMicAllowed, false);
  registry->RegisterBooleanPref(kTerminalSshAllowedByPolicy, true);
  registry->RegisterBooleanPref(crostini::prefs::kReportCrostiniUsageEnabled,
                                false);
  registry->RegisterStringPref(kCrostiniLastLaunchTerminaComponentVersion,
                               std::string());
  registry->RegisterStringPref(kCrostiniLastLaunchTerminaKernelVersion,
                               std::string());
  registry->RegisterInt64Pref(kCrostiniLastLaunchTimeWindowStart, 0u);
  registry->RegisterInt64Pref(kCrostiniLastDiskSize, 0u);
  registry->RegisterBooleanPref(kUserCrostiniAllowedByPolicy, true);
  registry->RegisterBooleanPref(kUserCrostiniExportImportUIAllowedByPolicy,
                                true);
  registry->RegisterBooleanPref(kVmManagementCliAllowedByPolicy, true);
  registry->RegisterBooleanPref(kUserCrostiniRootAccessAllowedByPolicy, true);
  registry->RegisterBooleanPref(kCrostiniPortForwardingAllowedByPolicy, true);
  registry->RegisterFilePathPref(kCrostiniAnsiblePlaybookFilePath,
                                 base::FilePath());
  registry->RegisterBooleanPref(kCrostiniDefaultContainerConfigured, false);

  registry->RegisterIntegerPref(
      kCrostiniArcAdbSideloadingUserPref,
      static_cast<int>(CrostiniArcAdbSideloadingUserAllowanceMode::kDisallow));

  guest_os::prefs::RegisterEngagementProfilePrefs(registry,
                                                  kEngagementPrefsPrefix);
}

}  // namespace prefs
}  // namespace crostini
