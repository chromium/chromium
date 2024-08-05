// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_FEATURES_H_
#define CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_FEATURES_H_

#include "base/feature_list.h"

namespace features {

BASE_DECLARE_FEATURE(kIsNameEnabled);
bool IsNameEnabled();
BASE_DECLARE_FEATURE(kNearbySharing);
BASE_DECLARE_FEATURE(kNearbySharingDeviceContacts);
BASE_DECLARE_FEATURE(kNearbySharingOnePageOnboarding);
BASE_DECLARE_FEATURE(kNearbySharingRestrictToContacts);
bool IsRestrictToContactsEnabled();
BASE_DECLARE_FEATURE(kNearbySharingSelfShare);
bool IsSelfShareEnabled();
BASE_DECLARE_FEATURE(kNearbySharingVisibilityReminder);
BASE_DECLARE_FEATURE(kNearbySharingWebRtc);
BASE_DECLARE_FEATURE(kNearbySharingWifiLan);

}  // namespace features

#endif  // CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_FEATURES_H_
