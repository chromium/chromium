// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/test/integration/search_engines_helper.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"

namespace {

using search_engines_helper::AddSearchEngine;
using search_engines_helper::AllServicesMatch;
using search_engines_helper::ChangeDefaultSearchProvider;
using search_engines_helper::DeleteSearchEngine;
using search_engines_helper::EditSearchEngine;
using search_engines_helper::GetDefaultSearchEngineKeyword;
using search_engines_helper::GetServiceForBrowserContext;
using search_engines_helper::GetVerifierService;
using search_engines_helper::HasSearchEngine;
using search_engines_helper::HasSearchEngineChecker;
using search_engines_helper::SearchEnginesMatchChecker;
using search_engines_helper::ServiceMatchesVerifier;
using search_engines_helper::TemplateURLBuilder;

class TwoClientSearchEnginesSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  TwoClientSearchEnginesSyncTest() : SyncTest(TWO_CLIENT) {
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos,
                                syncer::kSeparateLocalAndAccountSearchEngines},
          /*disabled_features=*/{});
    }
  }
  ~TwoClientSearchEnginesSyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  bool SetupClients() override {
    if (!SyncTest::SetupClients()) {
      return false;
    }

    // In most cases this codepath should have exactly two clients, but there is
    // an exception for E2E tests, where ResetSyncForPrimaryAccount()
    // temporarily sets up one only.
    for (int i = 0; i < num_clients(); ++i) {
      search_test_utils::WaitForTemplateURLServiceToLoad(
          TemplateURLServiceFactory::GetForProfile(GetProfile(i)));
    }

    return true;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         TwoClientSearchEnginesSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

class TwoClientSearchEnginesSyncTestWithVerifier
    : public TwoClientSearchEnginesSyncTest {
 public:
  TwoClientSearchEnginesSyncTestWithVerifier() = default;
  ~TwoClientSearchEnginesSyncTestWithVerifier() override = default;

  bool UseVerifier() override {
    // TODO(crbug.com/40724973): rewrite test to not use verifier.
    return true;
  }

  bool SetupClients() override {
    if (!TwoClientSearchEnginesSyncTest::SetupClients()) {
      return false;
    }
    search_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(verifier()));
    return true;
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         TwoClientSearchEnginesSyncTestWithVerifier,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest, E2E_ENABLED(Add)) {
  ASSERT_TRUE(ResetSyncForPrimaryAccount());
  ASSERT_TRUE(SetupSync());
  // TODO(crbug.com/41453418): Ideally we could immediately assert
  // AllServicesMatch(), but that's not possible today without introducing
  // flakiness due to random GUIDs in prepopulated engines.
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  const std::string kKeyword = "test0";
  AddSearchEngine(/*profile_index=*/0, kKeyword);
  ASSERT_TRUE(HasSearchEngine(/*profile_index=*/0, kKeyword));

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
  ASSERT_TRUE(HasSearchEngine(/*profile_index=*/1, kKeyword));
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest, E2E_ENABLED(Delete)) {
  ASSERT_TRUE(ResetSyncForPrimaryAccount());
  ASSERT_TRUE(SetupSync());
  // TODO(crbug.com/41453418): Ideally we could immediately assert
  // AllServicesMatch(), but that's not possible today without introducing
  // flakiness due to random GUIDs in prepopulated engines.
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  const std::string kKeyword = "test0";
  AddSearchEngine(/*profile_index=*/0, kKeyword);
  ASSERT_TRUE(HasSearchEngine(/*profile_index=*/0, kKeyword));

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
  ASSERT_TRUE(HasSearchEngine(/*profile_index=*/1, kKeyword));

  DeleteSearchEngine(/*profile_index=*/0, kKeyword);

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
  ASSERT_FALSE(HasSearchEngine(/*profile_index=*/1, kKeyword));
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest,
                       E2E_ENABLED(AddMultiple)) {
  ASSERT_TRUE(ResetSyncForPrimaryAccount());
  ASSERT_TRUE(SetupSync());
  // TODO(crbug.com/41453418): Ideally we could immediately assert
  // AllServicesMatch(), but that's not possible today without introducing
  // flakiness due to random GUIDs in prepopulated engines.
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  // Add a few entries.
  AddSearchEngine(/*profile_index=*/0, "test0");
  AddSearchEngine(/*profile_index=*/0, "test1");
  AddSearchEngine(/*profile_index=*/0, "test2");

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTestWithVerifier, Duplicates) {
  ASSERT_TRUE(SetupSync());
  // TODO(crbug.com/41453418): Ideally we could immediately assert
  // AllServicesMatch(), but that's not possible today without introducing
  // flakiness due to random GUIDs in prepopulated engines.
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  // Add two entries with the same Name and URL (but different keywords). Note
  // that we have to change the GUID of the duplicate.
  TemplateURLBuilder builder("test0");
  GetServiceForBrowserContext(0)->Add(builder.Build());
  GetVerifierService()->Add(builder.Build());

  builder.data()->SetKeyword(u"test1");
  builder.data()->sync_guid = "newguid";
  GetServiceForBrowserContext(0)->Add(builder.Build());
  GetVerifierService()->Add(builder.Build());

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest,
                       E2E_ENABLED(UpdateKeyword)) {
  ASSERT_TRUE(ResetSyncForPrimaryAccount());
  ASSERT_TRUE(SetupSync());
  // TODO(crbug.com/41453418): Ideally we could immediately assert
  // AllServicesMatch(), but that's not possible today without introducing
  // flakiness due to random GUIDs in prepopulated engines.
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  AddSearchEngine(/*profile_index=*/0, "test0");

  // Change the keyword.
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  EditSearchEngine(/*profile_index=*/0, /*keyword=*/"test0", u"test0",
                   /*new_keyword=*/"newkeyword", "http://www.test0.com/");

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest, E2E_ENABLED(UpdateUrl)) {
  ASSERT_TRUE(ResetSyncForPrimaryAccount());
  ASSERT_TRUE(SetupSync());
  // TODO(crbug.com/41453418): Ideally we could immediately assert
  // AllServicesMatch(), but that's not possible today without introducing
  // flakiness due to random GUIDs in prepopulated engines.
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  AddSearchEngine(/*profile_index=*/0, "test0");

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  // Change the URL.
  EditSearchEngine(/*profile_index=*/0, /*keyword=*/"test0", u"test0",
                   /*new_keyword=*/"test0", "http://www.wikipedia.org/q=%s");

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest,
                       E2E_ENABLED(UpdateName)) {
  ASSERT_TRUE(ResetSyncForPrimaryAccount());
  ASSERT_TRUE(SetupSync());
  // TODO(crbug.com/41453418): Ideally we could immediately assert
  // AllServicesMatch(), but that's not possible today without introducing
  // flakiness due to random GUIDs in prepopulated engines.
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  AddSearchEngine(/*profile_index=*/0, "test0");

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  // Change the short name.
  EditSearchEngine(/*profile_index=*/0, "test0", u"New Name", "test0",
                   "http://www.test0.com/");

  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest, ConflictKeyword) {
  ASSERT_TRUE(SetupSync());
  // TODO(crbug.com/41453418): Ideally we could immediately assert
  // AllServicesMatch(), but that's not possible today without introducing
  // flakiness due to random GUIDs in prepopulated engines.
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  // Add a different search engine to each client, but make their keywords
  // conflict.
  AddSearchEngine(/*profile_index=*/0, "test0");
  AddSearchEngine(/*profile_index=*/1, "test1");
  TemplateURLService* service = GetServiceForBrowserContext(1);
  TemplateURL* turl = service->GetTemplateURLForKeyword(u"test1");
  EXPECT_TRUE(turl);
  service->ResetTemplateURL(turl, turl->short_name(), u"test0", turl->url());

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllServicesMatch());
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTest, MergeMultiple) {
  ASSERT_TRUE(SetupSync());
  // TODO(crbug.com/41453418): Ideally we could immediately assert
  // AllServicesMatch(), but that's not possible today without introducing
  // flakiness due to random GUIDs in prepopulated engines.
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  // Set up some different search engines on each client, with some interesting
  // conflicts. client0: { SE0, SE1, SE2 }
  AddSearchEngine(/*profile_index=*/0, "test0");
  AddSearchEngine(/*profile_index=*/0, "test1");
  AddSearchEngine(/*profile_index=*/0, "test2");

  // client1: { SE0, SE2, SE3, SE0 + different URL }
  AddSearchEngine(/*profile_index=*/1, "test0");
  AddSearchEngine(/*profile_index=*/1, "test2");
  AddSearchEngine(/*profile_index=*/1, "test3");

  TemplateURLBuilder builder("test0");
  builder.data()->SetKeyword(u"somethingelse.com");
  builder.data()->SetURL("http://www.somethingelse.com/");
  builder.data()->sync_guid = "somethingelse";
  GetServiceForBrowserContext(1)->Add(builder.Build());

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllServicesMatch());
}

IN_PROC_BROWSER_TEST_P(TwoClientSearchEnginesSyncTestWithVerifier,
                       DisableSync) {
  ASSERT_TRUE(SetupSync());
  // TODO(crbug.com/41453418): Ideally we could immediately assert
  // AllServicesMatch(), but that's not possible today without introducing
  // flakiness due to random GUIDs in prepopulated engines.
  ASSERT_TRUE(SearchEnginesMatchChecker().Wait());

  ASSERT_TRUE(GetClient(1)->DisableAllSelectableTypes());
  AddSearchEngine(/*profile_index=*/0, "test0");
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(ServiceMatchesVerifier(0));
  ASSERT_FALSE(ServiceMatchesVerifier(1));

  ASSERT_TRUE(GetClient(1)->EnableAllSelectableTypes());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllServicesMatch());
}

}  // namespace
