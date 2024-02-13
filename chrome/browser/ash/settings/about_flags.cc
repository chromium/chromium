// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/about_flags.h"

#include <string_view>

#include "base/check.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/site_isolation/about_flags.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
#include "components/account_id/account_id.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_pref_names.h"
#include "components/ownership/owner_settings_service.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"

namespace ash {
namespace about_flags {

namespace {

std::set<std::string> ParseFlagsFromCommandLine(
    base::CommandLine* command_line) {
  std::set<std::string> flags;
  std::string encoded =
      command_line->GetSwitchValueNative(chromeos::switches::kFeatureFlags);
  if (encoded.empty()) {
    return flags;
  }

  auto flags_list = base::JSONReader::Read(encoded);
  if (!flags_list) {
    LOG(WARNING) << "Failed to parse feature flags configuration";
    return flags;
  }

  for (const auto& flag : flags_list.value().GetList()) {
    if (!flag.is_string()) {
      LOG(WARNING) << "Invalid entry in encoded feature flags";
      continue;
    }
    flags.insert(flag.GetString());
  }
  return flags;
}

std::map<std::string, std::string> ParseOriginListFlagsFromCommmandLine(
    base::CommandLine* command_line) {
  std::map<std::string, std::string> origin_list_flags;
  std::string encoded = command_line->GetSwitchValueNative(
      chromeos::switches::kFeatureFlagsOriginList);
  if (encoded.empty()) {
    return origin_list_flags;
  }

  auto origin_list_flags_dict = base::JSONReader::Read(encoded);
  if (!origin_list_flags_dict) {
    LOG(WARNING) << "Failed to parse origin list configuration";
    return origin_list_flags;
  }

  for (const auto entry : origin_list_flags_dict->GetDict()) {
    if (!entry.second.is_string()) {
      LOG(WARNING) << "Invalid entry in encoded origin list flags";
      continue;
    }
    origin_list_flags[entry.first] = entry.second.GetString();
  }
  return origin_list_flags;
}

}  // namespace

OwnerFlagsStorage::OwnerFlagsStorage(
    PrefService* prefs,
    ownership::OwnerSettingsService* owner_settings_service)
    : flags_ui::PrefServiceFlagsStorage(prefs),
      owner_settings_service_(owner_settings_service) {}

OwnerFlagsStorage::~OwnerFlagsStorage() {}

bool OwnerFlagsStorage::SetFlags(const std::set<std::string>& flags) {
  // Write the flags configuration to profile preferences, which are used to
  // determine flags to apply when launching a user session.
  PrefServiceFlagsStorage::SetFlags(flags);

  // Also write the flags to device settings so they get applied to the Chrome
  // OS login screen. The device setting is read by session_manager and passed
  // to Chrome via a command line flag on startup.
  base::Value::List feature_flags_list;
  for (const auto& flag : flags) {
    feature_flags_list.Append(flag);
  }
  owner_settings_service_->Set(kFeatureFlags,
                               base::Value(std::move(feature_flags_list)));

  return true;
}

ReadOnlyFlagsStorage::ReadOnlyFlagsStorage(
    const std::set<std::string>& flags,
    const std::map<std::string, std::string>& origin_list_flags)
    : flags_(flags), origin_list_flags_(origin_list_flags) {}

ReadOnlyFlagsStorage::ReadOnlyFlagsStorage(base::CommandLine* command_line)
    : flags_(ParseFlagsFromCommandLine(command_line)),
      origin_list_flags_(ParseOriginListFlagsFromCommmandLine(command_line)) {}

ReadOnlyFlagsStorage::~ReadOnlyFlagsStorage() = default;

std::set<std::string> ReadOnlyFlagsStorage::GetFlags() const {
  return flags_;
}

bool ReadOnlyFlagsStorage::SetFlags(const std::set<std::string>& flags) {
  return false;
}

void ReadOnlyFlagsStorage::CommitPendingWrites() {}

std::string ReadOnlyFlagsStorage::GetOriginListFlag(
    const std::string& internal_entry_name) const {
  const auto& entry = origin_list_flags_.find(internal_entry_name);
  return entry != origin_list_flags_.end() ? entry->second : std::string();
}

void ReadOnlyFlagsStorage::SetOriginListFlag(
    const std::string& internal_entry_name,
    const std::string& origin_list_value) {}

std::string ReadOnlyFlagsStorage::GetStringFlag(
    const std::string& internal_entry_name) const {
  return GetOriginListFlag(internal_entry_name);
}

void ReadOnlyFlagsStorage::SetStringFlag(const std::string& internal_entry_name,
                                         const std::string& string_value) {}

FeatureFlagsUpdate::FeatureFlagsUpdate(
    const ::flags_ui::FlagsStorage& flags_storage,
    PrefService* profile_prefs) {
  flags_ = flags_storage.GetFlags();
  ApplyUserPolicyToFlags(profile_prefs, &flags_);

  for (const auto& flag : flags_) {
    const auto origin_list_flag = flags_storage.GetOriginListFlag(flag);
    if (!origin_list_flag.empty()) {
      origin_list_flags_[flag] = origin_list_flag;
    }
  }
}

FeatureFlagsUpdate::~FeatureFlagsUpdate() = default;

bool FeatureFlagsUpdate::DiffersFromCommandLine(
    base::CommandLine* cmdline,
    std::set<std::string>* flags_difference) {
  flags_difference->clear();

  const auto cmdline_flags = ParseFlagsFromCommandLine(cmdline);
  std::set_symmetric_difference(
      flags_.begin(), flags_.end(), cmdline_flags.begin(), cmdline_flags.end(),
      std::inserter(*flags_difference, flags_difference->begin()));

  auto lookup = [](const std::map<std::string, std::string>& origin_list_flags,
                   const std::string& key) {
    const auto entry = origin_list_flags.find(key);
    return entry == origin_list_flags.end() ? std::nullopt
                                            : std::make_optional(entry->second);
  };
  const auto cmdline_origin_list_flags =
      ParseOriginListFlagsFromCommmandLine(cmdline);
  for (const auto& flag : flags_) {
    if (lookup(origin_list_flags_, flag) !=
        lookup(cmdline_origin_list_flags, flag)) {
      flags_difference->insert(flag);
    }
  }

  return !flags_difference->empty();
}

void FeatureFlagsUpdate::UpdateSessionManager() {
  // TODO(crbug.com/832857): Introduce a CHECK to ensure primary user.
  // Early out so that switches for secondary users are not applied to the whole
  // session. This could be removed when things like flags UI of secondary users
  // are fixed properly and TODO above to add CHECK() is done.
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* primary_user = user_manager->GetPrimaryUser();
  if (!primary_user || primary_user != user_manager->GetActiveUser())
    return;

  std::set<std::string> flags = flags_;

  // If LacrosAvailability policy is set, inject it into the feature flag,
  // so that the value is preserved on restarting the Chrome.
  // This is a kind of pseudo feature flag, so do not apply it in
  // ApplyUserPolicyToFlags to store in |flags_|, otherwise the value will
  // be used to decide whether or not to reboot to apply feature flags.
  const PrefService::Preference* lacros_launch_switch_pref =
      g_browser_process->local_state()->FindPreference(
          ::prefs::kLacrosLaunchSwitch);
  if (lacros_launch_switch_pref->IsManaged()) {
    // If there's the value, convert it into the feature name.
    std::string_view value =
        ash::standalone_browser::GetLacrosAvailabilityPolicyName(
            static_cast<ash::standalone_browser::LacrosAvailability>(
                lacros_launch_switch_pref->GetValue()->GetInt()));
    DCHECK(!value.empty())
        << "The unexpect value is set to LacrosAvailability: "
        << lacros_launch_switch_pref->GetValue()->GetInt();
    auto* entry = ::about_flags::GetCurrentFlagsState()->FindFeatureEntryByName(
        ash::standalone_browser::kLacrosAvailabilityPolicyInternalName);
    DCHECK(entry);
    int index;
    for (index = 0; index < entry->NumOptions(); ++index) {
      if (value == entry->ChoiceForOption(index).command_line_value)
        break;
    }
    if (static_cast<size_t>(index) != entry->choices.size()) {
      LOG(ERROR) << "Updating the lacros_availability: " << index;
      flags.insert(entry->NameForOption(index));
    }
  }

  const PrefService::Preference* lacros_data_backward_migration_mode_pref =
      g_browser_process->local_state()->FindPreference(
          ::prefs::kLacrosDataBackwardMigrationMode);
  if (lacros_data_backward_migration_mode_pref->IsManaged()) {
    auto value =
        lacros_data_backward_migration_mode_pref->GetValue()->GetString();
    auto* entry = ::about_flags::GetCurrentFlagsState()->FindFeatureEntryByName(
        crosapi::browser_util::
            kLacrosDataBackwardMigrationModePolicyInternalName);
    DCHECK(entry);
    int index;
    for (index = 0; index < entry->NumOptions(); ++index) {
      if (value == entry->ChoiceForOption(index).command_line_value)
        break;
    }
    if (static_cast<size_t>(index) != entry->choices.size()) {
      LOG(ERROR) << "Updating the lacros_data_backward_migration_mode: "
                 << index;
      flags.insert(entry->NameForOption(index));
    }
  }

  auto account_id = cryptohome::CreateAccountIdentifierFromAccountId(
      primary_user->GetAccountId());
  SessionManagerClient::Get()->SetFeatureFlagsForUser(
      account_id, {flags.begin(), flags.end()}, origin_list_flags_);
}

// static
void FeatureFlagsUpdate::ApplyUserPolicyToFlags(PrefService* user_profile_prefs,
                                                std::set<std::string>* flags) {
  // Get target value for --site-per-process for the user session according to
  // policy. If it is supposed to be enabled, make sure it can not be disabled
  // using flags-induced command-line switches.
  const PrefService::Preference* site_per_process_pref =
      user_profile_prefs->FindPreference(::prefs::kSitePerProcess);
  if (site_per_process_pref->IsManaged() &&
      site_per_process_pref->GetValue()->GetBool()) {
    flags->erase(::about_flags::SiteIsolationTrialOptOutChoiceEnabled());
  }
}

}  // namespace about_flags
}  // namespace ash
