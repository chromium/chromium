// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "base/feature_list.h"

namespace features {

// Enables Nearby Sharing functionality.
const base::Feature kNearbySharing{"NearbySharing",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Enables support for Nearby Share on child accounts.
const base::Feature kNearbySharingChildAccounts{
    "NearbySharingChildAccounts", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables use of device contacts in Nearby Share. The Nearby server returns
// both Google contacts and device contacts in ListContactPeople RPC responses.
// When this flag is disabled, device contacts will be filtered out by the
// Chrome OS client. This flag acts as a kill switch.
const base::Feature kNearbySharingDeviceContacts{
    "NearbySharingDeviceContacts", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables use of WebRTC in Nearby Share.
const base::Feature kNearbySharingWebRtc{"NearbySharingWebRtc",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
