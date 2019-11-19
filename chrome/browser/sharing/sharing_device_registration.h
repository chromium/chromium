// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_DEVICE_REGISTRATION_H_
#define CHROME_BROWSER_SHARING_SHARING_DEVICE_REGISTRATION_H_

#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync_device_info/device_info.h"

class PrefService;

namespace instance_id {
class InstanceIDDriver;
}

class SharingSyncPreference;
class VapidKeyManager;
enum class SharingDeviceRegistrationResult;

// Responsible for registering and unregistering device with
// SharingSyncPreference.
class SharingDeviceRegistration {
 public:
  using RegistrationCallback =
      base::OnceCallback<void(SharingDeviceRegistrationResult)>;
  using TargetInfoCallback = base::OnceCallback<void(
      SharingDeviceRegistrationResult,
      base::Optional<syncer::DeviceInfo::SharingTargetInfo>)>;

  SharingDeviceRegistration(PrefService* pref_service,
                            SharingSyncPreference* prefs,
                            instance_id::InstanceIDDriver* instance_id_driver,
                            VapidKeyManager* vapid_key_manager);
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

  // Returns if device can handle an incoming webrtc peer connection request.
  bool IsPeerConnectionSupported() const;

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
      const std::string& authorized_entity,
      SharingDeviceRegistrationResult result,
      base::Optional<syncer::DeviceInfo::SharingTargetInfo> vapid_target_info);

  void OnSharingTargetInfoRetrieved(
      RegistrationCallback callback,
      const std::string& authorized_entity,
      syncer::DeviceInfo::SharingTargetInfo vapid_target_info,
      SharingDeviceRegistrationResult result,
      base::Optional<syncer::DeviceInfo::SharingTargetInfo>
          sharing_target_info);

  void OnVapidFCMTokenDeleted(RegistrationCallback callback,
                              SharingDeviceRegistrationResult result);

  void DeleteFCMToken(const std::string& authorized_entity,
                      RegistrationCallback callback);

  void OnFCMTokenDeleted(RegistrationCallback callback,
                         instance_id::InstanceID::Result result);

  // Returns the authorization entity for FCM registration.
  base::Optional<std::string> GetAuthorizationEntity() const;

  // Computes and returns a set of all enabled features on the device.
  std::set<sync_pb::SharingSpecificFields_EnabledFeatures> GetEnabledFeatures()
      const;

  PrefService* pref_service_;
  SharingSyncPreference* sharing_sync_preference_;
  instance_id::InstanceIDDriver* instance_id_driver_;
  VapidKeyManager* vapid_key_manager_;
  base::Optional<std::set<sync_pb::SharingSpecificFields_EnabledFeatures>>
      enabled_features_testing_value_;

  base::WeakPtrFactory<SharingDeviceRegistration> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SharingDeviceRegistration);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_DEVICE_REGISTRATION_H_
