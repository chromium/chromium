// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/task/task_traits.h"

namespace base {

#if defined(NCTEST_TASK_TRAITS_MULTIPLE_MAY_BLOCK)  // [r"The traits bag contains multiple traits of the same type."]
constexpr TaskTraits traits = {MayBlock(), MayBlock()};
#elif defined(NCTEST_TASK_TRAITS_MULTIPLE_WITH_BASE_SYNC_PRIMITIVES)  // [r"The traits bag contains multiple traits of the same type."]
constexpr TaskTraits traits = {WithBaseSyncPrimitives(),
                               WithBaseSyncPrimitives()};
#elif defined(NCTEST_TASK_TRAITS_MULTIPLE_TASK_PRIORITY)  // [r"The traits bag contains multiple traits of the same type."]
constexpr TaskTraits traits = {TaskPriority::BEST_EFFORT,
                               TaskPriority::USER_BLOCKING};
#elif defined(NCTEST_TASK_TRAITS_MULTIPLE_SHUTDOWN_BEHAVIOR)  // [r"The traits bag contains multiple traits of the same type."]
constexpr TaskTraits traits = {TaskShutdownBehavior::BLOCK_SHUTDOWN,
                               TaskShutdownBehavior::BLOCK_SHUTDOWN};
#elif defined(NCTEST_TASK_TRAITS_MULTIPLE_SAME_TYPE_MIX)  // [r"The traits bag contains multiple traits of the same type."]
constexpr TaskTraits traits = {TaskShutdownBehavior::BLOCK_SHUTDOWN,
                               MayBlock(),
                               TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
#elif defined(NCTEST_TASK_TRAITS_INVALID_TYPE)  // [r"no matching constructor for initialization of 'const base::TaskTraits'"]
constexpr TaskTraits traits = {TaskShutdownBehavior::BLOCK_SHUTDOWN, true};
#elif defined(NCTEST_TASK_TRAITS_CURRENT_THREAD_AND_THREADPOOL)  // [r"base::CurrentThread is mutually exclusive with base::ThreadPool"]
constexpr TaskTraits traits = {ThreadPool(), CurrentThread()};
#endif

}  // namespace base
