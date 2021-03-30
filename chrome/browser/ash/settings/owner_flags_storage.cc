// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/owner_flags_storage.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/about_flags.h"
#include "chrome/common/pref_names.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_pref_names.h"
#include "components/ownership/owner_settings_service.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"

namespace ash {
namespace about_flags {

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
  std::vector<base::Value> feature_flags_list;
  for (const auto& flag : flags) {
    feature_flags_list.push_back(base::Value(flag));
  }
  owner_settings_service_->Set(kFeatureFlags,
                               base::Value(std::move(feature_flags_list)));

  return true;
}

ReadOnlyFlagsStorage::ReadOnlyFlagsStorage(const std::set<std::string>& flags)
    : flags_(flags) {}

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
  return std::string();
}

void ReadOnlyFlagsStorage::SetOriginListFlag(
    const std::string& internal_entry_name,
    const std::string& origin_list_value) {}

std::set<std::string> ParseFlagsFromCommandLine() {
  std::string encoded =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          chromeos::switches::kFeatureFlags);
  if (encoded.empty()) {
    return {};
  }

  auto flags_list = base::JSONReader::Read(encoded);
  if (!flags_list) {
    LOG(WARNING) << "Failed to parse feature flags configuration";
    return {};
  }

  std::set<std::string> flags;
  for (const auto& flag : flags_list.value().GetList()) {
    if (!flag.is_string()) {
      LOG(WARNING) << "Invalid entry in encoded feature flags";
      continue;
    }
    flags.insert(flag.GetString());
  }

  return flags;
}

}  // namespace about_flags
}  // namespace ash
