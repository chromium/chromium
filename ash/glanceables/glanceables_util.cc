// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_util.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"

namespace ash::glanceables_util {

base::FilePath GetSignoutScreenshotPath() {
  base::FilePath home_dir;
  CHECK(base::PathService::Get(base::DIR_HOME, &home_dir));
  return home_dir.AppendASCII("signout_screenshot.png");
}

}  // namespace ash::glanceables_util
