// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_CROS_SETTINGS_H_
#define CHROME_BROWSER_ASH_SETTINGS_CROS_SETTINGS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

static_assert(BUILDFLAG(IS_CHROMEOS_ASH), "For ChromeOS ash-chrome only");

class PrefService;

namespace ash {

class DeviceSettingsService;
class SupervisedUserCrosSettingsProvider;

// This class manages per-device/global settings.
class CrosSettings {
 public:
  // Manage singleton instance.
  static void Initialize(PrefService* local_state);
  static bool IsInitialized();
  static void Shutdown();
  static CrosSettings* Get();

  // Sets the singleton to |test_instance|. Does not take ownership of the
  // instance. Should be matched with a call to |ShutdownForTesting| once the
  // test is finished and before the instance is deleted.
  static void SetForTesting(CrosSettings* test_instance);
  static void ShutdownForTesting();

  // Creates an instance with no providers as yet. This is meant for unit tests,
  // production code uses the singleton returned by Get() above.
  CrosSettings();

  // Creates a device settings service instance. This is meant for unit tests,
  // production code uses the singleton returned by Get() above.
  CrosSettings(DeviceSettingsService* device_settings_service,
               PrefService* local_state);

  CrosSettings(const CrosSettings&) = delete;
  CrosSettings& operator=(const CrosSettings&) = delete;

  virtual ~CrosSettings();

  // Helper function to test if the given |path| is a valid cros setting.
  static bool IsCrosSettings(const std::string& path);

  // Returns setting value for the given |path|.
  const base::Value* GetPref(const std::string& path) const;

  // Requests that all providers ensure the values they are serving were read
  // from a trusted store:
  // * If all providers are serving trusted values, returns TRUSTED. This
  //   indicates that the cros settings returned by |this| can be trusted during
  //   the current loop cycle.
  // * If at least one provider ran into a permanent failure while trying to
  //   read values from its trusted store, returns PERMANENTLY_UNTRUSTED. This
  //   indicates that the cros settings will never become trusted.
  // * Otherwise, returns TEMPORARILY_UNTRUSTED. This indicates that at least
  //   one provider needs to read values from its trusted store first. The
  //   |callback| will be called back when the read is done.
  //   PrepareTrustedValues() should be called again at that point to determine
  //   whether all providers are serving trusted values now.
  virtual CrosSettingsProvider::TrustedStatus PrepareTrustedValues(
      base::OnceClosure callback) const;

  // These are convenience forms of Get().  The value will be retrieved
  // and the return value will be true if the |path| is valid and the value at
  // the end of the path can be returned in the form specified.
  bool GetBoolean(const std::string& path, bool* out_value) const;
  bool GetInteger(const std::string& path, int* out_value) const;
  bool GetDouble(const std::string& path, double* out_value) const;
  bool GetString(const std::string& path, std::string* out_value) const;
  bool GetList(const std::string& path,
               const base::Value::List** out_value) const;
  bool GetDictionary(const std::string& path,
                     const base::Value::Dict** out_value) const;

  // Checks if the given username is on the list of users allowed to sign-in to
  // this device. |wildcard_match| may be nullptr. If it's present, it'll be set
  // to true if the list check was satisfied via a wildcard. In some
  // configurations user can be allowed based on the |user_type|. See
  // |DeviceFamilyLinkAccountsAllowed| policy.
  bool IsUserAllowlisted(
      const std::string& username,
      bool* wildcard_match,
      const absl::optional<user_manager::UserType>& user_type) const;

  // Helper function for the allowlist op. Implemented here because we will need
  // this in a few places. The functions searches for |email| in the pref |path|
  // It respects allowlists so foo@bar.baz will match *@bar.baz too. If the
  // match was via a wildcard, |wildcard_match| is set to true.
  bool FindEmailInList(const std::string& path,
                       const std::string& email,
                       bool* wildcard_match) const;

  // Same as above, but receives already populated user list.
  static bool FindEmailInList(const base::Value::List& list,
                              const std::string& email,
                              bool* wildcard_match);

  // Adding/removing of providers.
  bool AddSettingsProvider(std::unique_ptr<CrosSettingsProvider> provider);
  std::unique_ptr<CrosSettingsProvider> RemoveSettingsProvider(
      CrosSettingsProvider* provider);

  // Add an observer Callback for changes for the given |path|.
  [[nodiscard]] base::CallbackListSubscription AddSettingsObserver(
      const std::string& path,
      base::RepeatingClosure callback);

  // Returns the provider that handles settings with the |path| or prefix.
  CrosSettingsProvider* GetProvider(const std::string& path) const;

  const SupervisedUserCrosSettingsProvider*
  supervised_user_cros_settings_provider() const {
    return supervised_user_cros_settings_provider_;
  }

 private:
  friend class CrosSettingsTest;

  // Fires system setting change callback.
  void FireObservers(const std::string& path);

  // List of ChromeOS system settings providers.
  std::vector<std::unique_ptr<CrosSettingsProvider>> providers_;

  // Owner unique pointer in |providers_|.
  raw_ptr<SupervisedUserCrosSettingsProvider>
      supervised_user_cros_settings_provider_;

  // A map from settings names to a list of observers. Observers get fired in
  // the order they are added.
  std::map<std::string, std::unique_ptr<base::RepeatingClosureList>>
      settings_observers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Helper class for tests. Initializes the CrosSettings singleton on
// construction and tears it down again on destruction.
class ScopedTestCrosSettings {
 public:
  explicit ScopedTestCrosSettings(PrefService* local_state);

  ScopedTestCrosSettings(const ScopedTestCrosSettings&) = delete;
  ScopedTestCrosSettings& operator=(const ScopedTestCrosSettings&) = delete;

  ~ScopedTestCrosSettings();
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when Chrome OS code migration is
// done.
namespace chromeos {
using ::ash::CrosSettings;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_SETTINGS_CROS_SETTINGS_H_
