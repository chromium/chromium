// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/native_library.h"

#include <dlfcn.h>

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_alloc_base/files/file_path.h"

namespace partition_alloc::internal::base {

std::string NativeLibraryLoadError::ToString() const {
  return message;
}

NativeLibrary LoadNativeLibraryWithOptions(const FilePath& library_path,
                                           const NativeLibraryOptions& options,
                                           NativeLibraryLoadError* error) {
  // TODO(crbug.com/40158212): Temporarily disable this ScopedBlockingCall.
  // After making partition_alloc ScopedBlockingCall() to see the same
  // blocking_observer_ in base's ScopedBlockingCall(), we will copy
  // ScopedBlockingCall code and will enable this.

  // dlopen() opens the file off disk.
  // ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);

  // We deliberately do not use RTLD_DEEPBIND by default.  For the history why,
  // please refer to the bug tracker.  Some useful bug reports to read include:
  // http://crbug.com/17943, http://crbug.com/17557, http://crbug.com/36892,
  // and http://crbug.com/40794.
  int flags = RTLD_LAZY;
#if PA_BUILDFLAG(IS_ANDROID) || !defined(RTLD_DEEPBIND)
  // Certain platforms don't define RTLD_DEEPBIND. Android dlopen() requires
  // further investigation, as it might vary across versions. Crash here to
  // warn developers that they're trying to rely on uncertain behavior.
  PA_BASE_CHECK(!options.prefer_own_symbols);
#else
  if (options.prefer_own_symbols) {
    flags |= RTLD_DEEPBIND;
  }
#endif
  void* dl = dlopen(library_path.value().c_str(), flags);
  if (!dl && error) {
    error->message = dlerror();
  }

  return dl;
}

void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          const std::string& name) {
  return dlsym(library, name.c_str());
}

}  // namespace partition_alloc::internal::base
