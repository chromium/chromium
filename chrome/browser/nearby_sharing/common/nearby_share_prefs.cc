// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"

#include <string>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_enums.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"

namespace prefs {

const char kNearbySharingActiveProfilePrefName[] =
    "nearby_sharing.active_profile";
const char kNearbySharingAllowedContactsPrefName[] =
    "nearby_sharing.allowed_contacts";
const char kNearbySharingBackgroundVisibilityName[] =
    "nearby_sharing.background_visibility";
const char kNearbySharingContactUploadHashPrefName[] =
    "nearby_sharing.contact_upload_hash";
const char kNearbySharingDataUsageName[] = "nearby_sharing.data_usage";
const char kNearbySharingDeviceIdPrefName[] = "nearby_sharing.device_id";
const char kNearbySharingDeviceNamePrefName[] = "nearby_sharing.device_name";
const char kNearbySharingEnabledPrefName[] = "nearby_sharing.enabled";
const char kNearbySharingFastInitiationNotificationStatePrefName[] =
    "nearby_sharing.fast_initiation_notification_state";
const char kNearbySharingOnboardingCompletePrefName[] =
    "nearby_sharing.onboarding_complete";
const char kNearbySharingFullNamePrefName[] = "nearby_sharing.full_name";
const char kNearbySharingIconUrlPrefName[] = "nearby_sharing.icon_url";
const char kNearbySharingIconTokenPrefName[] = "nearby_sharing.icon_token";
const char kNearbySharingNearbyDeviceTryingToShareDismissedTimePrefName[] =
    "nearby_sharing.nearby_device_trying_to_share_dismissed_time";
const char kNearbySharingPublicCertificateExpirationDictPrefName[] =
    "nearbyshare.public_certificate_expiration_dict";
const char kNearbySharingPrivateCertificateListPrefName[] =
    "nearbyshare.private_certificate_list";
const char kNearbySharingSchedulerContactDownloadAndUploadPrefName[] =
    "nearby_sharing.scheduler.contact_download_and_upload";
const char kNearbySharingSchedulerDownloadDeviceDataPrefName[] =
    "nearby_sharing.scheduler.download_device_data";
const char kNearbySharingSchedulerDownloadPublicCertificatesPrefName[] =
    "nearby_sharing.scheduler.download_public_certificates";
const char kNearbySharingSchedulerPeriodicContactUploadPrefName[] =
    "nearby_sharing.scheduler.periodic_contact_upload";
const char kNearbySharingSchedulerPrivateCertificateExpirationPrefName[] =
    "nearby_sharing.scheduler.private_certificate_expiration";
const char kNearbySharingSchedulerPublicCertificateExpirationPrefName[] =
    "nearby_sharing.scheduler.public_certificate_expiration";
const char kNearbySharingSchedulerUploadDeviceNamePrefName[] =
    "nearby_sharing.scheduler.upload_device_name";
const char kNearbySharingSchedulerUploadLocalDeviceCertificatesPrefName[] =
    "nearby_sharing.scheduler.upload_local_device_certificates";
const char kNearbySharingNextVisibilityReminderTimePrefName[] =
    "nearby_sharing.next_visibility_reminder_time";
}  // namespace prefs

void RegisterNearbySharingPrefs(PrefRegistrySimple* registry) {
  // These prefs are not synced across devices on purpose.

  registry->RegisterBooleanPref(prefs::kNearbySharingEnabledPrefName,
                                /*default_value=*/false);
  registry->RegisterIntegerPref(
      prefs::kNearbySharingFastInitiationNotificationStatePrefName,
      /*default_value=*/static_cast<int>(
          FastInitiationNotificationState::kEnabled));
  registry->RegisterBooleanPref(prefs::kNearbySharingOnboardingCompletePrefName,
                                /*default_value=*/false);
  registry->RegisterIntegerPref(
      prefs::kNearbySharingBackgroundVisibilityName,
      /*default_value=*/static_cast<int>(Visibility::kUnknown));
  registry->RegisterIntegerPref(
      prefs::kNearbySharingDataUsageName,
      /*default_value=*/static_cast<int>(DataUsage::kWifiOnly));
  registry->RegisterStringPref(prefs::kNearbySharingContactUploadHashPrefName,
                               /*default_value=*/std::string());
  registry->RegisterStringPref(prefs::kNearbySharingDeviceIdPrefName,
                               /*default_value=*/std::string());
  registry->RegisterStringPref(prefs::kNearbySharingDeviceNamePrefName,
                               /*default_value=*/std::string());
  registry->RegisterListPref(prefs::kNearbySharingAllowedContactsPrefName);
  registry->RegisterStringPref(prefs::kNearbySharingFullNamePrefName,
                               /*default_value=*/std::string());
  registry->RegisterStringPref(prefs::kNearbySharingIconUrlPrefName,
                               /*default_value=*/std::string());
  registry->RegisterStringPref(prefs::kNearbySharingIconTokenPrefName,
                               /*default_value=*/std::string());
  registry->RegisterTimePref(
      prefs::kNearbySharingNearbyDeviceTryingToShareDismissedTimePrefName,
      /*default_value=*/base::Time());
  registry->RegisterTimePref(
      prefs::kNearbySharingNextVisibilityReminderTimePrefName,
      /*default_value=*/base::Time());
  registry->RegisterDictionaryPref(
      prefs::kNearbySharingPublicCertificateExpirationDictPrefName);
  registry->RegisterListPref(
      prefs::kNearbySharingPrivateCertificateListPrefName);
  registry->RegisterDictionaryPref(
      prefs::kNearbySharingSchedulerContactDownloadAndUploadPrefName);
  registry->RegisterDictionaryPref(
      prefs::kNearbySharingSchedulerDownloadDeviceDataPrefName);
  registry->RegisterDictionaryPref(
      prefs::kNearbySharingSchedulerDownloadPublicCertificatesPrefName);
  registry->RegisterDictionaryPref(
      prefs::kNearbySharingSchedulerPeriodicContactUploadPrefName);
  registry->RegisterDictionaryPref(
      prefs::kNearbySharingSchedulerPrivateCertificateExpirationPrefName);
  registry->RegisterDictionaryPref(
      prefs::kNearbySharingSchedulerPublicCertificateExpirationPrefName);
  registry->RegisterDictionaryPref(
      prefs::kNearbySharingSchedulerUploadDeviceNamePrefName);
  registry->RegisterDictionaryPref(
      prefs::kNearbySharingSchedulerUploadLocalDeviceCertificatesPrefName);
}

void RegisterNearbySharingLocalPrefs(PrefRegistrySimple* local_state) {
  local_state->RegisterFilePathPref(prefs::kNearbySharingActiveProfilePrefName,
                                    /*default_value=*/base::FilePath());
}
