// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_features.h"
#include "base/feature_list.h"

namespace base::android::features {

// Alphabetical:

// When the browser process has been in the background for several minutes at a
// time, trigger an artificial critical memory pressure notification. This is
// intended to reduce memory footprint.
BASE_FEATURE(kBrowserProcessMemoryPurge,
             "BrowserProcessMemoryPurge",
             FEATURE_ENABLED_BY_DEFAULT);

}  // namespace base::android::features
