// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"

namespace arc {
namespace {

bool CreateFilesAndDirectories(const base::FilePath& temp_dir,
                               base::FilePath* source_dir,
                               base::FilePath* dest_dir) {
  if (!base::CreateTemporaryDirInDir(temp_dir, "source", source_dir))
    return false;

  // Create empty prop files so ArcSessionManager's property expansion code
  // works like production.
  for (const char* filename :
       {"default.prop", "build.prop", "vendor_build.prop",
        "system_ext_build.prop", "product_build.prop", "odm_build.prop"}) {
    if (base::WriteFile(source_dir->Append(filename), "", 1) != 1)
      return false;
  }

  return base::CreateTemporaryDirInDir(temp_dir, "dest", dest_dir);
}

}  // namespace

std::unique_ptr<ArcSessionManager> CreateTestArcSessionManager(
    std::unique_ptr<ArcSessionRunner> arc_session_runner) {
  auto manager = std::make_unique<ArcSessionManager>(
      std::move(arc_session_runner),
      std::make_unique<AdbSideloadingAvailabilityDelegateImpl>());
  // Our unit tests the ArcSessionManager::ExpandPropertyFiles() function won't
  // be automatically called. Because of that, we can call
  // OnExpandPropertyFilesForTesting() instead with |true| for easier unit
  // testing (without calling base::RunLoop().RunUntilIdle() here and there.)
  manager->OnExpandPropertyFilesAndReadSaltForTesting(true);
  return manager;
}

void ExpandPropertyFilesForTesting(ArcSessionManager* arc_session_manager) {
  arc_session_manager->OnExpandPropertyFilesAndReadSaltForTesting(true);
}

}  // namespace arc
