// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/common/nearby_share_switches.h"

namespace switches {

// Overrides the default validity period for Nearby Share certificates. Value
// must be larger than 0.
const char kNearbyShareCertificateValidityPeriodHours[] =
    "nearby-share-certificate-validity-period-hours";

// Overrides the default device ID to provide a stable ID in test environments.
// By default we generate a random 10-character string.
const char kNearbyShareDeviceID[] = "nearby-share-device-id";

// Overrides the default URL for Google APIs (https://www.googleapis.com) used
// by Nearby Share
const char kNearbyShareHTTPHost[] = "nearbysharing-http-host";

// Overrides the default number of private certificates generated. Value must be
// larger than 0.
const char kNearbyShareNumPrivateCertificates[] =
    "nearby-share-num-private-certificates";

// Enables verbose logging level for Nearby Share.
const char kNearbyShareVerboseLogging[] = "nearby-share-verbose-logging";

}  // namespace switches
