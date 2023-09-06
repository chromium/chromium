// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_CONSTANTS_H_
#define ASH_AMBIENT_AMBIENT_CONSTANTS_H_

#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/time/time.h"

namespace ash {

// Duration of the slide show animation. Also used as |delay| in posted task to
// download images.
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(500);

// Topic related numbers.

// The default interval to fetch Topics.
constexpr base::TimeDelta kTopicFetchInterval = base::Seconds(30);

// The default interval to fetch backup cache photos.
constexpr base::TimeDelta kBackupPhotoRefreshDelay = base::Minutes(5);

// The default interval to refresh weather.
constexpr base::TimeDelta kWeatherRefreshInterval = base::Minutes(5);

// The batch size of topics to fetch in one request.
constexpr int kTopicsBatchSize = 100;

// Max cached images.
constexpr int kMaxNumberOfCachedImages = 100;

constexpr int kMaxImageSizeInBytes = 5 * 1024 * 1024;

constexpr int kMaxReservedAvailableDiskSpaceByte = 200 * 1024 * 1024;

// The maximum number of consecutive failures in downloading or reading an image
// from disk.
constexpr int kMaxConsecutiveReadPhotoFailures = 3;

constexpr char kPhotoCacheExt[] = ".cache";

// Directory name of ambient mode.
constexpr char kAmbientModeDirectoryName[] = "ambient-mode";

constexpr char kAmbientModeCacheDirectoryName[] = "cache";

constexpr char kAmbientModeBackupCacheDirectoryName[] = "backup";

// The buffer time to use the access token.
constexpr base::TimeDelta kTokenUsageTimeBuffer = base::Minutes(10);

// PhotoView related constants.
// Spacing between two portrait images.
constexpr int kMarginLeftOfRelatedImageDip = 8;

// Media string related.
constexpr int kMediaStringMaxWidthDip = 280;

constexpr int kMediaStringGradientWidthDip = 20;

// UMA user action constants.
constexpr char kScreenSaverPreviewUserAction[] =
    "AmbientMode.ScreenSaverPreview.Started";

inline constexpr personalization_app::mojom::AmbientTheme kDefaultAmbientTheme =
    personalization_app::mojom::AmbientTheme::kSlideshow;

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_CONSTANTS_H_
