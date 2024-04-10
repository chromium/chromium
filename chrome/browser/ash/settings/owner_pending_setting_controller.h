// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_OWNER_PENDING_SETTING_CONTROLLER_H_
#define CHROME_BROWSER_ASH_SETTINGS_OWNER_PENDING_SETTING_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/ownership/owner_settings_service.h"

class PrefService;
class Profile;

namespace ownership {
class OwnerSettingsService;
}

namespace ash {

// An extra layer on top of CrosSettings / OwnerSettingsService that allows for
// writing a setting before ownership is taken.
//
// Ordinarily, the OwnerSettingsService interface is used for writing settings,
// and the CrosSettings interface is used for reading them - but as the OSS
// cannot be used until the device has an owner, this class can be used instead,
// since writing the new value with SetEnabled works even before ownership is
// taken.
//
// If OSS is ready then the new value is written straight away, and if not, then
// a pending write is queued that is completed as soon as the OSS is ready.
// This write will complete even if Chrome is restarted in the meantime.
// The caller need not care whether the write was immediate or pending, as long
// as they also use this class to read the value of the device pref.
// IsEnabled will return the pending value until ownership is taken and the
// pending value is written - from then on it will return the signed, stored
// value from CrosSettings.
class OwnerPendingSettingController
    : public ownership::OwnerSettingsService::Observer {
 public:
  OwnerPendingSettingController() = delete;
  OwnerPendingSettingController(const OwnerPendingSettingController&) = delete;
  OwnerPendingSettingController& operator=(
      const OwnerPendingSettingController&) = delete;

  // Store the new value. This will happen straight away if |profile| is the
  // owner, and it will cause a pending write to be buffered and written later
  // if the device has no owner yet. It will write a warning and skip if the
  // device already has an owner, and |profile| is not that owner.
  void Set(Profile* profile, const base::Value& value);

  // Returns the latest value - regardless of whether this has been successfully
  // signed and persisted, or if it is still stored as a pending write. Can
  // return std::nullopt if there is no pending write and no signed value.
  std::optional<base::Value> GetValue() const;

  // Add an observer |callback| for changes to the setting.
  [[nodiscard]] base::CallbackListSubscription AddObserver(
      const base::RepeatingClosure& callback);

  // Called once ownership is taken, |service| is the service of the user taking
  // ownership.
  void OnOwnershipTaken(ownership::OwnerSettingsService* service);

  // Sets the callback which is called once when the pending |value| is
  // propagated to the device settings. Support only one callback at a time.
  // CHECKs if the second callback is being set.
  // It's different from the |AddObserver| API. Observers are called
  // immediately after |Set| is called with the different |value| setting.
  void SetOnDeviceSettingsStoredCallBack(base::OnceClosure callback);

  // ownership::OwnerSettingsService::Observer implementation:
  void OnSignedPolicyStored(bool success) override;

  // Clears any value waiting to be written (from storage in local state).
  void ClearPendingValue();

 protected:
  OwnerPendingSettingController(const std::string& pref_name,
                                const std::string& pending_pref_name,
                                PrefService* local_state);
  ~OwnerPendingSettingController() override;

  // Delegates immediately to SetWithService if |service| is ready, otherwise
  // runs SetWithService asynchronously once |service| is ready.
  void SetWithServiceAsync(ownership::OwnerSettingsService* service,
                           const base::Value& value);

  // Callback used by SetWithServiceAsync.
  void SetWithServiceCallback(
      const base::WeakPtr<ownership::OwnerSettingsService>& service,
      const base::Value value,
      bool is_owner);

  // Uses |service| to write the latest value, as long as |service| belongs
  // to the owner - otherwise just prints a warning.
  void SetWithService(ownership::OwnerSettingsService* service,
                      const base::Value& value);

  // Notifies observers if the value has changed.
  void NotifyObservers();

  base::WeakPtr<OwnerPendingSettingController> as_weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<PrefService> local_state_;
  std::optional<base::Value> value_notified_to_observers_;
  base::RepeatingClosureList callback_list_;
  base::CallbackListSubscription setting_subscription_;

  base::ScopedObservation<ownership::OwnerSettingsService,
                          ownership::OwnerSettingsService::Observer>
      owner_settings_service_observation_{this};

  // Indicates if the setting value is in the process of being set with the
  // service. There is a small period of time needed between start saving the
  // value and before the value is stored correctly in the service. We should
  // not use the setting value from the service if it is still in the process
  // of being saved.
  bool is_value_being_set_with_service_ = false;

 private:
  friend class StatsReportingControllerTest;

  // Gets the current ownership status - owned, unowned, or unknown.
  DeviceSettingsService::OwnershipStatus GetOwnershipStatus() const;

  // Get the owner-settings service for a particular profile. A variety of
  // different results can be returned, depending on the profile.
  // a) A ready-to-use service that we know belongs to the owner.
  // b) A ready-to-use service that we know does NOT belong to the owner.
  // c) A service that is NOT ready-to-use, which MIGHT belong to the owner.
  // d) nullptr (for instance, if |profile| is a guest).
  ownership::OwnerSettingsService* GetOwnerSettingsService(Profile* profile);

  // Return the value waiting to be written (stored in local_state), if one
  // exists.
  std::optional<base::Value> GetPendingValue() const;

  // Return the value signed and stored in CrosSettings, if one exists.
  std::optional<base::Value> GetSignedStoredValue() const;

  // Returns whether pending value should be used when determining the value
  // of `GetValue`.
  bool ShouldReadFromPendingValue() const;

  base::OnceClosure on_device_settings_stored_callback_;

  const std::string pref_name_;
  const std::string pending_pref_name_;

  base::WeakPtrFactory<OwnerPendingSettingController> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SETTINGS_OWNER_PENDING_SETTING_CONTROLLER_H_
