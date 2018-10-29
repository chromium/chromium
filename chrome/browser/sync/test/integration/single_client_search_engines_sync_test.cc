// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/search_engines_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync/driver/sync_driver_switches.h"

class SingleClientSearchEnginesSyncTest : public FeatureToggler,
                                          public SyncTest {
 public:
  SingleClientSearchEnginesSyncTest()
      : FeatureToggler(switches::kSyncPseudoUSSSearchEngines),
        SyncTest(SINGLE_CLIENT) {}
  ~SingleClientSearchEnginesSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientSearchEnginesSyncTest);
};

IN_PROC_BROWSER_TEST_P(SingleClientSearchEnginesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::ServiceMatchesVerifier(0));
  search_engines_helper::AddSearchEngine(0, 0);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(search_engines_helper::ServiceMatchesVerifier(0));
}

INSTANTIATE_TEST_CASE_P(USS,
                        SingleClientSearchEnginesSyncTest,
                        ::testing::Values(false, true));
