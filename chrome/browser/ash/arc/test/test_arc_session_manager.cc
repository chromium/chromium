// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"

#include <utility>

#include "chrome/browser/ash/arc/session/arc_session_manager.h"

namespace arc {
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
