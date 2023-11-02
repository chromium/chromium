// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/task/task_traits.h"

namespace base {

constexpr TaskTraits traits = {MayBlock(), MayBlock()};  // expected-error {{constexpr variable 'traits' must be initialized by a constant expression}}
                                                         // expected-error@base/traits_bag.h:* {{The traits bag contains multiple traits of the same type.}}

constexpr TaskTraits traits2 = {WithBaseSyncPrimitives(),   // expected-error {{constexpr variable 'traits2' must be initialized by a constant expression}}
                                WithBaseSyncPrimitives()};  // expected-error@base/traits_bag.h:* {{The traits bag contains multiple traits of the same type.}}

constexpr TaskTraits traits3 = {TaskPriority::BEST_EFFORT,     // expected-error {{constexpr variable 'traits3' must be initialized by a constant expression}}
                                TaskPriority::USER_BLOCKING};  // expected-error@base/traits_bag.h:* {{The traits bag contains multiple traits of the same type.}}
                                                               // expected-error@*:* {{type occurs more than once in type list}}

// Note: the three repetitions of "The traits bag contains multiple traits of
// the same type." is *not* an error. Writing this really does cause three
// occurrences of the same error message!
constexpr TaskTraits traits4 = {TaskShutdownBehavior::BLOCK_SHUTDOWN,   // expected-error {{constexpr variable 'traits4' must be initialized by a constant expression}}
                                TaskShutdownBehavior::BLOCK_SHUTDOWN};  // expected-error@base/traits_bag.h:* {{The traits bag contains multiple traits of the same type.}}
                                                                        // expected-error@base/traits_bag.h:* {{The traits bag contains multiple traits of the same type.}}
                                                                        // expected-error@*:* {{type occurs more than once in type list}}
                                                                        // expected-error@base/traits_bag.h:* {{The traits bag contains multiple traits of the same type.}}

constexpr TaskTraits traits5 = {TaskShutdownBehavior::BLOCK_SHUTDOWN,         // expected-error {{constexpr variable 'traits5' must be initialized by a constant expression}}
                                MayBlock(),                                   // expected-error@base/traits_bag.h:* {{The traits bag contains multiple traits of the same type.}}
                                TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};  // expected-error@*:* {{type occurs more than once in type list}}

constexpr TaskTraits traits6 = {TaskShutdownBehavior::BLOCK_SHUTDOWN, true};  // expected-error {{no matching constructor for initialization of 'const TaskTraits'}}

}  // namespace base
