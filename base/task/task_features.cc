// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_features.h"

#include "base/feature_list.h"

namespace base {

const Feature kAllTasksUserBlocking{"AllTasksUserBlocking",
                                    FEATURE_DISABLED_BY_DEFAULT};

// This experiment no longer has any impact, but remains enabled by default
// because script streamer depends on it.
// TODO(etiennep): Cleanup this experiment.
const Feature kMergeBlockingNonBlockingPools = {
    "MergeBlockingNonBlockingPools", base::FEATURE_ENABLED_BY_DEFAULT};

const Feature kNoDetachBelowInitialCapacity = {
    "NoDetachBelowInitialCapacity", base::FEATURE_DISABLED_BY_DEFAULT};

const Feature kMayBlockWithoutDelay = {"MayBlockWithoutDelay",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_WIN) || defined(OS_MACOSX)
const Feature kUseNativeThreadPool = {"UseNativeThreadPool",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const Feature kUseFiveMinutesThreadReclaimTime = {
    "UseFiveMinutesThreadReclaimTime", base::FEATURE_DISABLED_BY_DEFAULT};

const Feature kNoPriorityInheritanceFromThreadPool{
    "NoPriorityInheritanceFromThreadPool", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace base
