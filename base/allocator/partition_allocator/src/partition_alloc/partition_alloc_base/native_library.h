// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_NATIVE_LIBRARY_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_NATIVE_LIBRARY_H_

// This file defines a cross-platform "NativeLibrary" type which represents
// a loadable module.

#include <string>

#include "build/build_config.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/files/file_path.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#elif BUILDFLAG(IS_APPLE)
#import <CoreFoundation/CoreFoundation.h>
#endif  // OS_*

namespace partition_alloc::internal::base {

#if BUILDFLAG(IS_WIN)
using NativeLibrary = HMODULE;
#elif BUILDFLAG(IS_APPLE)
enum NativeLibraryType { BUNDLE, DYNAMIC_LIB };
enum NativeLibraryObjCStatus {
  OBJC_UNKNOWN,
  OBJC_PRESENT,
  OBJC_NOT_PRESENT,
};
struct NativeLibraryStruct {
  NativeLibraryType type;
  CFBundleRefNum bundle_resource_ref;
  NativeLibraryObjCStatus objc_status;
  union {
    CFBundleRef bundle;
    void* dylib;
  };
};
using NativeLibrary = NativeLibraryStruct*;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
using NativeLibrary = void*;
#endif  // OS_*

struct PA_COMPONENT_EXPORT(PARTITION_ALLOC_BASE) NativeLibraryLoadError {
#if BUILDFLAG(IS_WIN)
  NativeLibraryLoadError() : code(0) {}
#endif  // BUILDFLAG(IS_WIN)

  // Returns a string representation of the load error.
  std::string ToString() const;

#if BUILDFLAG(IS_WIN)
  DWORD code;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  std::string message;
#endif  // BUILDFLAG(IS_WIN)
};

struct PA_COMPONENT_EXPORT(PARTITION_ALLOC_BASE) NativeLibraryOptions {
  NativeLibraryOptions() = default;
  NativeLibraryOptions(const NativeLibraryOptions& options) = default;

  // If |true|, a loaded library is required to prefer local symbol resolution
  // before considering global symbols. Note that this is already the default
  // behavior on most systems. Setting this to |false| does not guarantee the
  // inverse, i.e., it does not force a preference for global symbols over local
  // ones.
  bool prefer_own_symbols = false;
};

// Loads a native library from disk.  Release it with UnloadNativeLibrary when
// you're done.  Returns NULL on failure.
// If |error| is not NULL, it may be filled in on load error.
PA_COMPONENT_EXPORT(PARTITION_ALLOC_BASE)
NativeLibrary LoadNativeLibrary(const FilePath& library_path,
                                NativeLibraryLoadError* error);

// Loads a native library from disk.  Release it with UnloadNativeLibrary when
// you're done.  Returns NULL on failure.
// If |error| is not NULL, it may be filled in on load error.
PA_COMPONENT_EXPORT(PARTITION_ALLOC_BASE)
NativeLibrary LoadNativeLibraryWithOptions(const FilePath& library_path,
                                           const NativeLibraryOptions& options,
                                           NativeLibraryLoadError* error);

// Gets a function pointer from a native library.
PA_COMPONENT_EXPORT(PARTITION_ALLOC_BASE)
void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          const std::string& name);

}  // namespace partition_alloc::internal::base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_NATIVE_LIBRARY_H_
