// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/preloading_features.h"

namespace features {

// Forces Chrome to use the Preload pages settings sub page on desktop
// platforms. This allows a user to choose between no preloading, standard
// preloading, and extended preloading.
BASE_FEATURE(kPreloadingDesktopSettingsSubPage,
             "PreloadingDesktopSettingsSubPage",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
