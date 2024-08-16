// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#include <dlfcn.h>

#include <string_view>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"

namespace base {

std::string NativeLibraryLoadError::ToString() const {
  return message;
}

NativeLibrary LoadNativeLibraryWithOptions(const FilePath& library_path,
                                           const NativeLibraryOptions& options,
                                           NativeLibraryLoadError* error) {
  // dlopen() opens the file off disk.
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  // We deliberately do not use RTLD_DEEPBIND by default.  For the history why,
  // please refer to the bug tracker.  Some useful bug reports to read include:
  // http://crbug.com/17943, http://crbug.com/17557, http://crbug.com/36892,
  // and http://crbug.com/40794.
  int flags = RTLD_LAZY;
#if BUILDFLAG(IS_ANDROID) || !defined(RTLD_DEEPBIND)
  // Certain platforms don't define RTLD_DEEPBIND. Android dlopen() requires
  // further investigation, as it might vary across versions. Crash here to
  // warn developers that they're trying to rely on uncertain behavior.
  CHECK(!options.prefer_own_symbols);
#else
  if (options.prefer_own_symbols)
    flags |= RTLD_DEEPBIND;
#endif
  void* dl = dlopen(library_path.value().c_str(), flags);
  if (!dl && error)
    error->message = dlerror();

  return dl;
}

void UnloadNativeLibrary(NativeLibrary library) {
  int ret = dlclose(library);
  if (ret < 0) {
    DLOG(ERROR) << "dlclose failed: " << dlerror();
    NOTREACHED();
  }
}

void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          const char* name) {
  return dlsym(library, name);
}

std::string GetNativeLibraryName(std::string_view name) {
  DCHECK(IsStringASCII(name));
  return StrCat({"lib", name, ".so"});
}

std::string GetLoadableModuleName(std::string_view name) {
  return GetNativeLibraryName(name);
}

}  // namespace base
