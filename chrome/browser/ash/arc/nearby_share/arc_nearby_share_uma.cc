// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/arc_nearby_share_uma.h"

#include "base/metrics/histogram_functions.h"

namespace arc {

void UpdateNearbyShareArcBridgeFail(ArcBridgeFailResult result) {
  base::UmaHistogramEnumeration("Arc.NearbyShare.ArcBridgeFailure", result);
}

void UpdateNearbyShareDataHandlingFail(DataHandlingResult result) {
  base::UmaHistogramEnumeration("Arc.NearbyShare.DataHandlingFailure", result);
}

void UpdateNearbyShareIOFail(IOErrorResult result) {
  base::UmaHistogramEnumeration("Arc.NearbyShare.IOFailure", result);
}

void UpdateNearbyShareWindowFound(bool found) {
  base::UmaHistogramBoolean("Arc.NearbyShare.WindowFound", found);
}

void UpdateNearbyShareFileStreamError(base::File::Error result) {
  // Maps to histogram enum PlatformFileError.
  base::UmaHistogramExactLinear("Arc.NearbyShare.FileStreamFailure", -result,
                                -base::File::FILE_ERROR_MAX);
}

void UpdateNearbyShareFileStreamCompleteTime(
    const base::TimeDelta& elapsed_time) {
  base::UmaHistogramCustomTimes("Arc.NearbyShare.FileStreamComplete.TimeDelta",
                                elapsed_time,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(30), /*buckets=*/50);
}
}  // namespace arc
