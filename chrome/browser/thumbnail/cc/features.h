// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_CC_FEATURES_H_
#define CHROME_BROWSER_THUMBNAIL_CC_FEATURES_H_

#include "base/feature_list.h"

namespace thumbnail {

// A refactoring of the ThumbnailCache and TabContentManager that aims to
// optimize performance of the system.
BASE_DECLARE_FEATURE(kThumbnailCacheRefactor);

}  // namespace thumbnail

#endif  // CHROME_BROWSER_THUMBNAIL_CC_FEATURES_H_
