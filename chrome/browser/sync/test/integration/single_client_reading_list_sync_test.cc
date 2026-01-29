// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/reading_list_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/reading_list/core/mock_reading_list_model_observer.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/reading_list_specifics.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/test_matchers.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"

namespace {

using reading_list_helper::CreateTestReadingListEntity;
using reading_list_helper::GetReadingListURLsFromFakeServer;
using reading_list_helper::LocalReadingListURLsEqualityChecker;
using reading_list_helper::ServerReadingListTitlesEqualityChecker;
using reading_list_helper::ServerReadingListURLsEqualityChecker;
using reading_list_helper::WaitForReadingListModelLoaded;
using syncer::IsEmptyLocalDataDescription;
using syncer::MatchesDeletionOrigin;
using syncer::MatchesLocalDataDescription;
using syncer::MatchesLocalDataItemModel;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;

class SingleClientReadingListSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientReadingListSyncTest() : SyncTest(SINGLE_CLIENT) {
    std::vector<base::test::FeatureRef> enabled_features;
#if !BUILDFLAG(IS_ANDROID)
    enabled_features.push_back(
        syncer::kReadingListEnableSyncTransportModeUponSignIn);
#endif  // !BUILDFLAG(IS_ANDROID)
    if (GetParam() == SyncTest::SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features,
                                          /*disabled_features=*/{});
  }

  SingleClientReadingListSyncTest(const SingleClientReadingListSyncTest&) =
      delete;
  SingleClientReadingListSyncTest& operator=(
      const SingleClientReadingListSyncTest&) = delete;

  ~SingleClientReadingListSyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  bool SetupClients() override {
    if (!SyncTest::SetupClients()) {
      return false;
    }

    WaitForReadingListModelLoaded(model());
    return true;
  }

  raw_ptr<ReadingListModel> model() {
    return ReadingListModelFactory::GetForBrowserContext(GetProfile(0));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientReadingListSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SingleClientReadingListSyncTest,
                       ShouldDownloadAccountDataUponSignin) {
  const GURL kUrl("http://url.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "entry_title"));

  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));

  EXPECT_TRUE(LocalReadingListURLsEqualityChecker(model(), {kUrl}).Wait());
  EXPECT_THAT(model()->size(), Eq(1ul));
  EXPECT_FALSE(model()->NeedsExplicitUploadToSyncServer(kUrl));
}

IN_PROC_BROWSER_TEST_P(SingleClientReadingListSyncTest,
                       ShouldUploadOnlyEntriesCreatedAfterSignin) {
  if (GetParam() == SyncTest::SetupSyncMode::kSyncTheFeature) {
    GTEST_SKIP()
        << "This test verifies transport-only behavior where pre-existing "
           "data is not uploaded.";
  }
  ASSERT_TRUE(SetupClients());
  ASSERT_THAT(model()->size(), Eq(0ul));

  const GURL kLocalUrl("http://local_url.com/");
  model()->AddOrReplaceEntry(kLocalUrl, "local_title",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/std::nullopt,
                             /*creation_time=*/std::nullopt);

  ASSERT_THAT(model()->size(), Eq(1ul));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));
  ASSERT_THAT(model()->size(), Eq(1ul));

  const GURL kAccountUrl("http://account_url.com/");
  model()->AddOrReplaceEntry(kAccountUrl, "account_title",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/std::nullopt,
                             /*creation_time=*/std::nullopt);

  ASSERT_THAT(model()->size(), Eq(2ul));

  EXPECT_TRUE(model()->NeedsExplicitUploadToSyncServer(kLocalUrl));
  EXPECT_FALSE(model()->NeedsExplicitUploadToSyncServer(kAccountUrl));
  EXPECT_TRUE(ServerReadingListURLsEqualityChecker({{kAccountUrl}}).Wait());
  EXPECT_TRUE(model()->NeedsExplicitUploadToSyncServer(kLocalUrl));
  EXPECT_FALSE(model()->NeedsExplicitUploadToSyncServer(kAccountUrl));
}

IN_PROC_BROWSER_TEST_P(SingleClientReadingListSyncTest,
                       ShouldDeleteTheDeletedEntryFromTheServer) {
  const GURL kUrl("http://url.com/");
  const base::Location kLocation = FROM_HERE;

  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "entry_title"));

  ASSERT_TRUE(SetupClients());
  ASSERT_THAT(model()->size(), Eq(0ul));
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));
  ASSERT_THAT(model()->size(), Eq(1ul));

  model()->RemoveEntryByURL(kUrl, kLocation);
  ASSERT_THAT(model()->size(), Eq(0ul));
  EXPECT_TRUE(ServerReadingListURLsEqualityChecker({}).Wait());

  EXPECT_THAT(GetFakeServer()->GetCommittedDeletionOrigins(
                  syncer::DataType::READING_LIST),
              ElementsAre(MatchesDeletionOrigin(
                  version_info::GetVersionNumber(), kLocation)));
}

IN_PROC_BROWSER_TEST_P(SingleClientReadingListSyncTest,
                       ShouldDeleteAllEntriesFromTheServer) {
  const base::Location kLocation = FROM_HERE;

  fake_server_->InjectEntity(
      CreateTestReadingListEntity(GURL("http://url1.com/"), "entry_title1"));
  fake_server_->InjectEntity(
      CreateTestReadingListEntity(GURL("http://url2.com/"), "entry_title2"));

  ASSERT_TRUE(SetupClients());
  ASSERT_THAT(model()->size(), Eq(0ul));
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));
  ASSERT_THAT(model()->size(), Eq(2ul));

  model()->DeleteAllEntries(kLocation);
  ASSERT_THAT(model()->size(), Eq(0ul));
  EXPECT_TRUE(ServerReadingListURLsEqualityChecker({}).Wait());

  EXPECT_THAT(
      GetFakeServer()->GetCommittedDeletionOrigins(
          syncer::DataType::READING_LIST),
      ElementsAre(
          MatchesDeletionOrigin(version_info::GetVersionNumber(), kLocation),
          MatchesDeletionOrigin(version_info::GetVersionNumber(), kLocation)));
}

// ChromeOS doesn't have the concept of sign-out, so this only exists on other
// platforms.
#if !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(SingleClientReadingListSyncTest,
                       ShouldDeleteAccountDataUponSignout) {
  if (GetParam() == SyncTest::SetupSyncMode::kSyncTheFeature) {
    GTEST_SKIP()
        << "This test verifies transport-only behavior after sign-out.";
  }
  const GURL kUrl("http://url.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "entry_title"));

  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));

  ASSERT_THAT(model()->size(), Eq(1ul));

  GetClient(0)->SignOutPrimaryAccount();
  EXPECT_THAT(model()->size(), Eq(0ul));
}

IN_PROC_BROWSER_TEST_P(SingleClientReadingListSyncTest,
                       ShouldUpdateEntriesLocallyAndServerSide) {
  if (GetParam() == SyncTest::SetupSyncMode::kSyncTheFeature) {
    GTEST_SKIP()
        << "This test verifies transport-only behavior after sign-out.";
  }
  const GURL kAccountUrl("http://account_url.com/");
  fake_server_->InjectEntity(
      CreateTestReadingListEntity(kAccountUrl, "account_title"));
  const GURL kCommonUrl("http://common_url.com/");
  fake_server_->InjectEntity(
      CreateTestReadingListEntity(kCommonUrl, "common_title"));

  ASSERT_TRUE(SetupClients());
  ASSERT_THAT(model()->size(), Eq(0ul));

  model()->AddOrReplaceEntry(kCommonUrl, "common_title",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/std::nullopt,
                             /*creation_time=*/std::nullopt);
  const GURL kLocalUrl("http://local_url.com/");
  model()->AddOrReplaceEntry(kLocalUrl, "local_title",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/std::nullopt,
                             /*creation_time=*/std::nullopt);

  ASSERT_THAT(model()->size(), Eq(2ul));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));

  ASSERT_THAT(model()->size(), Eq(3ul));
  ASSERT_TRUE(model()->NeedsExplicitUploadToSyncServer(kLocalUrl));
  ASSERT_FALSE(model()->NeedsExplicitUploadToSyncServer(kCommonUrl));
  ASSERT_FALSE(model()->NeedsExplicitUploadToSyncServer(kAccountUrl));

  const std::string kNewLocalTitle("new_local_title");
  model()->SetEntryTitleIfExists(kLocalUrl, kNewLocalTitle);
  const std::string kNewCommonTitle("new_common_title");
  model()->SetEntryTitleIfExists(kCommonUrl, kNewCommonTitle);
  const std::string kNewAccountTitle("new_account_title");
  model()->SetEntryTitleIfExists(kAccountUrl, kNewAccountTitle);

  EXPECT_TRUE(model()->NeedsExplicitUploadToSyncServer(kLocalUrl));
  EXPECT_FALSE(model()->NeedsExplicitUploadToSyncServer(kCommonUrl));
  EXPECT_FALSE(model()->NeedsExplicitUploadToSyncServer(kAccountUrl));

  // Verify the merged view is updated.
  EXPECT_THAT(model()->GetEntryByURL(kLocalUrl)->Title(), Eq(kNewLocalTitle));
  EXPECT_THAT(model()->GetEntryByURL(kCommonUrl)->Title(), Eq(kNewCommonTitle));
  EXPECT_THAT(model()->GetEntryByURL(kAccountUrl)->Title(),
              Eq(kNewAccountTitle));

  // Verify that the server entries are updated.
  EXPECT_TRUE(ServerReadingListTitlesEqualityChecker(
                  {{kNewAccountTitle, kNewCommonTitle}})
                  .Wait());

  GetClient(0)->SignOutPrimaryAccount();

  // `NeedsExplicitUploadToSyncServer()` should return false when the user is
  // signed out.
  EXPECT_FALSE(model()->NeedsExplicitUploadToSyncServer(kLocalUrl));

  EXPECT_THAT(model()->size(), Eq(2ul));

  // Verify entries in the local storage are updated.
  EXPECT_THAT(model()->GetEntryByURL(kLocalUrl)->Title(), Eq(kNewLocalTitle));
  EXPECT_THAT(model()->GetEntryByURL(kCommonUrl)->Title(), Eq(kNewCommonTitle));
}

IN_PROC_BROWSER_TEST_P(SingleClientReadingListSyncTest,
                       ShouldUploadAllEntriesToTheSyncServer) {
  const GURL kUrlA("http://url_a.com/");
  const GURL kUrlB("http://url_b.com/");

  ASSERT_TRUE(SetupClients());
  ASSERT_THAT(model()->size(), Eq(0ul));
  model()->AddOrReplaceEntry(kUrlA, "title_a",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/std::nullopt,
                             /*creation_time=*/std::nullopt);
  model()->AddOrReplaceEntry(kUrlB, "title_b",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/std::nullopt,
                             /*creation_time=*/std::nullopt);

  if (GetParam() == SyncTest::SetupSyncMode::kSyncTheFeature) {
    // For full-sync, test that pre-existing local data is uploaded.
    ASSERT_TRUE(SetupSync());
    EXPECT_TRUE(ServerReadingListURLsEqualityChecker({kUrlA, kUrlB}).Wait());
  } else {
    // For transport-only, test that local data is uploaded only after explicit
    // instruction.
    ASSERT_TRUE(SetupSync());

    // In transport-only mode, local data should NOT be uploaded automatically.
    EXPECT_TRUE(ServerReadingListURLsEqualityChecker({}).Wait());

    // Trigger explicit upload.
    model()->MarkAllForUploadToSyncServerIfNeeded();

    EXPECT_TRUE(ServerReadingListURLsEqualityChecker({kUrlA, kUrlB}).Wait());
  }

  EXPECT_THAT(model()->size(), Eq(2ul));

  GetClient(0)->SignOutPrimaryAccount();
  if (GetParam() == SyncTest::SetupSyncMode::kSyncTheFeature) {
    // In full-sync mode, data is not cleared upon sign-out, which is consistent
    // with other data types like bookmarks.
    EXPECT_THAT(model()->size(), Eq(2ul));
  } else {
    // In transport-only mode, account data is cleared.
    EXPECT_THAT(model()->size(), Eq(0ul));
  }
}

// TODO: crbug.com/41490059 - Flaky on Android
IN_PROC_BROWSER_TEST_P(
    SingleClientReadingListSyncTest,
    ShouldFilterEntriesWithEmptyEntryIdUponIncrementalRemoteCreation) {
  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));

  ASSERT_THAT(model()->size(), Eq(0ul));

  const GURL kEntryWithCorruptId("http://EntryWithCorruptId.com/");
  std::unique_ptr<syncer::LoopbackServerEntity> kCorruptEntry =
      CreateTestReadingListEntity(kEntryWithCorruptId, "entry_title");
  sync_pb::EntitySpecifics specifics = kCorruptEntry->GetSpecifics();
  // An empty entry id makes it an invalid reading list specifics.
  *specifics.mutable_reading_list()->mutable_entry_id() = "";
  ASSERT_FALSE(ReadingListEntry::IsSpecificsValid(specifics.reading_list()));
  kCorruptEntry->SetSpecifics(specifics);
  fake_server_->InjectEntity(std::move(kCorruptEntry));

  const GURL kUrl("http://url.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "entry_title"));

  ASSERT_TRUE(ServerReadingListURLsEqualityChecker(
                  {kUrl, GURL("http://EntryWithCorruptId.com/")})
                  .Wait());
  EXPECT_TRUE(LocalReadingListURLsEqualityChecker(model(), {kUrl}).Wait());

  EXPECT_THAT(model()->size(), Eq(1ul));
}

IN_PROC_BROWSER_TEST_P(
    SingleClientReadingListSyncTest,
    ShouldFilterEntriesWithEmptyUrlUponIncrementalRemoteCreation) {
  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));

  ASSERT_THAT(model()->size(), Eq(0ul));

  std::unique_ptr<syncer::LoopbackServerEntity> kCorruptEntry =
      CreateTestReadingListEntity(GURL("http://CorruptEntry.com/"),
                                  "corrupt_entry_title");
  sync_pb::EntitySpecifics specifics = kCorruptEntry->GetSpecifics();
  // An empty URL makes it an invalid reading list specifics.
  *specifics.mutable_reading_list()->mutable_url() = "";
  ASSERT_FALSE(ReadingListEntry::IsSpecificsValid(specifics.reading_list()));
  kCorruptEntry->SetSpecifics(specifics);
  fake_server_->InjectEntity(std::move(kCorruptEntry));

  const GURL kUrl("http://url.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "entry_title"));

  ASSERT_TRUE(ServerReadingListURLsEqualityChecker({kUrl, GURL("")}).Wait());
  EXPECT_TRUE(LocalReadingListURLsEqualityChecker(model(), {kUrl}).Wait());

  EXPECT_THAT(model()->size(), Eq(1ul));
}

IN_PROC_BROWSER_TEST_P(
    SingleClientReadingListSyncTest,
    ShouldFilterEntriesWithUnequalEntryIdAndUrlUponIncrementalRemoteCreation) {
  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));

  ASSERT_THAT(model()->size(), Eq(0ul));

  std::unique_ptr<syncer::LoopbackServerEntity> kCorruptEntry =
      CreateTestReadingListEntity(GURL("http://CorruptEntry.com/"),
                                  "corrupt_entry_title");
  sync_pb::EntitySpecifics specifics = kCorruptEntry->GetSpecifics();
  // Unequal entry id and url makes it an invalid reading list specifics.
  *specifics.mutable_reading_list()->mutable_entry_id() =
      "http://UnequalEntryIdAndUrl.com/";
  ASSERT_FALSE(ReadingListEntry::IsSpecificsValid(specifics.reading_list()));
  kCorruptEntry->SetSpecifics(specifics);
  fake_server_->InjectEntity(std::move(kCorruptEntry));

  const GURL kUrl("http://url.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "entry_title"));

  ASSERT_TRUE(ServerReadingListURLsEqualityChecker(
                  {kUrl, GURL("http://CorruptEntry.com/")})
                  .Wait());
  EXPECT_TRUE(LocalReadingListURLsEqualityChecker(model(), {kUrl}).Wait());

  EXPECT_THAT(model()->size(), Eq(1ul));
}

IN_PROC_BROWSER_TEST_P(
    SingleClientReadingListSyncTest,
    ShouldFilterEntriesWithInvalidUrlUponIncrementalRemoteCreation) {
  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));

  ASSERT_THAT(model()->size(), Eq(0ul));

  std::unique_ptr<syncer::LoopbackServerEntity> kCorruptEntry =
      CreateTestReadingListEntity(GURL("http://CorruptEntry.com/"),
                                  "corrupt_entry_title");
  sync_pb::EntitySpecifics specifics = kCorruptEntry->GetSpecifics();
  // An invalid URL makes it an invalid reading list specifics.
  *specifics.mutable_reading_list()->mutable_url() = "CorruptEntryURL";
  ASSERT_FALSE(ReadingListEntry::IsSpecificsValid(specifics.reading_list()));
  kCorruptEntry->SetSpecifics(specifics);
  fake_server_->InjectEntity(std::move(kCorruptEntry));

  const GURL kUrl("http://url.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "entry_title"));

  ASSERT_TRUE(
      ServerReadingListURLsEqualityChecker({kUrl, GURL("CorruptEntryURL")})
          .Wait());
  EXPECT_TRUE(LocalReadingListURLsEqualityChecker(model(), {kUrl}).Wait());

  EXPECT_THAT(model()->size(), Eq(1ul));
}

IN_PROC_BROWSER_TEST_P(
    SingleClientReadingListSyncTest,
    ShouldFilterEntriesWithEmptyEntryIdUponIncrementalRemoteUpdate) {
  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  const GURL kEntryWithCorruptRemoteUpdateId(
      "http://EntryThatWillHaveCorruptRemoteUpdate.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(
      kEntryWithCorruptRemoteUpdateId, "entry_title"));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));

  EXPECT_TRUE(LocalReadingListURLsEqualityChecker(
                  model(), {kEntryWithCorruptRemoteUpdateId})
                  .Wait());
  ASSERT_THAT(model()->size(), Eq(1ul));

  std::unique_ptr<syncer::LoopbackServerEntity> kCorruptRemoteUpdate =
      CreateTestReadingListEntity(kEntryWithCorruptRemoteUpdateId,
                                  "corrupt_entry_title");
  sync_pb::EntitySpecifics specifics = kCorruptRemoteUpdate->GetSpecifics();
  // An empty entry id makes it an invalid reading list specifics.
  *specifics.mutable_reading_list()->mutable_entry_id() = "";
  ASSERT_FALSE(ReadingListEntry::IsSpecificsValid(specifics.reading_list()));
  kCorruptRemoteUpdate->SetSpecifics(specifics);
  fake_server_->InjectEntity(std::move(kCorruptRemoteUpdate));

  const GURL kUrl("http://url.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "entry_title"));

  ASSERT_TRUE(ServerReadingListURLsEqualityChecker(
                  {kUrl, kEntryWithCorruptRemoteUpdateId})
                  .Wait());
  EXPECT_TRUE(LocalReadingListURLsEqualityChecker(
                  model(), {kUrl, kEntryWithCorruptRemoteUpdateId})
                  .Wait());

  EXPECT_THAT(model()->size(), Eq(2ul));
  EXPECT_THAT(model()->GetEntryByURL(kEntryWithCorruptRemoteUpdateId)->Title(),
              Eq("entry_title"));
}

IN_PROC_BROWSER_TEST_P(
    SingleClientReadingListSyncTest,
    ShouldFilterEntriesWithEmptyUrlUponIncrementalRemoteUpdate) {
  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  const GURL kEntryWithCorruptRemoteUpdateUrl(
      "http://EntryThatWillHaveCorruptRemoteUpdate.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(
      kEntryWithCorruptRemoteUpdateUrl, "entry_title"));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));

  EXPECT_TRUE(LocalReadingListURLsEqualityChecker(
                  model(), {kEntryWithCorruptRemoteUpdateUrl})
                  .Wait());
  ASSERT_THAT(model()->size(), Eq(1ul));

  std::unique_ptr<syncer::LoopbackServerEntity> kCorruptRemoteUpdate =
      CreateTestReadingListEntity(kEntryWithCorruptRemoteUpdateUrl,
                                  "corrupt_entry_title");
  sync_pb::EntitySpecifics specifics = kCorruptRemoteUpdate->GetSpecifics();
  // An empty URL makes it an invalid reading list specifics.
  *specifics.mutable_reading_list()->mutable_url() = "";
  ASSERT_FALSE(ReadingListEntry::IsSpecificsValid(specifics.reading_list()));
  kCorruptRemoteUpdate->SetSpecifics(specifics);
  fake_server_->InjectEntity(std::move(kCorruptRemoteUpdate));

  const GURL kUrl("http://url.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "entry_title"));

  ASSERT_TRUE(ServerReadingListURLsEqualityChecker({kUrl, GURL("")}).Wait());
  EXPECT_TRUE(LocalReadingListURLsEqualityChecker(
                  model(), {kUrl, kEntryWithCorruptRemoteUpdateUrl})
                  .Wait());

  EXPECT_THAT(model()->size(), Eq(2ul));
  EXPECT_THAT(model()->GetEntryByURL(kEntryWithCorruptRemoteUpdateUrl)->Title(),
              Eq("entry_title"));
}

IN_PROC_BROWSER_TEST_P(
    SingleClientReadingListSyncTest,
    ShouldFilterEntriesWithUnequalEntryIdAndUrlUponIncrementalRemoteUpdate) {
  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  const GURL kEntryWithCorruptRemoteUpdateId(
      "http://EntryThatWillHaveCorruptRemoteUpdate.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(
      kEntryWithCorruptRemoteUpdateId, "entry_title"));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));

  EXPECT_TRUE(LocalReadingListURLsEqualityChecker(
                  model(), {kEntryWithCorruptRemoteUpdateId})
                  .Wait());
  ASSERT_THAT(model()->size(), Eq(1ul));

  std::unique_ptr<syncer::LoopbackServerEntity> kCorruptRemoteUpdate =
      CreateTestReadingListEntity(kEntryWithCorruptRemoteUpdateId,
                                  "corrupt_entry_title");
  sync_pb::EntitySpecifics specifics = kCorruptRemoteUpdate->GetSpecifics();
  // Unequal entry id and url makes it an invalid reading list specifics.
  *specifics.mutable_reading_list()->mutable_entry_id() =
      "http://UnequalEntryIdAndUrl.com/";
  ASSERT_FALSE(ReadingListEntry::IsSpecificsValid(specifics.reading_list()));
  kCorruptRemoteUpdate->SetSpecifics(specifics);
  fake_server_->InjectEntity(std::move(kCorruptRemoteUpdate));

  const GURL kUrl("http://url.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "entry_title"));

  ASSERT_TRUE(ServerReadingListURLsEqualityChecker(
                  {kUrl, kEntryWithCorruptRemoteUpdateId})
                  .Wait());
  EXPECT_TRUE(LocalReadingListURLsEqualityChecker(
                  model(), {kUrl, kEntryWithCorruptRemoteUpdateId})
                  .Wait());

  EXPECT_THAT(model()->size(), Eq(2ul));
  EXPECT_THAT(model()->GetEntryByURL(kEntryWithCorruptRemoteUpdateId)->Title(),
              Eq("entry_title"));
}

IN_PROC_BROWSER_TEST_P(
    SingleClientReadingListSyncTest,
    ShouldFilterEntriesWithInvalidUrlUponIncrementalRemoteUpdate) {
  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  const GURL kEntryWithCorruptRemoteUpdateUrl(
      "http://EntryThatWillHaveCorruptRemoteUpdate.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(
      kEntryWithCorruptRemoteUpdateUrl, "entry_title"));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));

  EXPECT_TRUE(LocalReadingListURLsEqualityChecker(
                  model(), {kEntryWithCorruptRemoteUpdateUrl})
                  .Wait());
  ASSERT_THAT(model()->size(), Eq(1ul));

  std::unique_ptr<syncer::LoopbackServerEntity> kCorruptRemoteUpdate =
      CreateTestReadingListEntity(kEntryWithCorruptRemoteUpdateUrl,
                                  "corrupt_entry_title");
  sync_pb::EntitySpecifics specifics = kCorruptRemoteUpdate->GetSpecifics();
  // An invalid URL makes it an invalid reading list specifics.
  *specifics.mutable_reading_list()->mutable_url() = "CorruptUpdateURL";
  ASSERT_FALSE(ReadingListEntry::IsSpecificsValid(specifics.reading_list()));
  kCorruptRemoteUpdate->SetSpecifics(specifics);
  fake_server_->InjectEntity(std::move(kCorruptRemoteUpdate));

  const GURL kUrl("http://url.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "entry_title"));

  ASSERT_TRUE(
      ServerReadingListURLsEqualityChecker({kUrl, GURL("CorruptUpdateURL")})
          .Wait());
  EXPECT_TRUE(LocalReadingListURLsEqualityChecker(
                  model(), {kUrl, kEntryWithCorruptRemoteUpdateUrl})
                  .Wait());

  EXPECT_THAT(model()->size(), Eq(2ul));
  EXPECT_THAT(model()->GetEntryByURL(kEntryWithCorruptRemoteUpdateUrl)->Title(),
              Eq("entry_title"));
}

IN_PROC_BROWSER_TEST_P(SingleClientReadingListSyncTest,
                       ShouldReturnLocalDataDescriptions) {
  if (GetParam() == SyncTest::SetupSyncMode::kSyncTheFeature) {
    GTEST_SKIP()
        << "This test verifies transport-only behavior where pre-existing "
           "data is not uploaded.";
  }
  ASSERT_TRUE(SetupClients());
  ASSERT_THAT(model()->size(), Eq(0ul));

  const GURL kUrlA("http://url_a.com/");
  model()->AddOrReplaceEntry(kUrlA, "title_a",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/std::nullopt,
                             /*creation_time=*/std::nullopt);

  const GURL kUrlB("http://url_b.com/");
  model()->AddOrReplaceEntry(kUrlB, "title_b",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/std::nullopt,
                             /*creation_time=*/std::nullopt);

  ASSERT_THAT(model()->GetKeys(), ElementsAre(kUrlA, kUrlB));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));

  ASSERT_THAT(model()->GetKeys(), ElementsAre(kUrlA, kUrlB));
  ASSERT_THAT(GetReadingListURLsFromFakeServer(SyncTest::GetFakeServer()),
              IsEmpty());

  EXPECT_THAT(
      GetClient(0)->GetLocalDataDescriptionAndWait(syncer::READING_LIST),
      // Items should be sorted by syncer::LocalDataItemModel::DataId.
      MatchesLocalDataDescription(
          syncer::DataType::READING_LIST,
          ElementsAre(MatchesLocalDataItemModel(
                          kUrlA, syncer::LocalDataItemModel::PageUrlIcon(kUrlA),
                          /*title=*/"title_a", /*subtitle=*/IsEmpty()),
                      MatchesLocalDataItemModel(
                          kUrlB, syncer::LocalDataItemModel::PageUrlIcon(kUrlB),
                          /*title=*/"title_b", /*subtitle=*/IsEmpty())),
          /*item_count=*/2u, /*domains=*/ElementsAre("url_a.com", "url_b.com"),
          /*domain_count=*/2u));
}

IN_PROC_BROWSER_TEST_P(SingleClientReadingListSyncTest,
                       ShouldBatchUploadAllEntries) {
  if (GetParam() == SyncTest::SetupSyncMode::kSyncTheFeature) {
    GTEST_SKIP()
        << "This test verifies transport-only behavior after sign-out.";
  }
  ASSERT_TRUE(SetupClients());
  ASSERT_THAT(model()->size(), Eq(0ul));

  const GURL kUrlA("http://url_a.com/");
  model()->AddOrReplaceEntry(kUrlA, "title_a",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/std::nullopt,
                             /*creation_time=*/std::nullopt);

  const GURL kUrlB("http://url_b.com/");
  model()->AddOrReplaceEntry(kUrlB, "title_b",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/std::nullopt,
                             /*creation_time=*/std::nullopt);

  ASSERT_THAT(model()->GetKeys(), ElementsAre(kUrlA, kUrlB));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));

  ASSERT_THAT(model()->GetKeys(), ElementsAre(kUrlA, kUrlB));
  ASSERT_THAT(GetReadingListURLsFromFakeServer(SyncTest::GetFakeServer()),
              IsEmpty());

  GetSyncService(0)->TriggerLocalDataMigration({syncer::READING_LIST});

  EXPECT_TRUE(ServerReadingListURLsEqualityChecker({kUrlA, kUrlB}).Wait());
  EXPECT_THAT(model()->GetKeys(), ElementsAre(kUrlA, kUrlB));

  GetClient(0)->SignOutPrimaryAccount();
  EXPECT_THAT(model()->GetKeys(), ElementsAre());
}

IN_PROC_BROWSER_TEST_P(SingleClientReadingListSyncTest,
                       ShouldBatchUploadSomeEntries) {
  if (GetParam() == SyncTest::SetupSyncMode::kSyncTheFeature) {
    GTEST_SKIP()
        << "This test verifies transport-only behavior after sign-out.";
  }
  ASSERT_TRUE(SetupClients());
  ASSERT_THAT(model()->size(), Eq(0ul));

  const GURL kUrlA("http://url_a.com/");
  model()->AddOrReplaceEntry(kUrlA, "title_a",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/std::nullopt,
                             /*creation_time=*/std::nullopt);

  const GURL kUrlB("http://url_b.com/");
  model()->AddOrReplaceEntry(kUrlB, "title_b",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/std::nullopt,
                             /*creation_time=*/std::nullopt);

  ASSERT_THAT(model()->GetKeys(), ElementsAre(kUrlA, kUrlB));

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));

  ASSERT_THAT(model()->GetKeys(), ElementsAre(kUrlA, kUrlB));
  ASSERT_THAT(GetReadingListURLsFromFakeServer(SyncTest::GetFakeServer()),
              IsEmpty());

  GetSyncService(0)->TriggerLocalDataMigrationForItems(
      {{syncer::DataType::READING_LIST, {kUrlA}}});

  EXPECT_TRUE(ServerReadingListURLsEqualityChecker({kUrlA}).Wait());
  EXPECT_THAT(model()->GetKeys(), ElementsAre(kUrlA, kUrlB));

  GetClient(0)->SignOutPrimaryAccount();
  EXPECT_THAT(model()->GetKeys(), ElementsAre(kUrlB));
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace
