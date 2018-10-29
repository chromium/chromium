// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_CROS_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_CROS_SETTINGS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"

class PrefService;

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace chromeos {

class DeviceSettingsService;
class StubCrosSettingsProvider;

// This class manages per-device/global settings.
class CrosSettings {
 public:
  // Manage singleton instance.
  static void Initialize(PrefService* local_state);
  static bool IsInitialized();
  static void Shutdown();
  static CrosSettings* Get();

  // Checks if the given username is whitelisted and allowed to sign-in to
  // this device. |wildcard_match| may be NULL. If it's present, it'll be set to
  // true if the whitelist check was satisfied via a wildcard.
  bool IsUserWhitelisted(const std::string& username,
                         bool* wildcard_match) const;

  // Creates a device settings service instance. This is meant for unit tests,
  // production code uses the singleton returned by Get() above.
  CrosSettings(DeviceSettingsService* device_settings_service,
               PrefService* local_state);
  virtual ~CrosSettings();

  // Helper function to test if the given |path| is a valid cros setting.
  static bool IsCrosSettings(const std::string& path);

  // Sets |in_value| to given |path| in cros settings.
  void Set(const std::string& path, const base::Value& in_value);

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
      const base::Closure& callback) const;

  // Convenience forms of Set().  These methods will replace any existing
  // value at that |path|, even if it has a different type.
  void SetBoolean(const std::string& path, bool in_value);
  void SetInteger(const std::string& path, int in_value);
  void SetDouble(const std::string& path, double in_value);
  void SetString(const std::string& path, const std::string& in_value);

  // Convenience functions for manipulating lists. Note that the following
  // functions employs a read, modify and write pattern. If underlying settings
  // provider updates its value asynchronously such as DeviceSettingsProvider,
  // value cache they read from might not be fresh and multiple calls to those
  // function would lose data. See http://crbug.com/127215
  void AppendToList(const std::string& path, const base::Value* value);
  void RemoveFromList(const std::string& path, const base::Value* value);

  // These are convenience forms of Get().  The value will be retrieved
  // and the return value will be true if the |path| is valid and the value at
  // the end of the path can be returned in the form specified.
  bool GetBoolean(const std::string& path, bool* out_value) const;
  bool GetInteger(const std::string& path, int* out_value) const;
  bool GetDouble(const std::string& path, double* out_value) const;
  bool GetString(const std::string& path, std::string* out_value) const;
  bool GetList(const std::string& path,
               const base::ListValue** out_value) const;
  bool GetDictionary(const std::string& path,
                     const base::DictionaryValue** out_value) const;

  // Helper function for the whitelist op. Implemented here because we will need
  // this in a few places. The functions searches for |email| in the pref |path|
  // It respects whitelists so foo@bar.baz will match *@bar.baz too. If the
  // match was via a wildcard, |wildcard_match| is set to true.
  bool FindEmailInList(const std::string& path,
                       const std::string& email,
                       bool* wildcard_match) const;

  // Same as above, but receives already populated user list.
  static bool FindEmailInList(const base::ListValue* list,
                              const std::string& email,
                              bool* wildcard_match);

  // Adding/removing of providers.
  bool AddSettingsProvider(std::unique_ptr<CrosSettingsProvider> provider);
  std::unique_ptr<CrosSettingsProvider> RemoveSettingsProvider(
      CrosSettingsProvider* provider);

  // Add an observer Callback for changes for the given |path|.
  using ObserverSubscription = base::CallbackList<void(void)>::Subscription;
  std::unique_ptr<ObserverSubscription> AddSettingsObserver(
      const std::string& path,
      const base::Closure& callback) WARN_UNUSED_RESULT;

  // Returns the provider that handles settings with the |path| or prefix.
  CrosSettingsProvider* GetProvider(const std::string& path) const;

  // Returns the StubCrosSettingsProvider. Returns |nullptr| unless the
  // kStubCrosSettings switch is set, which is only true during testing.
  StubCrosSettingsProvider* stubbed_provider_for_test() const {
    return stubbed_provider_ptr_;
  }

 private:
  friend class CrosSettingsTest;

  // Fires system setting change callback.
  void FireObservers(const std::string& path);

  // List of ChromeOS system settings providers.
  std::vector<std::unique_ptr<CrosSettingsProvider>> providers_;

  // A stubbed provider - only used if the kStubCrosSettings switch is set.
  StubCrosSettingsProvider* stubbed_provider_ptr_ = nullptr;

  // A map from settings names to a list of observers. Observers get fired in
  // the order they are added.
  std::map<std::string, std::unique_ptr<base::CallbackList<void(void)>>>
      settings_observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(CrosSettings);
};

// Helper class for tests. Initializes the CrosSettings singleton on
// construction and tears it down again on destruction.
class ScopedTestCrosSettings {
 public:
  explicit ScopedTestCrosSettings(PrefService* local_state);
  ~ScopedTestCrosSettings();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedTestCrosSettings);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_CROS_SETTINGS_H_
