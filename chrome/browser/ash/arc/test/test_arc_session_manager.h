// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TEST_TEST_ARC_SESSION_MANAGER_H_
#define CHROME_BROWSER_ASH_ARC_TEST_TEST_ARC_SESSION_MANAGER_H_

#include <memory>

namespace arc {

class ArcSessionManager;
class ArcSessionRunner;

// Creates an ArcSessionManager object that is suitable for unit testing.
// Unlike the regular one, this function's behaves as if the property files
// has already successfully been done.
std::unique_ptr<ArcSessionManager> CreateTestArcSessionManager(
    std::unique_ptr<ArcSessionRunner> arc_session_runner);

// Does something similar to CreateTestArcSessionManager(), but for an existing
// object. This function is useful for ARC browser_tests where ArcSessionManager
// object is (re)created with ArcServiceLauncher::ResetForTesting().
void ExpandPropertyFilesForTesting(ArcSessionManager* arc_session_manager);

}  // namespace arc

#endif  //  CHROME_BROWSER_ASH_ARC_TEST_TEST_ARC_SESSION_MANAGER_H_
