// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/common/task_manager_features.h"

#include "build/android_buildflags.h"

namespace features {

#if BUILDFLAG(IS_ANDROID)
// Enables the Task Manager on Clank.
BASE_FEATURE(kTaskManagerClank,
             base::FEATURE_DISABLED_BY_DEFAULT
);

// Enables the Task Manager Toolbar (chips and search) on Android.
BASE_FEATURE(kTaskManagerToolbar, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace features
