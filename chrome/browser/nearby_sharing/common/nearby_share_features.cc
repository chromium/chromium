// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "base/feature_list.h"

namespace features {

// Enables Nearby Sharing functionality.
BASE_FEATURE(kNearbySharing, "NearbySharing", base::FEATURE_ENABLED_BY_DEFAULT);

// Enables use of device contacts in Nearby Share. The Nearby server returns
// both Google contacts and device contacts in ListContactPeople RPC responses.
// When this flag is disabled, device contacts will be filtered out by the
// Chrome OS client. This flag acts as a kill switch.
BASE_FEATURE(kNearbySharingDeviceContacts,
             "NearbySharingDeviceContacts",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables new one-page onboarding workflow for Nearby Share.
BASE_FEATURE(kNearbySharingOnePageOnboarding,
             "NearbySharingOnePageOnboarding",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables UI features for Self Share to allow seamless sharing between a user's
// own devices.
BASE_FEATURE(kNearbySharingSelfShare,
             "NearbySharingSelfShare",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables use of WebRTC in Nearby Share.
BASE_FEATURE(kNearbySharingWebRtc,
             "NearbySharingWebRtc",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables use of WifiLan in Nearby Share.
BASE_FEATURE(kNearbySharingWifiLan,
             "NearbySharingWifiLan",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsSelfShareEnabled() {
  return base::FeatureList::IsEnabled(kNearbySharingSelfShare);
}

}  // namespace features
