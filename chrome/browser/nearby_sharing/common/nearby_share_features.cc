// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "base/feature_list.h"

namespace features {

// Enables Nearby Sharing functionality.
const base::Feature kNearbySharing{"NearbySharing",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Enables background scanning for Nearby Share, allowing devices to
// persistently scan and present a notification when a nearby device is
// attempting to share.
const base::Feature kNearbySharingBackgroundScanning{
    "NearbySharingBackgroundScanning", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables support for Nearby Share on child accounts.
const base::Feature kNearbySharingChildAccounts{
    "NearbySharingChildAccounts", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables use of device contacts in Nearby Share. The Nearby server returns
// both Google contacts and device contacts in ListContactPeople RPC responses.
// When this flag is disabled, device contacts will be filtered out by the
// Chrome OS client. This flag acts as a kill switch.
const base::Feature kNearbySharingDeviceContacts{
    "NearbySharingDeviceContacts", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables new one-page onboarding workflow for Nearby Share.
const base::Feature kNearbySharingOnePageOnboarding{
    "NearbySharingOnePageOnboarding", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables receiving WiFi networks using Nearby Share.
const base::Feature kNearbySharingReceiveWifiCredentials{
    "NearbySharingReceiveWifiCredentials", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables auto-accept functionality when sharing between a user's own devices.
const base::Feature kNearbySharingSelfShareAutoAccept{
    "NearbySharingSelfShareAutoAccept", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables UI features for Self Share, to allow seamless sharing between a
// user's own devices.
const base::Feature kNearbySharingSelfShareUI{
    "NearbySharingSelfShareUI", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables use of WebRTC in Nearby Share.
const base::Feature kNearbySharingWebRtc{"NearbySharingWebRtc",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enables use of WifiLan in Nearby Share.
const base::Feature kNearbySharingWifiLan{"NearbySharingWifiLan",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
