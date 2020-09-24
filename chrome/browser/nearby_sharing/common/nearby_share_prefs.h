// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_PREFS_H_
#define CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_PREFS_H_

class PrefRegistrySimple;

namespace prefs {

extern const char kNearbySharingActiveProfilePrefName[];
extern const char kNearbySharingAllowedContactsPrefName[];
extern const char kNearbySharingBackgroundVisibilityName[];
extern const char kNearbySharingContactUploadHashPrefName[];
extern const char kNearbySharingDataUsageName[];
extern const char kNearbySharingDeviceIdPrefName[];
extern const char kNearbySharingDeviceNamePrefName[];
extern const char kNearbySharingEnabledPrefName[];
extern const char kNearbySharingOnboardingCompletePrefName[];
extern const char kNearbySharingFullNamePrefName[];
extern const char kNearbySharingIconUrlPrefName[];
extern const char kNearbySharingOnboardingDismissedTimePrefName[];
extern const char kNearbySharingPrivateCertificateListPrefName[];
extern const char kNearbySharingPublicCertificateExpirationDictPrefName[];
extern const char kNearbySharingSchedulerContactDownloadAndUploadPrefName[];
extern const char kNearbySharingSchedulerDownloadDeviceDataPrefName[];
extern const char kNearbySharingSchedulerDownloadPublicCertificatesPrefName[];
extern const char kNearbySharingSchedulerPrivateCertificateExpirationPrefName[];
extern const char kNearbySharingSchedulerPublicCertificateExpirationPrefName[];
extern const char kNearbySharingSchedulerUploadDeviceNamePrefName[];
extern const char
    kNearbySharingSchedulerUploadLocalDeviceCertificatesPrefName[];

}  // namespace prefs

void RegisterNearbySharingPrefs(PrefRegistrySimple* registry);

void RegisterNearbySharingLocalPrefs(PrefRegistrySimple* local_state);

#endif  // CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_PREFS_H_
