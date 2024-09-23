// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_UTIL_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_UTIL_H_

#include <optional>

#include "base/files/file_path.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_id.h"

class Profile;

namespace bruschetta {

extern const char kToolsDlc[];
extern const char kUefiDlc[];

extern const char kBruschettaVmName[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. BruschettaResult in
// tools/metrics/histograms/enums.xml must be updated when making a change to
// this enum.
enum class BruschettaResult {
  kUnknown = 0,
  kSuccess = 1,
  kDlcInstallError = 2,
  // Deprecated: kBiosNotAccessible = 3
  kStartVmFailed = 4,
  kTimeout = 5,
  kForbiddenByPolicy = 6,
  kConciergeUnavailable = 7,
  kMaxValue = kConciergeUnavailable,
};

// The launch-time policy that applies to a specific VM. This is used to
// decide if we need to force a VM to shutdown after it's policy changed.
struct RunningVmPolicy {
  bool vtpm_enabled;
};

using InstallableConfig = std::pair<std::string, base::Value::Dict>;

// Returns the string name of the BruschettaResult.
const char* BruschettaResultString(const BruschettaResult res);

guest_os::GuestId GetBruschettaAlphaId();

guest_os::GuestId MakeBruschettaId(std::string vm_name);

std::optional<const base::Value::Dict*> GetRunnableConfig(
    const Profile* profile,
    const std::string& config_id);

base::FilePath BruschettaChromeOSBaseDirectory();

std::optional<const base::Value::Dict*> GetInstallableConfig(
    const Profile* profile,
    const std::string& config_id);

base::flat_map<std::string, base::Value::Dict> GetInstallableConfigs(
    const Profile* profile);

// In-place sort installable configs into display order.
void SortInstallableConfigs(std::vector<InstallableConfig>* configs);

// Returns true if an installable config for Bruschetta is present in the
// enterprise policy. (e.g. kBruschettaPolicyId)
bool HasInstallableConfig(const Profile* profile, const std::string& config_id);

// Returns true if Bruschetta is installed.
bool IsInstalled(Profile* profile, const guest_os::GuestId& guest_id);

std::optional<const base::Value::Dict*> GetConfigForGuest(
    Profile* profile,
    const guest_os::GuestId& guest_id,
    prefs::PolicyEnabledState enabled_level);

std::optional<RunningVmPolicy> GetLaunchPolicyForConfig(Profile* profile,
                                                        std::string config_id);

std::string GetVmUsername(const Profile* profile);

// Gets the overall VM Name (i.e. *not* the name of a specific installed VM or
// configuration which we more commonly use throughout the UI), to be used for
// e.g. the installer UI before we know which configuration will be installed.
std::u16string GetOverallVmName(Profile* profile);

// Gets a URL to learn more about the feature, supplied in policy so an
// enterprise can document their specific VM. Returns an empty GURL if not set.
GURL GetLearnMoreUrl(Profile* profile);

// Gets the display name of the specified `guest` running under `profile`.
std::string GetDisplayName(Profile* profile, guest_os::GuestId guest);

// Returns whether the default Bruschetta VM is running for the user.
bool IsBruschettaRunning(Profile* profile);

// Gets the display name for the default Bruschetta VM.
std::string GetBruschettaDisplayName(Profile* profile);

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_UTIL_H_
