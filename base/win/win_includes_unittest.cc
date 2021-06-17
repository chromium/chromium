// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file ensures that these header files don't include Windows.h and can
// compile without including Windows.h. This helps to improve compile times.

#include "base/atomicops.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/process/process_handle.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_local_storage.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"

#ifdef _WINDOWS_
#error Windows.h was included inappropriately.
#endif

// Make sure windows.h can be included after windows_types.h
#include "base/win/windows_types.h"

#include <windows.h>

// Check that type sizes match.
static_assert(sizeof(CHROME_CONDITION_VARIABLE) == sizeof(CONDITION_VARIABLE),
              "Definition mismatch.");
static_assert(sizeof(CHROME_SRWLOCK) == sizeof(SRWLOCK),
              "Definition mismatch.");
