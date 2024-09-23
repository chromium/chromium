// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#include <windows.h>

#include <string_view>

#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_thread_priority.h"

namespace base {

namespace {

NativeLibrary LoadNativeLibraryHelper(const FilePath& library_path,
                                      NativeLibraryLoadError* error) {
  // LoadLibrary() opens the file off disk and acquires the LoaderLock, hence
  // must not be called from DllMain.
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  // Mitigate the issues caused by loading DLLs on a background thread
  // (see http://crbug/973868 for context). This temporarily boosts this
  // thread's priority so that it doesn't get starved by higher priority threads
  // while it holds the LoaderLock.
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY_REPEATEDLY();

  HMODULE module_handle = nullptr;

  // LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR flag is needed to search the library
  // directory as the library may have dependencies on DLLs in this
  // directory.
  module_handle = ::LoadLibraryExW(
      library_path.value().c_str(), nullptr,
      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  // If LoadLibraryExW succeeds, log this metric and return.
  if (module_handle) {
    return module_handle;
  }
  // GetLastError() needs to be called immediately after
  // LoadLibraryExW call.
  if (error) {
    error->code = ::GetLastError();
  }

  // If LoadLibraryExW API/flags are unavailable or API call fails, try
  // LoadLibraryW API. From UMA, this fallback is necessary for many users.

  // Switch the current directory to the library directory as the library
  // may have dependencies on DLLs in this directory.
  bool restore_directory = false;
  FilePath current_directory;
  if (GetCurrentDirectory(&current_directory)) {
    FilePath plugin_path = library_path.DirName();
    if (!plugin_path.empty()) {
      SetCurrentDirectory(plugin_path);
      restore_directory = true;
    }
  }
  module_handle = ::LoadLibraryW(library_path.value().c_str());

  // GetLastError() needs to be called immediately after LoadLibraryW call.
  if (!module_handle && error) {
    error->code = ::GetLastError();
  }

  if (restore_directory)
    SetCurrentDirectory(current_directory);

  return module_handle;
}

NativeLibrary LoadSystemLibraryHelper(const FilePath& library_path,
                                      NativeLibraryLoadError* error) {
  // GetModuleHandleEx and subsequently LoadLibraryEx acquire the LoaderLock,
  // hence must not be called from Dllmain.
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  NativeLibrary module;
  BOOL module_found =
      ::GetModuleHandleExW(0, library_path.value().c_str(), &module);
  if (!module_found) {
    module = ::LoadLibraryExW(library_path.value().c_str(), nullptr,
                              LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (!module && error)
      error->code = ::GetLastError();
  }

  return module;
}

FilePath GetSystemLibraryName(FilePath::StringPieceType name) {
  FilePath library_path;
  // Use an absolute path to load the DLL to avoid DLL preloading attacks.
  if (PathService::Get(DIR_SYSTEM, &library_path))
    library_path = library_path.Append(name);
  return library_path;
}

}  // namespace

std::string NativeLibraryLoadError::ToString() const {
  return StringPrintf("%lu", code);
}

NativeLibrary LoadNativeLibraryWithOptions(const FilePath& library_path,
                                           const NativeLibraryOptions& options,
                                           NativeLibraryLoadError* error) {
  return LoadNativeLibraryHelper(library_path, error);
}

void UnloadNativeLibrary(NativeLibrary library) {
  FreeLibrary(library);
}

void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          const char* name) {
  return reinterpret_cast<void*>(GetProcAddress(library, name));
}

std::string GetNativeLibraryName(std::string_view name) {
  DCHECK(IsStringASCII(name));
  return StrCat({name, ".dll"});
}

std::string GetLoadableModuleName(std::string_view name) {
  return GetNativeLibraryName(name);
}

NativeLibrary LoadSystemLibrary(FilePath::StringPieceType name,
                                NativeLibraryLoadError* error) {
  FilePath library_path = GetSystemLibraryName(name);
  if (library_path.empty()) {
    if (error)
      error->code = ERROR_NOT_FOUND;
    return nullptr;
  }
  return LoadSystemLibraryHelper(library_path, error);
}

NativeLibrary PinSystemLibrary(FilePath::StringPieceType name,
                               NativeLibraryLoadError* error) {
  FilePath library_path = GetSystemLibraryName(name);
  if (library_path.empty()) {
    if (error)
      error->code = ERROR_NOT_FOUND;
    return nullptr;
  }

  // GetModuleHandleEx acquires the LoaderLock, hence must not be called from
  // Dllmain.
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  ScopedNativeLibrary module;
  if (::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN,
                           library_path.value().c_str(),
                           ScopedNativeLibrary::Receiver(module).get())) {
    return module.release();
  }

  // Load and pin the library since it wasn't already loaded.
  module = ScopedNativeLibrary(LoadSystemLibraryHelper(library_path, error));
  if (!module.is_valid())
    return nullptr;

  ScopedNativeLibrary temp;
  if (::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN,
                           library_path.value().c_str(),
                           ScopedNativeLibrary::Receiver(temp).get())) {
    return module.release();
  }

  if (error)
    error->code = ::GetLastError();
  // Return nullptr since we failed to pin the module.
  return nullptr;
}

}  // namespace base
