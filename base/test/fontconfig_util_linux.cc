// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/fontconfig_util_linux.h"

#include <fontconfig/fontconfig.h>

#include <memory>

#include "base/base_paths.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"

namespace base {

void SetUpFontconfig() {
  FilePath dir_module;
  CHECK(PathService::Get(DIR_MODULE, &dir_module));

  std::unique_ptr<Environment> env(Environment::Create());
  CHECK(env->SetVar("FONTCONFIG_SYSROOT", dir_module.value().c_str()));
}

}  // namespace base
