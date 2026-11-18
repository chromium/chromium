// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/launchpad/features.h"

namespace launchpad::features {

// The main feature flag for the Launchpad service. When disabled,
// all Launchpad features and services are turned off.
BASE_FEATURE(kLaunchpad, base::FEATURE_DISABLED_BY_DEFAULT);

// The maximum number of tasks stored in the in-memory `TasksStore`.
BASE_FEATURE_PARAM(size_t, kMaxTasksCacheSize, &kLaunchpad, 150);

}  // namespace launchpad::features
