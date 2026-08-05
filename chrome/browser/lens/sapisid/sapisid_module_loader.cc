// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/sapisid/sapisid_module_loader.h"

#include "base/base_paths.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/lens/sapisid/sapisid_module_api.h"

namespace sapisid {

// static
SapisidModuleLoader* SapisidModuleLoader::GetInstance() {
  static base::NoDestructor<SapisidModuleLoader> instance;
  return instance.get();
}

DISABLE_CFI_DLSYM
SapisidModuleLoader::SapisidModuleLoader() {
#if BUILDFLAG(IS_ANDROID)
  base::FilePath library_path(base::GetNativeLibraryName("sapisid"));
#else
  base::FilePath dir_exe;
  if (!base::PathService::Get(base::DIR_MODULE, &dir_exe)) {
    LOG(ERROR) << "Failed to get DIR_MODULE";
    return;
  }

#if BUILDFLAG(IS_MAC)
  dir_exe = dir_exe.AppendASCII("Libraries");
#endif

  base::FilePath library_path =
      dir_exe.AppendASCII(base::GetNativeLibraryName("sapisid"));
#endif

  native_library_ = base::ScopedNativeLibrary(library_path);

  if (native_library_.is_valid()) {
    typedef void (*InitFunc)();
    InitFunc init_func = reinterpret_cast<InitFunc>(
        native_library_.GetFunctionPointer("SAPI_Initialize"));
    if (init_func) {
      init_func();
    } else {
      LOG(ERROR) << "Found SAPISID module but missing SAPI_Initialize";
    }
  } else {
    // Suppress error in test environments if not found, but we can just log
    // error here.
    LOG(ERROR) << "Failed to load SAPISID module: "
               << native_library_.GetError()->ToString();
  }
}

SapisidModuleLoader::~SapisidModuleLoader() = default;

}  // namespace sapisid
