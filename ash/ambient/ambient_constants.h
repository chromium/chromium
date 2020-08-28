// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_CONSTANTS_H_
#define ASH_AMBIENT_AMBIENT_CONSTANTS_H_

#include "base/time/time.h"

namespace ash {

// Duration of the slide show animation. Also used as |delay| in posted task to
// download images.
constexpr base::TimeDelta kAnimationDuration =
    base::TimeDelta::FromMilliseconds(500);

// Topic related numbers.

// The default interval to fetch Topics.
constexpr base::TimeDelta kTopicFetchInterval =
    base::TimeDelta::FromSeconds(30);

// The default interval to refresh photos.
constexpr base::TimeDelta kPhotoRefreshInterval =
    base::TimeDelta::FromSeconds(60);

// The number of requests to fetch topics.
constexpr int kNumberOfRequests = 50;

// The batch size of topics to fetch in one request.
// Magic number 2 is based on experiments that no curation on Google Photos.
constexpr int kTopicsBatchSize = 2;

// Max cached images.
constexpr int kMaxNumberOfCachedImages = 100;

constexpr int kMaxImageSizeInBytes = 5 * 1024 * 1024;

constexpr int kMaxReservedAvailableDiskSpaceByte = 200 * 1024 * 1024;

constexpr char kPhotoFileExt[] = ".img";
constexpr char kPhotoDetailsFileExt[] = ".txt";

// Directory name of ambient mode.
constexpr char kAmbientModeDirectoryName[] = "ambient-mode";

// The buffer time to use the access token.
constexpr base::TimeDelta kTokenUsageTimeBuffer =
    base::TimeDelta::FromMinutes(10);

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_CONSTANTS_H_
