// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "partition_alloc/partition_alloc_base/native_library.h"

namespace partition_alloc::internal::base {

NativeLibrary LoadNativeLibrary(const FilePath& library_path,
                                NativeLibraryLoadError* error) {
  return LoadNativeLibraryWithOptions(library_path, NativeLibraryOptions(),
                                      error);
}

}  // namespace partition_alloc::internal::base
