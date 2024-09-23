// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_SETTINGS_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_SETTINGS_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class PrefService;

// Provides a type safe wrapper/abstraction over prefs for both C++ and
// Javascript (over mojo) to interact with Nearby user settings. This class
// always reads directly from prefs and relies on pref's memory cache.
// It is designed to be contained within the Nearby Sharing Service with an
// instance per user profile. This class also helps to keep some of the prefs
// logic out of |NearbyShareServiceImpl|.
//
// This class is also used to expose device properties that affect the settings
// UI, but cannot be added at load time because they need to be re-computed. See
// GetIsFastInitiationHardwareSupported() as an example.
//
// The mojo interface is intended to be exposed in settings, os_settings, and
// the nearby WebUI.
//
// NOTE: The pref-change registrar only notifies observers of pref value
// changes; observers are not notified if the pref value is set but does not
// change. This class inherits this behavior.
//
// NOTE: Because the observer interface is over mojo, setting a value directly
// will not synchronously trigger the observer event. Generally this is not a
// problem because these settings should only be changed by user interaction,
// but this is necessary to know when writing unit-tests.
//
// TODO(nohle): Use the NearbyShareContactManager to implement
// Get/SetAllowedContacts().
class NearbyShareSettings : public nearby_share::mojom::NearbyShareSettings,
                            public NearbyShareLocalDeviceDataManager::Observer {
 public:
  using DataUsage = nearby_share::mojom::DataUsage;
  NearbyShareSettings(
      PrefService* pref_service_,
      NearbyShareLocalDeviceDataManager* local_device_data_manager);
  ~NearbyShareSettings() override;

  // Synchronous getters for C++ clients, mojo setters can be used as is
  bool GetEnabled() const;
  nearby_share::mojom::FastInitiationNotificationState
  GetFastInitiationNotificationState() const;
  bool is_fast_initiation_hardware_supported() {
    return is_fast_initiation_hardware_supported_;
  }
  void SetIsFastInitiationHardwareSupported(bool is_supported);
  std::string GetDeviceName() const;
  nearby_share::mojom::DataUsage GetDataUsage() const;
  nearby_share::mojom::Visibility GetVisibility() const;
  const std::vector<std::string> GetAllowedContacts() const;
  bool IsOnboardingComplete() const;

  // Returns true if the feature is disabled by policy.
  bool IsDisabledByPolicy() const;

  // nearby_share::mojom::NearbyShareSettings
  void AddSettingsObserver(
      ::mojo::PendingRemote<nearby_share::mojom::NearbyShareSettingsObserver>
          observer) override;
  void GetEnabled(base::OnceCallback<void(bool)> callback) override;
  void GetFastInitiationNotificationState(
      base::OnceCallback<
          void(nearby_share::mojom::FastInitiationNotificationState)> callback)
      override;
  void GetIsFastInitiationHardwareSupported(
      base::OnceCallback<void(bool)> callback) override;
  void SetEnabled(bool enabled) override;
  void SetFastInitiationNotificationState(
      nearby_share::mojom::FastInitiationNotificationState state) override;
  void IsOnboardingComplete(base::OnceCallback<void(bool)> callback) override;
  void SetIsOnboardingComplete(bool completed) override;
  void GetDeviceName(
      base::OnceCallback<void(const std::string&)> callback) override;
  void ValidateDeviceName(
      const std::string& device_name,
      base::OnceCallback<void(nearby_share::mojom::DeviceNameValidationResult)>
          callback) override;
  void SetDeviceName(
      const std::string& device_name,
      base::OnceCallback<void(nearby_share::mojom::DeviceNameValidationResult)>
          callback) override;
  void GetDataUsage(base::OnceCallback<void(nearby_share::mojom::DataUsage)>
                        callback) override;
  void SetDataUsage(nearby_share::mojom::DataUsage data_usage) override;
  void GetVisibility(base::OnceCallback<void(nearby_share::mojom::Visibility)>
                         callback) override;
  void SetVisibility(nearby_share::mojom::Visibility visibility) override;
  void GetAllowedContacts(
      base::OnceCallback<void(const std::vector<std::string>&)> callback)
      override;
  void SetAllowedContacts(
      const std::vector<std::string>& allowed_contacts) override;
  void Bind(
      mojo::PendingReceiver<nearby_share::mojom::NearbyShareSettings> receiver);

  // NearbyShareLocalDeviceDataManager::Observer:
  void OnLocalDeviceDataChanged(bool did_device_name_change,
                                bool did_full_name_change,
                                bool did_icon_change) override;

 private:
  void OnEnabledPrefChanged();
  void OnFastInitiationNotificationStatePrefChanged();
  void OnDataUsagePrefChanged();
  void OnVisibilityPrefChanged();
  void OnAllowedContactsPrefChanged();
  void OnIsOnboardingCompletePrefChanged();

  // If the Nearby Share parent feature is toggled on then Fast Initiation
  // notifications should be re-enabled unless the user explicitly disabled the
  // notification sub-feature.
  void ProcessFastInitiationNotificationParentPrefChanged(bool enabled);

  // This is false by default and gets updated in NearbySharingServiceImpl when
  // the bluetooth adapter availablility changes.
  bool is_fast_initiation_hardware_supported_ = false;
  mojo::RemoteSet<nearby_share::mojom::NearbyShareSettingsObserver>
      observers_set_;
  mojo::ReceiverSet<nearby_share::mojom::NearbyShareSettings> receiver_set_;
  raw_ptr<PrefService> pref_service_ = nullptr;
  raw_ptr<NearbyShareLocalDeviceDataManager> local_device_data_manager_ =
      nullptr;
  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_SETTINGS_H_
