// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_features.h"

namespace features {

// Enables the Task Manager on Clank.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kTaskManagerClank,
             "TaskManagerClank",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace features
