// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/search_engines_helper.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync/driver/sync_driver_switches.h"

using base::ASCIIToUTF16;

class TwoClientSearchEnginesSyncTest : public FeatureToggler, public SyncTest {
 public:
  TwoClientSearchEnginesSyncTest()
      : FeatureToggler(switches::kSyncPseudoUSSSearchEngines),
        SyncTest(TWO_CLIENT) {}
  ~TwoClientSearchEnginesSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientSearchEnginesSyncTest);
};

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest, E2E_ENABLED(Add)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  // Note that a random seed is needed due to the E2E nature of the tests, and
  // the synced data persisting in the server across tests.
  int search_engine_seed = base::Time::Now().ToInternalValue();
  search_engines_helper::AddSearchEngine(0, search_engine_seed);
  ASSERT_TRUE(search_engines_helper::HasSearchEngine(0, search_engine_seed));

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
  ASSERT_TRUE(search_engines_helper::HasSearchEngine(1, search_engine_seed));
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest, E2E_ENABLED(Delete)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  // Note that a random seed is needed due to the E2E nature of the tests, and
  // the synced data persisting in the server across tests.
  int search_engine_seed = base::Time::Now().ToInternalValue();
  search_engines_helper::AddSearchEngine(0, search_engine_seed);
  ASSERT_TRUE(search_engines_helper::HasSearchEngine(0, search_engine_seed));

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
  ASSERT_TRUE(search_engines_helper::HasSearchEngine(1, search_engine_seed));

  search_engines_helper::DeleteSearchEngineBySeed(0, search_engine_seed);

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
  ASSERT_FALSE(search_engines_helper::HasSearchEngine(1, search_engine_seed));
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest,
                       E2E_ENABLED(AddMultiple)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  // Add a few entries.
  for (int i = 0; i < 3; ++i)
    search_engines_helper::AddSearchEngine(0, i);

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest, Duplicates) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  // Add two entries with the same Name and URL (but different keywords).
  // Note that we have to change the GUID of the duplicate.
  search_engines_helper::AddSearchEngine(0, 0);
  Profile* profile = sync_datatype_helper::test()->GetProfile(0);
  TemplateURLServiceFactory::GetForProfile(profile)->Add(
      search_engines_helper::CreateTestTemplateURL(profile, 0,
          ASCIIToUTF16("somethingelse"), "newguid"));
  search_engines_helper::GetVerifierService()->Add(
      search_engines_helper::CreateTestTemplateURL(profile, 0,
          ASCIIToUTF16("somethingelse"), "newguid"));
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest,
                       E2E_ENABLED(UpdateKeyword)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  search_engines_helper::AddSearchEngine(0, 0);

  // Change the keyword.
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  search_engines_helper::EditSearchEngine(0, ASCIIToUTF16("test0"),
      ASCIIToUTF16("test0"), ASCIIToUTF16("newkeyword"),
      "http://www.test0.com/");

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest, E2E_ENABLED(UpdateUrl)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  search_engines_helper::AddSearchEngine(0, 0);

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  // Change the URL.
  search_engines_helper::EditSearchEngine(0, ASCIIToUTF16("test0"),
      ASCIIToUTF16("test0"), ASCIIToUTF16("test0"),
      "http://www.wikipedia.org/q=%s");

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest,
                       E2E_ENABLED(UpdateName)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  search_engines_helper::AddSearchEngine(0, 0);

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  // Change the short name.
  search_engines_helper::EditSearchEngine(0, ASCIIToUTF16("test0"),
      ASCIIToUTF16("New Name"), ASCIIToUTF16("test0"), "http://www.test0.com/");

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest, ConflictKeyword) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  // Add a different search engine to each client, but make their keywords
  // conflict.
  search_engines_helper::AddSearchEngine(0, 0);
  search_engines_helper::AddSearchEngine(1, 1);
  TemplateURLService* service =
      search_engines_helper::GetServiceForBrowserContext(1);
  TemplateURL* turl = service->GetTemplateURLForKeyword(ASCIIToUTF16("test1"));
  EXPECT_TRUE(turl);
  service->ResetTemplateURL(turl, turl->short_name(), ASCIIToUTF16("test0"),
                            turl->url());

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest, MergeMultiple) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  // Set up some different search engines on each client, with some interesting
  // conflicts.
  // client0: { SE0, SE1, SE2 }
  for (int i = 0; i < 3; ++i)
    search_engines_helper::AddSearchEngine(0, i);

  // client1: { SE0, SE2, SE3, SE0 + different URL }
  search_engines_helper::AddSearchEngine(1, 0);
  search_engines_helper::AddSearchEngine(1, 2);
  search_engines_helper::AddSearchEngine(1, 3);
  Profile* profile = sync_datatype_helper::test()->GetProfile(1);
  TemplateURLServiceFactory::GetForProfile(profile)->Add(
      search_engines_helper::CreateTestTemplateURL(profile, 0,
          ASCIIToUTF16("somethingelse.com"), "http://www.somethingelse.com/",
          "somethingelse"));

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  search_engines_helper::AddSearchEngine(0, 0);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(search_engines_helper::ServiceMatchesVerifier(0));
  ASSERT_FALSE(search_engines_helper::ServiceMatchesVerifier(1));

  ASSERT_TRUE(GetClient(1)->EnableSyncForAllDatatypes());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest,
                       E2E_ENABLED(SyncDefault)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  search_engines_helper::AddSearchEngine(0, 0);
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  // Change the default to the new search engine, sync, and ensure that it
  // changed in the second client. AllServicesMatch does a default search
  // provider check.
  search_engines_helper::ChangeDefaultSearchProvider(0, 0);
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
}

// Ensure that we can change the search engine and immediately delete it
// without putting the clients out of sync.
IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest,
                       E2E_ENABLED(DeleteSyncedDefault)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::AllServicesMatch());

  search_engines_helper::AddSearchEngine(0, 0);
  search_engines_helper::AddSearchEngine(0, 1);
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  search_engines_helper::ChangeDefaultSearchProvider(0, 0);
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  // Change the default on the first client and delete the old default.
  search_engines_helper::ChangeDefaultSearchProvider(0, 1);
  search_engines_helper::DeleteSearchEngineBySeed(0, 0);
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
}

INSTANTIATE_TEST_CASE_P(USS,
                        TwoClientSearchEnginesSyncTest,
                        ::testing::Values(false, true));
