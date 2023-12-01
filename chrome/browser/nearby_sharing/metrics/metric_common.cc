// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/metrics/metric_common.h"

namespace nearby::share::metrics {

Platform GetPlatform(const ShareTarget& share_target) {
  return share_target.type;
}

DeviceRelationship GetDeviceRelationship(const ShareTarget& share_target) {
  if (share_target.for_self_share) {
    return DeviceRelationship::kSelf;
  } else if (share_target.is_known) {
    return DeviceRelationship::kContact;
  } else {
    return DeviceRelationship::kStranger;
  }
}

}  // namespace nearby::share::metrics
