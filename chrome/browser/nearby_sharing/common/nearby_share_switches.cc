// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/common/nearby_share_switches.h"

namespace switches {

// Overrides the default URL for Google APIs (https://www.googleapis.com) used
// by Nearby Share
const char kNearbyShareHTTPHost[] = "nearbysharing-http-host";

// Enables verbose logging level for Nearby Share.
const char kNearbyShareVerboseLogging[] = "nearby-share-verbose-logging";

}  // namespace switches
