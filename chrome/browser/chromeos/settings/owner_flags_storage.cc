// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/owner_flags_storage.h"

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/about_flags.h"
#include "chrome/common/pref_names.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/flags_ui/flags_ui_pref_names.h"
#include "components/ownership/owner_settings_service.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace about_flags {

OwnerFlagsStorage::OwnerFlagsStorage(
    PrefService* prefs,
    ownership::OwnerSettingsService* owner_settings_service)
    : flags_ui::PrefServiceFlagsStorage(prefs),
      owner_settings_service_(owner_settings_service) {}

OwnerFlagsStorage::~OwnerFlagsStorage() {}

bool OwnerFlagsStorage::SetFlags(const std::set<std::string>& flags) {
  PrefServiceFlagsStorage::SetFlags(flags);

  base::ListValue experiments_list;

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  ::about_flags::ConvertFlagsToSwitches(this, &command_line,
                                        flags_ui::kNoSentinels);
  base::CommandLine::StringVector switches = command_line.argv();
  for (base::CommandLine::StringVector::const_iterator it =
           switches.begin() + 1;
       it != switches.end(); ++it) {
    experiments_list.AppendString(*it);
  }
  owner_settings_service_->Set(kStartUpFlags, experiments_list);

  return true;
}

}  // namespace about_flags
}  // namespace chromeos
