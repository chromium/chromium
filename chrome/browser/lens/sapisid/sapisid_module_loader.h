// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_SAPISID_SAPISID_MODULE_LOADER_H_
#define CHROME_BROWSER_LENS_SAPISID_SAPISID_MODULE_LOADER_H_
#include "base/no_destructor.h"
#include "base/scoped_native_library.h"

namespace sapisid {

class SapisidModuleLoader {
 public:
  static SapisidModuleLoader* GetInstance();

  SapisidModuleLoader(const SapisidModuleLoader&) = delete;
  SapisidModuleLoader& operator=(const SapisidModuleLoader&) = delete;

  const base::ScopedNativeLibrary& library() const { return native_library_; }

 private:
  friend base::NoDestructor<SapisidModuleLoader>;

  SapisidModuleLoader();
  ~SapisidModuleLoader();

  base::ScopedNativeLibrary native_library_;
};

}  // namespace sapisid

#endif  // CHROME_BROWSER_LENS_SAPISID_SAPISID_MODULE_LOADER_H_
