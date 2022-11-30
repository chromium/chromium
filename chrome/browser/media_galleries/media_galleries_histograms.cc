// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_histograms.h"

#include "base/metrics/histogram_macros.h"

namespace media_galleries {

void UsageCount(MediaGalleriesUsages usage) {
  DCHECK_LT(usage, MEDIA_GALLERIES_NUM_USAGES);
  UMA_HISTOGRAM_ENUMERATION("MediaGalleries.Usage", usage,
                            MEDIA_GALLERIES_NUM_USAGES);
}

}  // namespace media_galleries
