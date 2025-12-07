// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_LIBRARY_LOADER_LIBRARY_PREFETCHER_H_
#define BASE_ANDROID_LIBRARY_LOADER_LIBRARY_PREFETCHER_H_

#include <jni.h>
#include <stdint.h>

#include "base/android/library_loader/anchor_functions_buildflags.h"
#include "base/base_export.h"

#if BUILDFLAG(SUPPORTS_CODE_ORDERING)

namespace base {
namespace android {

// Prefetches the native library. This may be done in a forked process for the
// following reasons:
// - Isolating the main process from mistakes in getting the address range, only
//   crashing the forked process in case of mistake.
// - Not inflating the memory used by the main process uselessly, which could
//   increase its likelihood to be killed.
// The forked process has background priority and, since it is not declared to
// the Android runtime, can be killed at any time, which is not an issue here.
//
// An alternative in-process madvise prefetch can be used if the
// "LibraryPrefetcherMadvise" feature is enabled.
class BASE_EXPORT NativeLibraryPrefetcher {
 public:
  NativeLibraryPrefetcher() = delete;
  NativeLibraryPrefetcher(const NativeLibraryPrefetcher&) = delete;
  NativeLibraryPrefetcher& operator=(const NativeLibraryPrefetcher&) = delete;

  // Finds the executable code range and prefetches it. It fetches any ordered
  // section first.
  static void PrefetchNativeLibrary();
};

}  // namespace android
}  // namespace base

#endif  // BUILDFLAG(SUPPORTS_CODE_ORDERING)

#endif  // BASE_ANDROID_LIBRARY_LOADER_LIBRARY_PREFETCHER_H_
