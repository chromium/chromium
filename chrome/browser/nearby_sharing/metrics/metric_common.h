// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_METRICS_METRIC_COMMON_H_
#define CHROME_BROWSER_NEARBY_SHARING_METRICS_METRIC_COMMON_H_

#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"

namespace nearby::share::metrics {

// For now, we just forward the mojom interface until we can differentiate
// between Windows and ChromeOS.
using Platform = nearby_share::mojom::ShareTargetType;

enum DeviceRelationship {
  kSelf = 0,
  kContact = 1,
  kStranger = 2,
};

Platform GetPlatform(const ShareTarget& share_target);
DeviceRelationship GetDeviceRelationship(const ShareTarget& share_target);

}  // namespace nearby::share::metrics

#endif  // CHROME_BROWSER_NEARBY_SHARING_METRICS_METRIC_COMMON_H_
