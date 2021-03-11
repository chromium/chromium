// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_OWNER_FLAGS_STORAGE_H_
#define CHROME_BROWSER_ASH_SETTINGS_OWNER_FLAGS_STORAGE_H_

#include "base/compiler_specific.h"
#include "components/flags_ui/pref_service_flags_storage.h"

namespace ownership {
class OwnerSettingsService;
}

namespace ash {

namespace about_flags {

// Implements the FlagsStorage interface for the owner flags. It inherits from
// PrefServiceFlagsStorage but extends it with storing the flags in the signed
// device settings as well which effectively applies them to the Chrome OS login
// screen as well.
class OwnerFlagsStorage : public ::flags_ui::PrefServiceFlagsStorage {
 public:
  OwnerFlagsStorage(PrefService* prefs,
                    ownership::OwnerSettingsService* owner_settings_service);
  ~OwnerFlagsStorage() override;

  bool SetFlags(const std::set<std::string>& flags) override;

 private:
  ownership::OwnerSettingsService* owner_settings_service_;
};

// FlagsStorage implementation for Chrome OS startup. It is backed by a set of
// flags that are provided at initialization time. Functions other than
// GetFlags() are implemented as no-ops.
class ReadOnlyFlagsStorage : public ::flags_ui::FlagsStorage {
 public:
  // Initializes the object with a given set of flags.
  explicit ReadOnlyFlagsStorage(const std::set<std::string>& flags);
  ~ReadOnlyFlagsStorage() override;

  // ::flags_ui::FlagsStorage:
  std::set<std::string> GetFlags() const override;
  bool SetFlags(const std::set<std::string>& flags) override;
  void CommitPendingWrites() override;
  std::string GetOriginListFlag(
      const std::string& internal_entry_name) const override;
  void SetOriginListFlag(const std::string& internal_entry_name,
                         const std::string& origin_list_value) override;

 private:
  std::set<std::string> flags_;
};

// Parses flags specified in the --feature-flags command line switch. This is
// used by Chrome OS to pass flags configuration from session_manager to Chrome
// on startup.
std::set<std::string> ParseFlagsFromCommandLine();

}  // namespace about_flags
}  // namespace ash

// TODO(https://crbug.com/1164001): remove when Chrome OS code migration is
// done.
namespace chromeos {
namespace about_flags {
using ::ash::about_flags::OwnerFlagsStorage;
}  // namespace about_flags
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_SETTINGS_OWNER_FLAGS_STORAGE_H_
