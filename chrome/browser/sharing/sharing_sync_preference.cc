// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_sync_preference.h"

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/util/values/values_util.h"
#include "base/values.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace {

const char kVapidECPrivateKey[] = "vapid_private_key";
const char kVapidCreationTimestamp[] = "vapid_creation_timestamp";

const char kDeviceFcmToken[] = "device_fcm_token";
const char kDeviceP256dh[] = "device_p256dh";
const char kDeviceAuthSecret[] = "device_auth_secret";

const char kRegistrationAuthorizedEntity[] = "registration_authorized_entity";
const char kRegistrationTimestamp[] = "registration_timestamp";

const char kSharingInfoVapidTargetInfo[] = "vapid_target_info";
const char kSharingInfoSenderIdTargetInfo[] = "sender_id_target_info";
const char kSharingInfoEnabledFeatures[] = "enabled_features";

base::DictionaryValue TargetInfoToValue(
    const syncer::DeviceInfo::SharingTargetInfo& target_info) {
  std::string base64_p256dh, base64_auth_secret;
  base::Base64Encode(target_info.p256dh, &base64_p256dh);
  base::Base64Encode(target_info.auth_secret, &base64_auth_secret);

  base::DictionaryValue result;
  result.SetStringKey(kDeviceFcmToken, target_info.fcm_token);
  result.SetStringKey(kDeviceP256dh, base64_p256dh);
  result.SetStringKey(kDeviceAuthSecret, base64_auth_secret);
  return result;
}

base::Optional<syncer::DeviceInfo::SharingTargetInfo> ValueToTargetInfo(
    const base::Value& value) {
  const std::string* fcm_token = value.FindStringKey(kDeviceFcmToken);
  if (!fcm_token)
    return base::nullopt;

  const std::string* base64_p256dh = value.FindStringKey(kDeviceP256dh);
  const std::string* base64_auth_secret =
      value.FindStringKey(kDeviceAuthSecret);

  std::string p256dh, auth_secret;
  if (!base64_p256dh || !base64_auth_secret ||
      !base::Base64Decode(*base64_p256dh, &p256dh) ||
      !base::Base64Decode(*base64_auth_secret, &auth_secret)) {
    return base::nullopt;
  }

  return syncer::DeviceInfo::SharingTargetInfo{*fcm_token, std::move(p256dh),
                                               std::move(auth_secret)};
}

}  // namespace

using sync_pb::SharingSpecificFields;

SharingSyncPreference::FCMRegistration::FCMRegistration(
    base::Optional<std::string> authorized_entity,
    base::Time timestamp)
    : authorized_entity(std::move(authorized_entity)), timestamp(timestamp) {}

SharingSyncPreference::FCMRegistration::FCMRegistration(
    FCMRegistration&& other) = default;

SharingSyncPreference::FCMRegistration&
SharingSyncPreference::FCMRegistration::operator=(FCMRegistration&& other) =
    default;

SharingSyncPreference::FCMRegistration::~FCMRegistration() = default;

SharingSyncPreference::SharingSyncPreference(
    PrefService* prefs,
    syncer::DeviceInfoSyncService* device_info_sync_service)
    : prefs_(prefs), device_info_sync_service_(device_info_sync_service) {
  DCHECK(prefs_);
  DCHECK(device_info_sync_service_);
  local_device_info_provider_ =
      device_info_sync_service_->GetLocalDeviceInfoProvider();
  pref_change_registrar_.Init(prefs);
}

SharingSyncPreference::~SharingSyncPreference() = default;

// static
void SharingSyncPreference::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kSharingVapidKey, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(prefs::kSharingFCMRegistration);
  registry->RegisterDictionaryPref(prefs::kSharingLocalSharingInfo);
}

base::Optional<std::vector<uint8_t>> SharingSyncPreference::GetVapidKey()
    const {
  const base::DictionaryValue* vapid_key =
      prefs_->GetDictionary(prefs::kSharingVapidKey);
  std::string base64_private_key, private_key;
  if (!vapid_key->GetString(kVapidECPrivateKey, &base64_private_key))
    return base::nullopt;

  if (base::Base64Decode(base64_private_key, &private_key)) {
    return std::vector<uint8_t>(private_key.begin(), private_key.end());
  } else {
    LOG(ERROR) << "Could not decode stored vapid keys.";
    return base::nullopt;
  }
}

void SharingSyncPreference::SetVapidKey(
    const std::vector<uint8_t>& vapid_key) const {
  base::Time creation_timestamp = base::Time::Now();
  std::string base64_vapid_key;
  base::Base64Encode(std::string(vapid_key.begin(), vapid_key.end()),
                     &base64_vapid_key);

  DictionaryPrefUpdate update(prefs_, prefs::kSharingVapidKey);
  update->SetStringKey(kVapidECPrivateKey, base64_vapid_key);
  update->SetKey(kVapidCreationTimestamp,
                 util::TimeToValue(creation_timestamp));
}

void SharingSyncPreference::SetVapidKeyChangeObserver(
    const base::RepeatingClosure& obs) {
  ClearVapidKeyChangeObserver();
  pref_change_registrar_.Add(prefs::kSharingVapidKey, obs);
}

void SharingSyncPreference::ClearVapidKeyChangeObserver() {
  if (pref_change_registrar_.IsObserved(prefs::kSharingVapidKey))
    pref_change_registrar_.Remove(prefs::kSharingVapidKey);
}

base::Optional<SharingSyncPreference::FCMRegistration>
SharingSyncPreference::GetFCMRegistration() const {
  const base::DictionaryValue* registration =
      prefs_->GetDictionary(prefs::kSharingFCMRegistration);
  const std::string* authorized_entity_ptr =
      registration->FindStringKey(kRegistrationAuthorizedEntity);
  const base::Value* timestamp_value =
      registration->FindKey(kRegistrationTimestamp);
  if (!timestamp_value)
    return base::nullopt;

  base::Optional<std::string> authorized_entity;
  if (authorized_entity_ptr)
    authorized_entity = *authorized_entity_ptr;

  base::Optional<base::Time> timestamp = util::ValueToTime(timestamp_value);
  if (!timestamp)
    return base::nullopt;

  return FCMRegistration(authorized_entity, *timestamp);
}

void SharingSyncPreference::SetFCMRegistration(FCMRegistration registration) {
  DictionaryPrefUpdate update(prefs_, prefs::kSharingFCMRegistration);
  if (registration.authorized_entity) {
    update->SetStringKey(kRegistrationAuthorizedEntity,
                         std::move(*registration.authorized_entity));
  } else {
    update->RemoveKey(kRegistrationAuthorizedEntity);
  }
  update->SetKey(kRegistrationTimestamp,
                 util::TimeToValue(registration.timestamp));
}

void SharingSyncPreference::ClearFCMRegistration() {
  prefs_->ClearPref(prefs::kSharingFCMRegistration);
}

void SharingSyncPreference::SetLocalSharingInfo(
    syncer::DeviceInfo::SharingInfo sharing_info) {
  auto* device_info = local_device_info_provider_->GetLocalDeviceInfo();
  if (!device_info)
    return;

  // Update prefs::kSharingLocalSharingInfo to cache value locally.
  if (device_info->sharing_info() == sharing_info)
    return;

  base::DictionaryValue vapid_target_info =
      TargetInfoToValue(sharing_info.vapid_target_info);
  base::DictionaryValue sender_id_target_info =
      TargetInfoToValue(sharing_info.sender_id_target_info);

  base::ListValue list_value;
  for (SharingSpecificFields::EnabledFeatures feature :
       sharing_info.enabled_features) {
    list_value.Append(feature);
  }

  DictionaryPrefUpdate local_sharing_info_update(
      prefs_, prefs::kSharingLocalSharingInfo);
  local_sharing_info_update->SetKey(kSharingInfoVapidTargetInfo,
                                    std::move(vapid_target_info));
  local_sharing_info_update->SetKey(kSharingInfoSenderIdTargetInfo,
                                    std::move(sender_id_target_info));
  local_sharing_info_update->SetKey(kSharingInfoEnabledFeatures,
                                    std::move(list_value));

  device_info_sync_service_->RefreshLocalDeviceInfo();
}

void SharingSyncPreference::ClearLocalSharingInfo() {
  auto* device_info = local_device_info_provider_->GetLocalDeviceInfo();
  if (!device_info)
    return;

  // Update prefs::kSharingLocalSharingInfo to clear local cache.
  prefs_->ClearPref(prefs::kSharingLocalSharingInfo);

  if (device_info->sharing_info()) {
    device_info_sync_service_->RefreshLocalDeviceInfo();
  }
}

// static
base::Optional<syncer::DeviceInfo::SharingInfo>
SharingSyncPreference::GetLocalSharingInfoForSync(PrefService* prefs) {
  const base::DictionaryValue* registration =
      prefs->GetDictionary(prefs::kSharingLocalSharingInfo);

  const base::Value* vapid_target_info_value =
      registration->FindDictKey(kSharingInfoVapidTargetInfo);
  const base::Value* sender_id_target_info_value =
      registration->FindDictKey(kSharingInfoSenderIdTargetInfo);
  const base::Value* enabled_features_value =
      registration->FindListKey(kSharingInfoEnabledFeatures);
  if (!vapid_target_info_value || !sender_id_target_info_value ||
      !enabled_features_value) {
    return base::nullopt;
  }

  auto vapid_target_info = ValueToTargetInfo(*vapid_target_info_value);
  auto sender_id_target_info = ValueToTargetInfo(*sender_id_target_info_value);
  if (!vapid_target_info || !sender_id_target_info)
    return base::nullopt;

  std::set<SharingSpecificFields::EnabledFeatures> enabled_features;
  for (auto& value : enabled_features_value->GetList()) {
    DCHECK(value.is_int());
    int feature_value = value.GetInt();
    // Filter invalid enums from other browser versions.
    if (!sync_pb::SharingSpecificFields::EnabledFeatures_IsValid(feature_value))
      continue;

    enabled_features.insert(
        static_cast<SharingSpecificFields::EnabledFeatures>(feature_value));
  }

  return syncer::DeviceInfo::SharingInfo(std::move(*vapid_target_info),
                                         std::move(*sender_id_target_info),
                                         std::move(enabled_features));
}
