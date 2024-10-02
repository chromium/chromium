// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_util.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace bruschetta {

namespace {
std::optional<const base::Value::Dict*> GetConfigWithEnabledLevel(
    const Profile* profile,
    const std::string& config_id,
    prefs::PolicyEnabledState enabled_level) {
  // If virtual machines are disabled, we should treat every policy as
  // BLOCKED. If the caller is looking for an enabled level of RUN_ALLOWED
  // or higher we should return nothing, but it can still be useful in
  // some places to retrieve a config even if it's currently BLOCKED e.g. for
  // display names.
  if (!virtual_machines::AreVirtualMachinesAllowedByPolicy() &&
      enabled_level > prefs::PolicyEnabledState::BLOCKED) {
    return std::nullopt;
  }

  const auto* config_ptr = profile->GetPrefs()
                               ->GetDict(prefs::kBruschettaVMConfiguration)
                               .FindDict(config_id);
  if (!config_ptr || config_ptr->FindInt(prefs::kPolicyEnabledKey) <
                         static_cast<int>(enabled_level)) {
    return std::nullopt;
  }

  return config_ptr;
}
}  // namespace

const char kToolsDlc[] = "termina-tools-dlc";
const char kUefiDlc[] = "edk2-ovmf-dlc";

const char kBruschettaVmName[] = "bru";

const char* BruschettaResultString(const BruschettaResult res) {
#define ENTRY(name)            \
  case BruschettaResult::name: \
    return #name
  switch (res) {
    ENTRY(kUnknown);
    ENTRY(kSuccess);
    ENTRY(kDlcInstallError);
    ENTRY(kStartVmFailed);
    ENTRY(kTimeout);
    ENTRY(kForbiddenByPolicy);
    ENTRY(kConciergeUnavailable);
  }
#undef ENTRY
  return "unknown code";
}

guest_os::GuestId GetBruschettaAlphaId() {
  return MakeBruschettaId(kBruschettaVmName);
}

guest_os::GuestId MakeBruschettaId(std::string vm_name) {
  return guest_os::GuestId{guest_os::VmType::BRUSCHETTA, std::move(vm_name),
                           "penguin"};
}

std::optional<const base::Value::Dict*> GetRunnableConfig(
    const Profile* profile,
    const std::string& config_id) {
  return GetConfigWithEnabledLevel(profile, config_id,
                                   prefs::PolicyEnabledState::RUN_ALLOWED);
}

base::FilePath BruschettaChromeOSBaseDirectory() {
  return base::FilePath("/mnt/shared");
}

std::optional<const base::Value::Dict*> GetInstallableConfig(
    const Profile* profile,
    const std::string& config_id) {
  return GetConfigWithEnabledLevel(profile, config_id,
                                   prefs::PolicyEnabledState::INSTALL_ALLOWED);
}

bool HasInstallableConfig(const Profile* profile,
                          const std::string& config_id) {
  return GetInstallableConfig(profile, config_id).has_value();
}

base::flat_map<std::string, base::Value::Dict> GetInstallableConfigs(
    const Profile* profile) {
  base::flat_map<std::string, base::Value::Dict> ret;
  for (auto it :
       profile->GetPrefs()->GetDict(prefs::kBruschettaVMConfiguration)) {
    if (HasInstallableConfig(profile, it.first)) {
      ret.emplace(it.first, it.second.GetDict().Clone());
    }
  }

  return ret;
}

void SortInstallableConfigs(std::vector<InstallableConfig>* configs) {
  auto GetDisplayOrder = [](const InstallableConfig& c) -> int {
    return c.second.FindInt(bruschetta::prefs::kPolicyDisplayOrderKey)
        .value_or(0);
  };
  std::sort(configs->begin(), configs->end(),
            [&GetDisplayOrder](const InstallableConfig& a,
                               const InstallableConfig& b) {
              return GetDisplayOrder(a) < GetDisplayOrder(b);
            });
}

bool IsInstalled(Profile* profile, const guest_os::GuestId& guest_id) {
  const base::Value* value = guest_os::GetContainerPrefValue(
      profile, guest_id, guest_os::prefs::kVmNameKey);
  return value != nullptr;
}

std::optional<RunningVmPolicy> GetLaunchPolicyForConfig(Profile* profile,
                                                        std::string config_id) {
  auto config_option = GetRunnableConfig(profile, config_id);
  if (!config_option.has_value()) {
    return std::nullopt;
  }
  const auto* config = *config_option;

  RunningVmPolicy ret = {.vtpm_enabled =
                             config->FindDict(prefs::kPolicyVTPMKey)
                                 ->FindBool(prefs::kPolicyVTPMEnabledKey)
                                 .value()};

  return ret;
}

std::string GetVmUsername(const Profile* profile) {
  std::string username = profile->GetProfileUserName();
  // Return the part before the '@' if this is an email. Since find returns
  // std::string::npos if it can't find the token this will return the full
  // username in that case.
  return username.substr(0, username.find("@"));
}

std::optional<const base::Value::Dict*> GetConfigForGuest(
    Profile* profile,
    const guest_os::GuestId& guest_id,
    prefs::PolicyEnabledState enabled_level) {
  const auto* config_id_val = guest_os::GetContainerPrefValue(
      profile, guest_id, guest_os::prefs::kBruschettaConfigId);
  if (!config_id_val) {
    return std::nullopt;
  }

  const auto& config_id = config_id_val->GetString();

  return GetConfigWithEnabledLevel(profile, config_id, enabled_level);
}

std::u16string GetOverallVmName(Profile* profile) {
  const std::u16string fallback_name =
      l10n_util::GetStringUTF16(IDS_BRUSCHETTA_NAME);
  if (!profile) {
    // If no profile is present (e.g. some tests), we can't access the policy.
    return fallback_name;
  }

  // First see if the name is explicitly set in policy.
  const auto& installer_config =
      profile->GetPrefs()->GetDict(prefs::kBruschettaInstallerConfiguration);
  const auto* display_name =
      installer_config.FindString(prefs::kPolicyDisplayNameKey);
  if (display_name) {
    return base::UTF8ToUTF16(*display_name);
  }

  // If not, pick the name of the first VM in configuration.
  std::vector<bruschetta::InstallableConfig> configs =
      bruschetta::GetInstallableConfigs(profile).extract();
  if (!configs.empty()) {
    SortInstallableConfigs(&configs);
    const base::Value::Dict& first_vm = configs[0].second;
    const auto* name = first_vm.FindString(prefs::kPolicyNameKey);
    if (name) {
      return base::UTF8ToUTF16(*name);
    }
    // Config exists but no name, use the key as its name.
    return base::UTF8ToUTF16(configs[0].first);
  }
  return fallback_name;
}

GURL GetLearnMoreUrl(Profile* profile) {
  // First see if the name is explicitly set in policy.
  const auto& installer_config =
      profile->GetPrefs()->GetDict(prefs::kBruschettaInstallerConfiguration);
  const auto* url = installer_config.FindString(prefs::kPolicyLearnMoreUrlKey);
  if (url) {
    return GURL(*url);
  }
  return GURL();
}

std::string GetDisplayName(Profile* profile, guest_os::GuestId guest) {
  auto config =
      GetConfigForGuest(profile, guest, prefs::PolicyEnabledState::BLOCKED);
  const std::string* name = nullptr;
  if (config.has_value()) {
    name = config.value()->FindString(prefs::kPolicyNameKey);
  }
  if (name) {
    return *name;
  }
  // If the config doesn't exist, the terminal will default to
  // <vm_name>:<container_name>, but container_name isn't meaningful for us
  // so just use the vm_name instead.
  return guest.vm_name;
}

bool IsBruschettaRunning(Profile* profile) {
  auto* service = bruschetta::BruschettaServiceFactory::GetForProfile(profile);
  return service && service->IsVmRunning(kBruschettaVmName);
}

std::string GetBruschettaDisplayName(Profile* profile) {
  return GetDisplayName(profile, GetBruschettaAlphaId());
}

}  // namespace bruschetta
