// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DCHECK_IS_ON_H_
#define BASE_DCHECK_IS_ON_H_

#include "base/debug/debugging_buildflags.h"

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
#define DCHECK_IS_ON() false
#else
#define DCHECK_IS_ON() true
#endif

#if BUILDFLAG(EXPENSIVE_DCHECKS_ARE_ON)
#define EXPENSIVE_DCHECKS_ARE_ON() true
#else
#define EXPENSIVE_DCHECKS_ARE_ON() false
#endif

// Equivalent to EXPENSIVE_DCHECKS_ARE_ON(), but used to gate expensive test
// logic rather than assertions so that it runs on CI and local developer builds
// without slowing down CQ trybuilders.
#if BUILDFLAG(EXPENSIVE_TEST_LOGIC_IS_ON)
#define EXPENSIVE_TEST_LOGIC_IS_ON() true
#else
#define EXPENSIVE_TEST_LOGIC_IS_ON() false
#endif

#if BUILDFLAG(ENABLE_CHECK_ELISION_WARNING)
#define ENABLE_CHECK_ELISION_WARNING() true
#else
#define ENABLE_CHECK_ELISION_WARNING() false
#endif

#endif  // BASE_DCHECK_IS_ON_H_
