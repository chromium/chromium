// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/sync/test/integration/search_engines_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using search_engines_helper::HasSearchEngine;
using testing::NotNull;

class SingleClientSearchEnginesSyncTest : public SyncTest {
 public:
  SingleClientSearchEnginesSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientSearchEnginesSyncTest() override = default;

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

  bool UseVerifier() override {
    // TODO(crbug.com/40724973): rewrite test to not use verifier.
    return true;
  }

  std::unique_ptr<syncer::LoopbackServerEntity> CreateFromTemplateURL(
      std::unique_ptr<TemplateURL> turl) {
    DCHECK(turl);
    syncer::SyncData sync_data =
        TemplateURLService::CreateSyncDataFromTemplateURL(*turl);
    return syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
        /*non_unique_name=*/sync_data.GetTitle(),
        /*client_tag=*/turl->sync_guid(), sync_data.GetSpecifics(),
        /*creation_time=*/0,
        /*last_modified_time=*/0);
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientSearchEnginesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(search_engines_helper::ServiceMatchesVerifier(0));
  search_engines_helper::AddSearchEngine(/*profile_index=*/0,
                                         /*keyword=*/"test0");
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(search_engines_helper::ServiceMatchesVerifier(0));
}

IN_PROC_BROWSER_TEST_F(SingleClientSearchEnginesSyncTest,
                       DuplicateKeywordEnginesAllFromSync) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  TemplateURLService* service =
      search_engines_helper::GetServiceForBrowserContext(0);
  ASSERT_FALSE(HasSearchEngine(/*profile_index=*/0, "key1"));
  ASSERT_FALSE(service->GetTemplateURLForGUID("guid1"));
  ASSERT_FALSE(service->GetTemplateURLForGUID("guid2"));
  ASSERT_FALSE(service->GetTemplateURLForGUID("guid3"));

  // Create two TemplateURLs with the same keyword, but different guids.
  // "guid2" is newer, so it should be treated as better.
  fake_server_->InjectEntity(CreateFromTemplateURL(CreateTestTemplateURL(
      /*keyword=*/u"key1", /*url=*/"http://key1.com",
      /*guid=*/"guid1", base::Time::FromTimeT(10))));
  fake_server_->InjectEntity(CreateFromTemplateURL(CreateTestTemplateURL(
      /*keyword=*/u"key1", /*url=*/"http://key1.com",
      /*guid=*/"guid2", base::Time::FromTimeT(5))));
  fake_server_->InjectEntity(CreateFromTemplateURL(CreateTestTemplateURL(
      /*keyword=*/u"key1", /*url=*/"http://key1.com",
      /*guid=*/"guid3", base::Time::FromTimeT(5))));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  EXPECT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
  TemplateURL* guid1 = service->GetTemplateURLForGUID("guid1");
  TemplateURL* guid2 = service->GetTemplateURLForGUID("guid2");
  TemplateURL* guid3 = service->GetTemplateURLForGUID("guid3");
  ASSERT_THAT(guid1, NotNull());
  ASSERT_THAT(guid2, NotNull());
  ASSERT_THAT(guid3, NotNull());
  // All three should retain their "key1" keywords, even if they are duplicates.
  EXPECT_EQ(u"key1", guid1->keyword());
  EXPECT_EQ(u"key1", guid2->keyword());
  EXPECT_EQ(u"key1", guid3->keyword());

  // But "guid1" should be considered the "best", as it's the most recent.
  EXPECT_EQ(guid1, service->GetTemplateURLForKeyword(u"key1"));
}
