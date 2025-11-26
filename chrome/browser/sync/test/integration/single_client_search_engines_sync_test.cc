// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/sync/test/integration/committed_all_nudged_changes_checker.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/search_engines_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/protocol/search_engine_specifics.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using search_engines_helper::HasSearchEngine;
using testing::IsNull;
using testing::NotNull;

namespace {

std::unique_ptr<syncer::LoopbackServerEntity> CreateFromTemplateURL(
    std::unique_ptr<TemplateURL> turl) {
  DCHECK(turl);
  syncer::SyncData sync_data =
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl->data());
  return syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
      /*non_unique_name=*/sync_data.GetTitle(),
      /*client_tag=*/turl->sync_guid(), sync_data.GetSpecifics(),
      /*creation_time=*/0,
      /*last_modified_time=*/0);
}

}  // namespace

class SingleClientSearchEnginesSyncTestBase : public SyncTest {
 public:
  explicit SingleClientSearchEnginesSyncTestBase(TestType test_type)
      : SyncTest(test_type) {}
  ~SingleClientSearchEnginesSyncTestBase() override = default;

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
};

class SingleClientSearchEnginesSyncTest
    : public SingleClientSearchEnginesSyncTestBase,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientSearchEnginesSyncTest()
      : SingleClientSearchEnginesSyncTestBase(SINGLE_CLIENT) {
    if (GetSetupSyncMode() == SyncTest::SetupSyncMode::kSyncTransportOnly) {
      scoped_feature_list_.InitAndEnableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    }
  }
  ~SingleClientSearchEnginesSyncTest() override = default;

  SetupSyncMode GetSetupSyncMode() const override { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SingleClientSearchEnginesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(search_engines_helper::ServiceMatchesVerifier(0));
  search_engines_helper::AddSearchEngine(/*profile_index=*/0,
                                         /*keyword=*/"test0");
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(search_engines_helper::ServiceMatchesVerifier(0));
}

IN_PROC_BROWSER_TEST_P(SingleClientSearchEnginesSyncTest,
                       DuplicateKeywordEnginesAllFromSync) {
  ASSERT_TRUE(SetupClients());
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

  ASSERT_TRUE(SetupSync());
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

IN_PROC_BROWSER_TEST_P(SingleClientSearchEnginesSyncTest,
                       ShouldNotUploadUntouchedAutogeneratedEngines) {
  ASSERT_TRUE(SetupSync());

  TemplateURLService* service =
      search_engines_helper::GetServiceForBrowserContext(0);

  // `safe_for_autoreplace` being true indicates that the keyword is
  // autogenerated. `TemplateURLData::ActiveStatus::kUnspecified` indicates that
  // the keyword is untouched by the user. The following tests the different
  // combinations of these two parameters.

  // Untouched but non-autogenerated keyword. Should be synced.
  // Note: Such keywords are not expected to exist, since manually created
  // keywords will either be active or inactive.
  service->Add(CreateTestTemplateURL(
      u"untouched_not_autogenerated", "http://url1.com", "guid1",
      base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));

  // Manually deactivated, non-autogenerated keyword. Should be synced.
  service->Add(CreateTestTemplateURL(
      u"deactivated_not_autogenerated", "http://url2.com", "guid2",
      base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kFalse));

  // Manually activated, non-autogenerated keyword. Should be synced.
  service->Add(CreateTestTemplateURL(
      u"activated_not_autogenerated", "http://url3.com", "guid3",
      base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kTrue));

  // Untouched autogenerated keyword. Should not be synced.
  service->Add(CreateTestTemplateURL(
      u"untouched_autogenerated", "http://url4.com", "guid4",
      base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));

  // Manually deactivated, autogenerated keyword. Should be synced.
  service->Add(CreateTestTemplateURL(
      u"deactivated_autogenerated", "http://url5.com", "guid5",
      base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kFalse));

  // Manually activated, autogenerated keyword. Should be synced.
  service->Add(CreateTestTemplateURL(
      u"activated_autogenerated", "http://url6.com", "guid6",
      base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kTrue));

  // Wait for all local changes to be committed.
  ASSERT_TRUE(CommittedAllNudgedChangesChecker(GetSyncService(0)).Wait());

  EXPECT_TRUE(search_engines_helper::HasSearchEngineInFakeServer(
      "untouched_not_autogenerated", GetFakeServer()));
  EXPECT_TRUE(search_engines_helper::HasSearchEngineInFakeServer(
      "deactivated_not_autogenerated", GetFakeServer()));
  EXPECT_TRUE(search_engines_helper::HasSearchEngineInFakeServer(
      "activated_not_autogenerated", GetFakeServer()));
  EXPECT_TRUE(search_engines_helper::HasSearchEngineInFakeServer(
      "deactivated_autogenerated", GetFakeServer()));
  EXPECT_TRUE(search_engines_helper::HasSearchEngineInFakeServer(
      "activated_autogenerated", GetFakeServer()));
  // Only the untouched autogenerated keyword is not synced.
  EXPECT_FALSE(search_engines_helper::HasSearchEngineInFakeServer(
      "untouched_autogenerated", GetFakeServer()));
}

IN_PROC_BROWSER_TEST_P(SingleClientSearchEnginesSyncTest,
                       ShouldNotSyncUntouchedAutogeneratedEnginesFromRemote) {
  ASSERT_TRUE(SetupClients());

  // `safe_for_autoreplace` being true indicates that the keyword is
  // autogenerated. `TemplateURLData::ActiveStatus::kUnspecified` indicates that
  // the keyword is untouched by the user. The following tests the different
  // combinations of these two parameters.

  // Untouched but non-autogenerated keyword. Should be synced.
  fake_server_->InjectEntity(CreateFromTemplateURL(CreateTestTemplateURL(
      u"untouched_not_autogenerated", "http://url1.com", "guid1",
      base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified)));

  // Deactivated but non-autogenerated keyword. Should be synced.
  fake_server_->InjectEntity(CreateFromTemplateURL(CreateTestTemplateURL(
      u"deactivated_not_autogenerated", "http://url2.com", "guid2",
      base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kFalse)));

  // Activated but non-autogenerated keyword. Should be synced.
  fake_server_->InjectEntity(CreateFromTemplateURL(CreateTestTemplateURL(
      u"activated_not_autogenerated", "http://url3.com", "guid3",
      base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kTrue)));

  // Untouched autogenerated keyword. Should not be synced.
  fake_server_->InjectEntity(CreateFromTemplateURL(CreateTestTemplateURL(
      u"untouched_autogenerated", "http://url4.com", "guid4",
      base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified)));

  // Deactivated autogenerated keyword. Should be synced.
  fake_server_->InjectEntity(CreateFromTemplateURL(CreateTestTemplateURL(
      u"deactivated_autogenerated", "http://url4.com", "guid5",
      base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kFalse)));

  // Activated autogenerated keyword. Should be synced.
  fake_server_->InjectEntity(CreateFromTemplateURL(CreateTestTemplateURL(
      u"activated_autogenerated", "http://url6.com", "guid6",
      base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kTrue)));

  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(
      HasSearchEngine(/*profile_index=*/0, "untouched_not_autogenerated"));
  EXPECT_TRUE(
      HasSearchEngine(/*profile_index=*/0, "deactivated_not_autogenerated"));
  EXPECT_TRUE(
      HasSearchEngine(/*profile_index=*/0, "activated_not_autogenerated"));
  EXPECT_TRUE(
      HasSearchEngine(/*profile_index=*/0, "deactivated_autogenerated"));
  EXPECT_TRUE(HasSearchEngine(/*profile_index=*/0, "activated_autogenerated"));
  // Only the autogenerated untouched keyword is not synced from remote.
  EXPECT_FALSE(HasSearchEngine(/*profile_index=*/0, "untouched_autogenerated"));
}

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientSearchEnginesSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

class
    SingleClientSearchEnginesSyncTestWithSeparateLocalAndAccountSearchEnginesEnabled
    : public SingleClientSearchEnginesSyncTestBase {
 public:
  SingleClientSearchEnginesSyncTestWithSeparateLocalAndAccountSearchEnginesEnabled()
      : SingleClientSearchEnginesSyncTestBase(SINGLE_CLIENT) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{syncer::kSeparateLocalAndAccountSearchEngines,
                              // This is needed to enable search engines in
                              // transport mode.
                              switches::kEnablePreferencesAccountStorage,
                              syncer::kReplaceSyncPromosWithSignInPromos},
        /*disabled_features=*/{});
  }

  // This test suite runs in transport-only mode because it deals with sepe.
  SetupSyncMode GetSetupSyncMode() const override {
    return SyncTest::SetupSyncMode::kSyncTransportOnly;
  }

 protected:
  auto CreateSyncEntity(const std::u16string& keyword,
                        const std::string& url,
                        const std::string& guid,
                        base::Time last_modified) {
    return CreateFromTemplateURL(CreateTestTemplateURL(
        /*keyword=*/keyword, /*url=*/url,
        /*guid=*/guid, last_modified,
        /*safe_for_autoreplace=*/false,
        /*created_by_policy=*/TemplateURLData::PolicyOrigin::kNoPolicy,
        /*prepopulate_id=*/100));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SingleClientSearchEnginesSyncTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldNotUploadLocalSearchEngines) {
  ASSERT_TRUE(SetupClients());
  search_engines_helper::AddSearchEngine(/*profile_index=*/0, "key1");

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  EXPECT_FALSE(search_engines_helper::HasSearchEngineInFakeServer(
      "key1", GetFakeServer()));
  EXPECT_TRUE(
      search_engines_helper::HasSearchEngine(/*profile_index=*/0, "key1"));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientSearchEnginesSyncTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldDownloadAccountSearchEngines) {
  ASSERT_TRUE(SetupClients());
  search_engines_helper::AddSearchEngine(/*profile_index=*/0, "key1");

  ASSERT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
  ASSERT_FALSE(HasSearchEngine(/*profile_index=*/0, "key2"));

  // Create a remote TemplateURL.
  fake_server_->InjectEntity(CreateSyncEntity(
      /*keyword=*/u"key2", /*url=*/"http://key2.com",
      /*guid=*/"guid2", base::Time::FromTimeT(10)));

  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
  EXPECT_TRUE(HasSearchEngine(/*profile_index=*/0, "key2"));
  EXPECT_FALSE(search_engines_helper::HasSearchEngineInFakeServer(
      "key1", GetFakeServer()));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientSearchEnginesSyncTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldReplaceConflictingLocalBuiltInSearchEngine) {
  ASSERT_TRUE(SetupClients());
  search_engines_helper::AddSearchEngine(/*profile_index=*/0, "key1");

  ASSERT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
  ASSERT_FALSE(HasSearchEngine(/*profile_index=*/0, "key2"));

  // Create a remote TemplateURL with the same prepopulate_id to conflict with
  // the local search engine.
  fake_server_->InjectEntity(CreateFromTemplateURL(CreateTestTemplateURL(
      /*keyword=*/u"key2", /*url=*/"http://key2.com",
      /*guid=*/"guid2")));

  ASSERT_TRUE(SetupSync());

  EXPECT_FALSE(HasSearchEngine(/*profile_index=*/0, "key1"));
  EXPECT_TRUE(HasSearchEngine(/*profile_index=*/0, "key2"));
  EXPECT_FALSE(search_engines_helper::HasSearchEngineInFakeServer(
      "key1", GetFakeServer()));

  // Disable sync.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPreferences));

  EXPECT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
  EXPECT_FALSE(HasSearchEngine(/*profile_index=*/0, "key2"));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientSearchEnginesSyncTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldRemoveAccountSearchEnginesUponSignout) {
  ASSERT_TRUE(SetupClients());
  search_engines_helper::AddSearchEngine(/*profile_index=*/0, "key1");
  // Create a remote TemplateURL with a different prepopulate_id to avoid
  // conflict.
  fake_server_->InjectEntity(CreateSyncEntity(
      /*keyword=*/u"key2", /*url=*/"http://key2.com",
      /*guid=*/"guid2", base::Time::FromTimeT(10)));

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
  ASSERT_TRUE(HasSearchEngine(/*profile_index=*/0, "key2"));

  // Disable sync.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPreferences));

  EXPECT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
  EXPECT_FALSE(search_engines_helper::HasSearchEngineInFakeServer(
      "key1", GetFakeServer()));
  EXPECT_FALSE(HasSearchEngine(/*profile_index=*/0, "key2"));
  EXPECT_TRUE(search_engines_helper::HasSearchEngineInFakeServer(
      "key2", GetFakeServer()));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientSearchEnginesSyncTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldAddToLocalAndAccountSearchEngines) {
  ASSERT_TRUE(SetupSync());

  search_engines_helper::AddSearchEngine(/*profile_index=*/0, "key1");

  EXPECT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
  EXPECT_TRUE(
      search_engines_helper::FakeServerHasSearchEngineChecker("key1").Wait());

  // Disable sync.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPreferences));

  EXPECT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
  EXPECT_TRUE(search_engines_helper::HasSearchEngineInFakeServer(
      "key1", GetFakeServer()));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientSearchEnginesSyncTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldDualWriteUponEdit) {
  ASSERT_TRUE(SetupClients());
  search_engines_helper::AddSearchEngine(/*profile_index=*/0, "key1");

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
  ASSERT_FALSE(search_engines_helper::HasSearchEngineInFakeServer(
      "key1", GetFakeServer()));

  // Update the short name and the url.
  search_engines_helper::EditSearchEngine(
      /*profile_index=*/0, "key1", u"short_name", "key1", "http://key1.com");
  EXPECT_TRUE(
      search_engines_helper::FakeServerHasSearchEngineChecker("key1").Wait());

  // Disable sync.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPreferences));

  EXPECT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
  EXPECT_TRUE(search_engines_helper::HasSearchEngineInFakeServer(
      "key1", GetFakeServer()));

  // Ensure that the edits are applied to the local value.
  const TemplateURL* local =
      search_engines_helper::GetServiceForBrowserContext(0)
          ->GetTemplateURLForKeyword(u"key1");
  ASSERT_TRUE(local);
  EXPECT_EQ(u"short_name", local->short_name());
  EXPECT_EQ("http://key1.com", local->url());

  // Ensure that the edits are applied to the account value.
  const std::optional<sync_pb::SearchEngineSpecifics>& account =
      search_engines_helper::GetSearchEngineInFakeServerWithKeyword(
          "key1", GetFakeServer());
  ASSERT_TRUE(account);
  EXPECT_EQ("short_name", account->short_name());
  EXPECT_EQ("http://key1.com", account->url());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientSearchEnginesSyncTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    PRE_ShouldPreserveLocalAndAccountSearchEnginesAcrossRestart) {
  ASSERT_TRUE(SetupClients());
  search_engines_helper::AddSearchEngine(/*profile_index=*/0, "key1");
  fake_server_->InjectEntity(CreateSyncEntity(
      /*keyword=*/u"key2", /*url=*/"http://key2.com",
      /*guid=*/"guid2", base::Time::FromTimeT(10)));

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
  ASSERT_TRUE(HasSearchEngine(/*profile_index=*/0, "key2"));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientSearchEnginesSyncTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldPreserveLocalAndAccountSearchEnginesAcrossRestart) {
  ASSERT_TRUE(SetupClients());

  EXPECT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
  EXPECT_TRUE(HasSearchEngine(/*profile_index=*/0, "key2"));

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  EXPECT_FALSE(search_engines_helper::HasSearchEngineInFakeServer(
      "key1", GetFakeServer()));
  EXPECT_TRUE(search_engines_helper::HasSearchEngineInFakeServer(
      "key2", GetFakeServer()));

  // Disable sync.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPreferences));

  EXPECT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
  EXPECT_FALSE(search_engines_helper::HasSearchEngineInFakeServer(
      "key1", GetFakeServer()));
  EXPECT_FALSE(HasSearchEngine(/*profile_index=*/0, "key2"));
  EXPECT_TRUE(search_engines_helper::HasSearchEngineInFakeServer(
      "key2", GetFakeServer()));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientSearchEnginesSyncTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    PRE_ShouldNotMarkLocalSearchEngineAsAccount) {
  ASSERT_TRUE(SetupClients());
  search_engines_helper::AddSearchEngine(/*profile_index=*/0, "key1");

  ASSERT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientSearchEnginesSyncTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldNotMarkLocalSearchEngineAsAccount) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
  EXPECT_FALSE(search_engines_helper::HasSearchEngineInFakeServer(
      "key1", GetFakeServer()));

  // Disable sync.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPreferences));

  EXPECT_TRUE(HasSearchEngine(/*profile_index=*/0, "key1"));
  EXPECT_FALSE(search_engines_helper::HasSearchEngineInFakeServer(
      "key1", GetFakeServer()));
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(
    SingleClientSearchEnginesSyncTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    PRE_ShouldClearAccountDataOnStartupIfSignInAllowedBitChanged) {
  ASSERT_TRUE(SetupClients());

  // Set the sign-in allowed bit to true initially.
  preferences_helper::GetPrefs(/*index=*/0)
      ->SetBoolean(prefs::kSigninAllowedOnNextStartup, true);

  TemplateURLService* service =
      search_engines_helper::GetServiceForBrowserContext(0);
  service->Add(CreateTestTemplateURL(u"localkeyword", "http://local.com",
                                     "guid", base::Time::FromTimeT(100)));

  GetFakeServer()->InjectEntity(CreateFromTemplateURL(
      CreateTestTemplateURL(u"accountkeyword", "http://account.com", "guid",
                            base::Time::FromTimeT(100))));

  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // Account value is effective.
  ASSERT_THAT(service->GetTemplateURLForGUID("guid"),
              testing::Pointee(
                  testing::Property(&TemplateURL::keyword, u"accountkeyword")));

  // Simulate turning off the sign-in allowed bit on the settings page.
  preferences_helper::GetPrefs(/*index=*/0)
      ->SetBoolean(prefs::kSigninAllowedOnNextStartup, false);
}

IN_PROC_BROWSER_TEST_F(
    SingleClientSearchEnginesSyncTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldClearAccountDataOnStartupIfSignInAllowedBitChanged) {
  ASSERT_TRUE(SetupClients());

  // Original local value should be active and the account value should not have
  // been applied.
  EXPECT_THAT(search_engines_helper::GetServiceForBrowserContext(0)
                  ->GetTemplateURLForGUID("guid"),
              testing::Pointee(
                  testing::Property(&TemplateURL::keyword, u"localkeyword")));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(
    SingleClientSearchEnginesSyncTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    PRE_ShouldClearAccountDataOnStartupIfAccountStateChanged) {
  ASSERT_TRUE(SetupClients());

  TemplateURLService* service =
      search_engines_helper::GetServiceForBrowserContext(0);
  service->Add(CreateTestTemplateURL(u"localkeyword", "http://local.com",
                                     "guid", base::Time::FromTimeT(100)));

  GetFakeServer()->InjectEntity(CreateFromTemplateURL(
      CreateTestTemplateURL(u"accountkeyword", "http://account.com", "guid",
                            base::Time::FromTimeT(100))));

#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_TRUE(SetupSync());
#else
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Account value is effective.
  ASSERT_THAT(service->GetTemplateURLForGUID("guid"),
              testing::Pointee(
                  testing::Property(&TemplateURL::keyword, u"accountkeyword")));

  // Simulate a data type error to prevent clearing of account data.
  GetSyncService(0)->ReportDataTypeErrorForTest(syncer::SEARCH_ENGINES);
#if BUILDFLAG(IS_CHROMEOS)
  // Disable sync.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPreferences));
#else
  // Sign out.
  GetClient(0)->SignOutPrimaryAccount();
#endif  // BUILDFLAG(IS_CHROMEOS)
  ASSERT_TRUE(HasSearchEngine(/*profile_index=*/0, "accountkeyword"));

  ExcludeDataTypesFromCheckForDataTypeFailures({syncer::SEARCH_ENGINES});
}

IN_PROC_BROWSER_TEST_F(
    SingleClientSearchEnginesSyncTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldClearAccountDataOnStartupIfAccountStateChanged) {
  ASSERT_TRUE(SetupClients());

  // Original local value should be active and the account value should not have
  // been applied.
  EXPECT_THAT(search_engines_helper::GetServiceForBrowserContext(0)
                  ->GetTemplateURLForGUID("guid"),
              testing::Pointee(
                  testing::Property(&TemplateURL::keyword, u"localkeyword")));
}
