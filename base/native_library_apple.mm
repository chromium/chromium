// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#include <dlfcn.h>
#include <mach-o/getsect.h>

#include <string_view>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"

namespace base {

std::string NativeLibraryLoadError::ToString() const {
  return message;
}

NativeLibrary LoadNativeLibraryWithOptions(const FilePath& library_path,
                                           const NativeLibraryOptions& options,
                                           NativeLibraryLoadError* error) {
  // dlopen() etc. open the file off disk.
  if (library_path.Extension() == "dylib" || !DirectoryExists(library_path)) {
    void* dylib = dlopen(library_path.value().c_str(), RTLD_LAZY);
    if (!dylib) {
      if (error) {
        error->message = dlerror();
      }
      return nullptr;
    }
    NativeLibrary native_lib = new NativeLibraryStruct();
    native_lib->type = DYNAMIC_LIB;
    native_lib->dylib = dylib;
    return native_lib;
  }
  apple::ScopedCFTypeRef<CFURLRef> url = apple::FilePathToCFURL(library_path);
  if (!url) {
    return nullptr;
  }
  CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, url.get());
  if (!bundle) {
    return nullptr;
  }

  NativeLibrary native_lib = new NativeLibraryStruct();
  native_lib->type = BUNDLE;
  native_lib->bundle = bundle;
  return native_lib;
}

void UnloadNativeLibrary(NativeLibrary library) {
  if (library->type == BUNDLE) {
    CFRelease(library->bundle);
  } else {
    dlclose(library->dylib);
  }
  delete library;
}

void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          const char* name) {
  // Get the function pointer using the right API for the type.
  if (library->type == BUNDLE) {
    apple::ScopedCFTypeRef<CFStringRef> symbol_name =
        SysUTF8ToCFStringRef(name);
    return CFBundleGetFunctionPointerForName(library->bundle,
                                             symbol_name.get());
  }

  return dlsym(library->dylib, name);
}

std::string GetNativeLibraryName(std::string_view name) {
  DCHECK(IsStringASCII(name));
#if BUILDFLAG(IS_IOS)
  // Returns Frameworks/mylib.framework/mylib
  return FilePath()
      .Append("Frameworks")
      .Append(name)
      .AddExtension("framework")
      .Append(name)
      .value();
#else
  return StrCat({"lib", name, ".dylib"});
#endif
}

std::string GetLoadableModuleName(std::string_view name) {
  DCHECK(IsStringASCII(name));
#if BUILDFLAG(IS_IOS)
  // Returns Frameworks/mylib.framework
  return FilePath()
      .Append("Frameworks")
      .Append(name)
      .AddExtension("framework")
      .value();
#else
  return StrCat({name, ".so"});
#endif
}

}  // namespace base
