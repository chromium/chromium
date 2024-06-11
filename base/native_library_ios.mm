// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#include <string_view>

#include "base/check.h"
#include "base/notimplemented.h"
#include "base/strings/string_util.h"

namespace base {

std::string NativeLibraryLoadError::ToString() const {
  return message;
}

NativeLibrary LoadNativeLibraryWithOptions(const base::FilePath& library_path,
                                           const NativeLibraryOptions& options,
                                           NativeLibraryLoadError* error) {
  NOTIMPLEMENTED();
  if (error)
    error->message = "Not implemented.";
  return nullptr;
}

void UnloadNativeLibrary(NativeLibrary library) {
  NOTIMPLEMENTED();
  DCHECK(!library);
}

void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          const char* name) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::string GetNativeLibraryName(std::string_view name) {
  DCHECK(IsStringASCII(name));
  return std::string(name);
}

std::string GetLoadableModuleName(std::string_view name) {
  return GetNativeLibraryName(name);
}

}  // namespace base
