// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_util.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"

namespace ash {
namespace glanceables_util {

base::FilePath GetSignoutScreenshotPath() {
  base::FilePath home_dir;
  CHECK(base::PathService::Get(base::DIR_HOME, &home_dir));
  return home_dir.AppendASCII("signout_screenshot.png");
}

}  // namespace glanceables_util
}  // namespace ash
