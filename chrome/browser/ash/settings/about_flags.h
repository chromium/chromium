// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_ABOUT_FLAGS_H_
#define CHROME_BROWSER_ASH_SETTINGS_ABOUT_FLAGS_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "components/flags_ui/pref_service_flags_storage.h"

class PrefService;

namespace base {
class CommandLine;
}

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
  raw_ptr<ownership::OwnerSettingsService, DanglingUntriaged>
      owner_settings_service_;
};

// FlagsStorage implementation for Chrome OS startup. It is backed by a set of
// flags that are provided at initialization time. Functions other than
// GetFlags() are implemented as no-ops.
class ReadOnlyFlagsStorage : public ::flags_ui::FlagsStorage {
 public:
  ReadOnlyFlagsStorage(
      const std::set<std::string>& flags,
      const std::map<std::string, std::string>& origin_list_flags);
  // Parses flags specified in the --feature-flags and
  // --feature-flags-origin-list command line switches. This is used by Chrome
  // OS to pass flags configuration from session_manager to Chrome on startup.
  explicit ReadOnlyFlagsStorage(base::CommandLine* command_line);
  ~ReadOnlyFlagsStorage() override;

  // ::flags_ui::FlagsStorage:
  std::set<std::string> GetFlags() const override;
  bool SetFlags(const std::set<std::string>& flags) override;
  void CommitPendingWrites() override;
  std::string GetOriginListFlag(
      const std::string& internal_entry_name) const override;
  void SetOriginListFlag(const std::string& internal_entry_name,
                         const std::string& origin_list_value) override;
  std::string GetStringFlag(
      const std::string& internal_entry_name) const override;
  void SetStringFlag(const std::string& internal_entry_name,
                     const std::string& string_value) override;

 private:
  std::set<std::string> flags_;
  std::map<std::string, std::string> origin_list_flags_;
};

// A helper class to update Chrome OS session manager feature flags
// configuration from a provided FlagsStorage instance. This is meant to be used
// in the code paths that restart the browser to apply a different set of flags.
class FeatureFlagsUpdate {
 public:
  FeatureFlagsUpdate(const ::flags_ui::FlagsStorage& flags_storage,
                     PrefService* profile_prefs);
  ~FeatureFlagsUpdate();

  // Checks whether the flags configuration specified in |cmdline| is different.
  // |flags_difference| is filled in to indicate which flags differ.
  bool DiffersFromCommandLine(base::CommandLine* cmdline,
                              std::set<std::string>* flags_difference);

  // Updates session_manager with the flags configuration via D-Bus.
  void UpdateSessionManager();

 private:
  static void ApplyUserPolicyToFlags(PrefService* user_profile_prefs,
                                     std::set<std::string>* flags);

  std::set<std::string> flags_;
  std::map<std::string, std::string> origin_list_flags_;
};

}  // namespace about_flags
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SETTINGS_ABOUT_FLAGS_H_
