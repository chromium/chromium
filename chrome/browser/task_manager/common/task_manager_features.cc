// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/common/task_manager_features.h"

#include "build/android_buildflags.h"

namespace features {

#if BUILDFLAG(IS_ANDROID)
// Enables the Task Manager on Clank.
BASE_FEATURE(kTaskManagerClank,
#if BUILDFLAG(IS_DESKTOP_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)
);
#else
// Enables the Task Manager Desktop Refresh project.
BASE_FEATURE(kTaskManagerDesktopRefresh, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace features
