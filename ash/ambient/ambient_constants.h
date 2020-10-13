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

// The default interval to fetch backup cache photos.
constexpr base::TimeDelta kBackupPhotoRefreshDelay =
    base::TimeDelta::FromMinutes(5);

// The default interval to refresh weather.
constexpr base::TimeDelta kWeatherRefreshInterval =
    base::TimeDelta::FromMinutes(5);

// The delay between ambient mode starts and enabling lock screen.
constexpr base::TimeDelta kLockScreenDelay = base::TimeDelta::FromSeconds(5);

// The batch size of topics to fetch in one request.
constexpr int kTopicsBatchSize = 100;

// Max cached images.
constexpr int kMaxNumberOfCachedImages = 100;

constexpr int kMaxImageSizeInBytes = 5 * 1024 * 1024;

constexpr int kMaxReservedAvailableDiskSpaceByte = 200 * 1024 * 1024;

// The maximum number of consecutive failures in downloading or reading an image
// from disk.
constexpr int kMaxConsecutiveReadPhotoFailures = 3;

constexpr char kPhotoFileExt[] = ".img";
constexpr char kPhotoDetailsFileExt[] = ".txt";

// Directory name of ambient mode.
constexpr char kAmbientModeDirectoryName[] = "ambient-mode";

constexpr char kAmbientModeCacheDirectoryName[] = "cache";

constexpr char kAmbientModeBackupCacheDirectoryName[] = "backup";

// The buffer time to use the access token.
constexpr base::TimeDelta kTokenUsageTimeBuffer =
    base::TimeDelta::FromMinutes(10);

// PhotoView related constants.
// Spacing between two portrait images.
constexpr int kMarginLeftOfRelatedImageDip = 8;

// Media string related.
constexpr int kMediaStringMaxWidthDip = 280;

constexpr int kMediaStringGradientWidthDip = 20;

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_CONSTANTS_H_
