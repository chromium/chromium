// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_DEVICE_REGISTRATION_H_
#define CHROME_BROWSER_SHARING_SHARING_DEVICE_REGISTRATION_H_

#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync_device_info/device_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace instance_id {
class InstanceIDDriver;
}  // namespace instance_id

namespace syncer {
class SyncService;
}  // namespace syncer

enum class SharingDeviceRegistrationResult;
class SharingSyncPreference;
class VapidKeyManager;

// Responsible for registering and unregistering device with
// SharingSyncPreference.
class SharingDeviceRegistration {
 public:
  using RegistrationCallback =
      base::OnceCallback<void(SharingDeviceRegistrationResult)>;
  using TargetInfoCallback = base::OnceCallback<void(
      SharingDeviceRegistrationResult,
      absl::optional<syncer::DeviceInfo::SharingTargetInfo>)>;

  SharingDeviceRegistration(PrefService* pref_service,
                            SharingSyncPreference* prefs,
                            VapidKeyManager* vapid_key_manager,
                            instance_id::InstanceIDDriver* instance_id_driver,
                            syncer::SyncService* sync_service);

  SharingDeviceRegistration(const SharingDeviceRegistration&) = delete;
  SharingDeviceRegistration& operator=(const SharingDeviceRegistration&) =
      delete;

  virtual ~SharingDeviceRegistration();

  // Registers device with sharing sync preferences. Takes a |callback| function
  // which receives the result of FCM registration for device.
  virtual void RegisterDevice(RegistrationCallback callback);

  // Un-registers device with sharing sync preferences.
  virtual void UnregisterDevice(RegistrationCallback callback);

  // Returns if device can handle receiving phone numbers for calling.
  bool IsClickToCallSupported() const;

  // Returns if device can handle receiving of shared clipboard contents.
  virtual bool IsSharedClipboardSupported() const;

  // Returns if device can handle receiving of sms fetcher requests.
  virtual bool IsSmsFetcherSupported() const;

  // Returns if device can handle receiving of remote copy contents.
  virtual bool IsRemoteCopySupported() const;

  // Returns if device can handle receiving of optimization guide push
  // notification.
  virtual bool IsOptimizationGuidePushNotificationSupported() const;

  // For testing
  void SetEnabledFeaturesForTesting(
      std::set<sync_pb::SharingSpecificFields_EnabledFeatures>
          enabled_features);

 private:
  FRIEND_TEST_ALL_PREFIXES(SharingDeviceRegistrationTest,
                           RegisterDeviceTest_Success);

  void RetrieveTargetInfo(const std::string& authorized_entity,
                          TargetInfoCallback callback);

  void OnFCMTokenReceived(TargetInfoCallback callback,
                          const std::string& authorized_entity,
                          const std::string& fcm_token,
                          instance_id::InstanceID::Result result);

  void OnEncryptionInfoReceived(TargetInfoCallback callback,
                                const std::string& fcm_token,
                                std::string p256dh,
                                std::string auth_secret);

  void OnVapidTargetInfoRetrieved(
      RegistrationCallback callback,
      absl::optional<std::string> authorized_entity,
      SharingDeviceRegistrationResult result,
      absl::optional<syncer::DeviceInfo::SharingTargetInfo> vapid_target_info);

  void OnSharingTargetInfoRetrieved(
      RegistrationCallback callback,
      absl::optional<std::string> authorized_entity,
      absl::optional<syncer::DeviceInfo::SharingTargetInfo> vapid_target_info,
      SharingDeviceRegistrationResult result,
      absl::optional<syncer::DeviceInfo::SharingTargetInfo>
          sharing_target_info);

  void OnVapidFCMTokenDeleted(RegistrationCallback callback,
                              SharingDeviceRegistrationResult result);

  void DeleteFCMToken(const std::string& authorized_entity,
                      RegistrationCallback callback);

  void OnFCMTokenDeleted(RegistrationCallback callback,
                         instance_id::InstanceID::Result result);

  // Returns the authorization entity for FCM registration.
  absl::optional<std::string> GetAuthorizationEntity() const;

  // Computes and returns a set of all enabled features on the device.
  // |supports_vapid|: If set to true, then enabled features with VAPID suffix
  // will be returned, meaning old clients can send VAPID message to this device
  // for those features.
  std::set<sync_pb::SharingSpecificFields_EnabledFeatures> GetEnabledFeatures(
      bool supports_vapid) const;

  raw_ptr<PrefService> pref_service_;
  raw_ptr<SharingSyncPreference> sharing_sync_preference_;
  raw_ptr<VapidKeyManager> vapid_key_manager_;
  raw_ptr<instance_id::InstanceIDDriver> instance_id_driver_;
  raw_ptr<syncer::SyncService> sync_service_;
  absl::optional<std::set<sync_pb::SharingSpecificFields_EnabledFeatures>>
      enabled_features_testing_value_;

  base::WeakPtrFactory<SharingDeviceRegistration> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_SHARING_DEVICE_REGISTRATION_H_
