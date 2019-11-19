// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_features.h"

namespace features {

#if defined(OS_CHROMEOS)
// Enables being able to zoom a web page by double tapping in Chrome OS tablet
// mode.
const base::Feature kDoubleTapToZoomInTabletMode{
    "DoubleTapToZoomInTabletMode", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
const base::Feature kSyncClipboardServiceFeature{
    "SyncClipboardService", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // OS_WIN || OS_MACOSX || OS_LINUX

}  // namespace features
