// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"

#include <utility>

#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/global_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_installer.h"

namespace arc {
std::unique_ptr<ArcSessionManager> CreateTestArcSessionManager(
    std::unique_ptr<ArcSessionRunner> arc_session_runner,
    ArcDlcInstaller* arc_dlc_installer) {
  auto manager = std::make_unique<ArcSessionManager>(
      TestingBrowserProcess::GetGlobal()->local_state(),
      TestingBrowserProcess::GetGlobal()
          ->GetFeatures()
          ->application_locale_storage(),
      std::move(arc_session_runner),
      std::make_unique<AdbSideloadingAvailabilityDelegateImpl>(),
      arc_dlc_installer);
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
