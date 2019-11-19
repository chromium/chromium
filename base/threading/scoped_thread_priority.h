// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_SCOPED_THREAD_PRIORITY_H_
#define BASE_THREADING_SCOPED_THREAD_PRIORITY_H_

#include "base/base_export.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "build/build_config.h"

namespace base {

class Location;
enum class ThreadPriority : int;

// This class must be instantiated in every scope where a DLL can be loaded on
// a background thread. On Windows, loading a DLL on a background thread can
// lead to a priority inversion on the loader lock and cause huge janks.
class BASE_EXPORT ScopedThreadMayLoadLibraryOnBackgroundThread {
 public:
  explicit ScopedThreadMayLoadLibraryOnBackgroundThread(
      const Location& from_here);
  ~ScopedThreadMayLoadLibraryOnBackgroundThread();

 private:
#if defined(OS_WIN)
  // The original priority when entering the scope.
  base::Optional<ThreadPriority> original_thread_priority_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ScopedThreadMayLoadLibraryOnBackgroundThread);
};

// Feature for mitigation of DLL loading on a background thread.
#if defined(OS_WIN)
BASE_EXPORT extern const base::Feature kBoostThreadPriorityOnLibraryLoading;
#endif  // OS_WIN

}  // namespace base

#endif  // BASE_THREADING_SCOPED_THREAD_PRIORITY_H_
