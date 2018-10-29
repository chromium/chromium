// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_PROVIDER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "components/ownership/owner_settings_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_value_map.h"

class PrefService;

namespace base {
class Value;
}  // namespace base

namespace enterprise_management {
class ChromeDeviceSettingsProto;
}  // namespace enterprise_management

namespace chromeos {

// CrosSettingsProvider implementation that works with device settings.
// Dependency: chromeos::InstallAttributes must be initialized while this class
// is in use.
//
// Note that the write path is in the process of being migrated to
// OwnerSettingsServiceChromeOS (crbug.com/230018).
class DeviceSettingsProvider
    : public CrosSettingsProvider,
      public DeviceSettingsService::Observer,
      public ownership::OwnerSettingsService::Observer {
 public:
  // The callback type that is called to get the device mode.
  typedef base::Callback<policy::DeviceMode(void)> GetDeviceModeCallback;

  DeviceSettingsProvider(const NotifyObserversCallback& notify_cb,
                         DeviceSettingsService* device_settings_service,
                         PrefService* pref_service);
  ~DeviceSettingsProvider() override;

  // Returns true if |path| is handled by this provider.
  static bool IsDeviceSetting(const std::string& name);

  // CrosSettingsProvider implementation.
  const base::Value* Get(const std::string& path) const override;
  TrustedStatus PrepareTrustedValues(const base::Closure& callback) override;
  bool HandlesSetting(const std::string& path) const override;

  // Helper function that decodes policies from provided proto into the pref
  // map.
  static void DecodePolicies(
      const enterprise_management::ChromeDeviceSettingsProto& policy,
      PrefValueMap* new_values_cache);

 private:
  // CrosSettingsProvider implementation:
  void DoSet(const std::string& path, const base::Value& value) override;

  // DeviceSettingsService::Observer implementation:
  void OwnershipStatusChanged() override;
  void DeviceSettingsUpdated() override;
  void OnDeviceSettingsServiceShutdown() override;

  // ownership::OwnerSettingsService::Observer implementation:
  void OnTentativeChangesInPolicy(
      const enterprise_management::PolicyData& policy_data) override;

  // Populates in-memory cache from the local_state cache that is used to store
  // device settings before the device is owned and to speed up policy
  // availability before the policy blob is fetched on boot.
  void RetrieveCachedData();

  // Parses the policy data and fills in |values_cache_|.
  void UpdateValuesCache(
      const enterprise_management::PolicyData& policy_data,
      const enterprise_management::ChromeDeviceSettingsProto& settings,
      TrustedStatus trusted_status);

  // In case of missing policy blob we should verify if this is upgrade of
  // machine owned from pre version 12 OS and the user never touched the device
  // settings. In this case revert to defaults and let people in until the owner
  // comes and changes that.
  bool MitigateMissingPolicy();

  // Checks if the current cache value can be trusted for being representative
  // for the disk cache.
  TrustedStatus RequestTrustedEntity();

  // Invokes UpdateFromService() to synchronize with |device_settings_service_|,
  // then triggers the next store operation if applicable.
  void UpdateAndProceedStoring();

  // Re-reads state from |device_settings_service_|, adjusts
  // |trusted_status_| and calls UpdateValuesCache() if applicable. Returns true
  // if new settings have been loaded.
  bool UpdateFromService();

  // Pending callbacks that need to be invoked after settings verification.
  std::vector<base::Closure> callbacks_;

  DeviceSettingsService* device_settings_service_;
  PrefService* local_state_;

  mutable PrefValueMap migration_values_;

  TrustedStatus trusted_status_;
  DeviceSettingsService::OwnershipStatus ownership_status_;

  // The device settings as currently reported through the
  // CrosSettingsProvider interface. This may be different from the
  // actual current device settings (which can be obtained from
  // |device_settings_service_|) in case the device does not have an
  // owner yet. As soon as ownership of the device will be taken,
  // |device_settings_| will stored on disk and won't be used.
  enterprise_management::ChromeDeviceSettingsProto device_settings_;

  // A cache of values, indexed by the settings keys served through the
  // CrosSettingsProvider interface. This is always kept in sync with the
  // current device settings.
  PrefValueMap values_cache_;

  // Weak pointer factory for creating store operation callbacks.
  base::WeakPtrFactory<DeviceSettingsProvider> store_callback_factory_;

  friend class DeviceSettingsProviderTest;
  FRIEND_TEST_ALL_PREFIXES(DeviceSettingsProviderTest,
                           InitializationTestUnowned);
  FRIEND_TEST_ALL_PREFIXES(DeviceSettingsProviderTest,
                           PolicyFailedPermanentlyNotification);
  FRIEND_TEST_ALL_PREFIXES(DeviceSettingsProviderTest, PolicyLoadNotification);
  DISALLOW_COPY_AND_ASSIGN(DeviceSettingsProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_PROVIDER_H_
