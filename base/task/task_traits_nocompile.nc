// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/task/task_traits.h"

namespace base {

// expected-error@base/traits_bag.h:* {{static assertion failed: The traits bag contains multiple traits of the same type.}}
// expected-note@+1 {{in instantiation of function template specialization}}
constexpr TaskTraits traits = {MayBlock(), MayBlock()};

// expected-error@base/traits_bag.h:* {{static assertion failed: The traits bag contains multiple traits of the same type.}}
// expected-note@+1 {{in instantiation of function template specialization}}
constexpr TaskTraits traits2 = {WithBaseSyncPrimitives(),
                                WithBaseSyncPrimitives()};

// expected-error@base/traits_bag.h:* 2 {{static assertion failed: The traits bag contains multiple traits of the same type.}}
// expected-note@+1 2 {{in instantiation of function template specialization}}
constexpr TaskTraits traits3 = {TaskPriority::BEST_EFFORT,
                                TaskPriority::USER_BLOCKING};

// expected-error@base/traits_bag.h:* 2 {{static assertion failed: The traits bag contains multiple traits of the same type.}}
// expected-note@+1 2 {{in instantiation of function template specialization}}
constexpr TaskTraits traits4 = {TaskShutdownBehavior::BLOCK_SHUTDOWN,
                                TaskShutdownBehavior::BLOCK_SHUTDOWN};

// expected-error@base/traits_bag.h:* 2 {{static assertion failed: The traits bag contains multiple traits of the same type.}}
// expected-note@+1 2 {{in instantiation of function template specialization}}
constexpr TaskTraits traits5 = {TaskShutdownBehavior::BLOCK_SHUTDOWN,
                                MayBlock(),
                                TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};

// expected-error@+3 {{no matching constructor for initialization of 'const TaskTraits'}}
// expected-error@*:* {{no matching constructor for initialization of 'base::TaskTraits::ValidTrait'}}
// expected-error@*:* {{no matching constructor for initialization of 'base::TaskTraits::ValidTraitInheritThreadType'}}
constexpr TaskTraits traits6 = {TaskShutdownBehavior::BLOCK_SHUTDOWN, true};

// expected-error@+2 {{no matching constructor for initialization of 'const TaskTraits'}}
// expected-error@*:* {{no matching constructor for initialization of 'base::TaskTraits::ValidTrait'}}
constexpr TaskTraits traits7 = {MaxThreadType(ThreadType::kDefault)};

// expected-error@+3 {{no matching constructor for initialization of 'const TaskTraits'}}
// expected-error@*:* {{no matching constructor for initialization of 'base::TaskTraits::ValidTrait'}}
// expected-error@*:* {{no matching constructor for initialization of 'base::TaskTraits::ValidTraitInheritThreadType'}}
constexpr TaskTraits traits8 = {InheritThreadType(), TaskPriority::USER_BLOCKING};

}  // namespace base
