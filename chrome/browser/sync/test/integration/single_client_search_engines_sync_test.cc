// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/test/integration/search_engines_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync/driver/profile_sync_service.h"
#include "content/public/test/browser_test.h"

class SingleClientSearchEnginesSyncTest : public SyncTest {
 public:
  SingleClientSearchEnginesSyncTest() : SyncTest(SINGLE_CLIENT) {}

  ~SingleClientSearchEnginesSyncTest() override {}

  bool SetupClients() override {
    if (!SyncTest::SetupClients()) {
      return false;
    }

    // Wait for models to load.
    search_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(verifier()));
    search_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(GetProfile(0)));

    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientSearchEnginesSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientSearchEnginesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::ServiceMatchesVerifier(0));
  search_engines_helper::AddSearchEngine(0, 0);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(search_engines_helper::ServiceMatchesVerifier(0));
}
