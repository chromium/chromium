// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
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
#include "components/sync/test/test_matchers.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"

namespace {

using syncer::MatchesDeletionOrigin;
using testing::ElementsAre;
using testing::Eq;

// Checker used to block until the reading list URLs on the server match a
// given set of expected reading list URLs.
class ServerReadingListURLsEqualityChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  explicit ServerReadingListURLsEqualityChecker(
      const std::set<GURL>& expected_urls)
      : expected_urls_(std::move(expected_urls)) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for server-side reading list URLs to match expected.";

    std::vector<sync_pb::SyncEntity> entities =
        fake_server()->GetSyncEntitiesByDataType(syncer::READING_LIST);

    std::set<GURL> actual_urls;
    for (const sync_pb::SyncEntity& entity : entities) {
      actual_urls.insert(GURL(entity.specifics().reading_list().url()));
    }

    testing::StringMatchResultListener result_listener;
    const bool matches = ExplainMatchResult(
        testing::ContainerEq(expected_urls_), actual_urls, &result_listener);
    *os << result_listener.str();
    return matches;
  }

  ServerReadingListURLsEqualityChecker(
      const ServerReadingListURLsEqualityChecker&) = delete;
  ServerReadingListURLsEqualityChecker& operator=(
      const ServerReadingListURLsEqualityChecker&) = delete;

  ~ServerReadingListURLsEqualityChecker() override = default;

 private:
  const std::set<GURL> expected_urls_;
};

class LocalReadingListURLsEqualityChecker
    : public StatusChangeChecker,
      public testing::NiceMock<MockReadingListModelObserver> {
 public:
  LocalReadingListURLsEqualityChecker(ReadingListModel* model,
                                      const base::flat_set<GURL>& expected_urls)
      : model_(model), expected_urls_(std::move(expected_urls)) {
    model_->AddObserver(this);
  }

  LocalReadingListURLsEqualityChecker(
      const LocalReadingListURLsEqualityChecker&) = delete;
  LocalReadingListURLsEqualityChecker& operator=(
      const LocalReadingListURLsEqualityChecker&) = delete;

  ~LocalReadingListURLsEqualityChecker() override {
    model_->RemoveObserver(this);
  }

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for local reading list URLs to match the expected URLs.";

    testing::StringMatchResultListener result_listener;
    const bool matches =
        ExplainMatchResult(testing::ContainerEq(expected_urls_),
                           model_->GetKeys(), &result_listener);
    *os << result_listener.str();
    return matches;
  }

  // ReadingListModelObserver implementation.
  void ReadingListDidApplyChanges(ReadingListModel* model) override {
    CheckExitCondition();
  }

 private:
  const raw_ptr<ReadingListModel> model_;
  const base::flat_set<GURL> expected_urls_;
};

// Checker used to block until the reading set titles on the server match a
// given set of expected reading list titles.
class ServerReadingListTitlesEqualityChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  explicit ServerReadingListTitlesEqualityChecker(
      std::set<std::string> expected_titles)
      : expected_titles_(std::move(expected_titles)) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for server-side reading list titles to match expected.";

    std::vector<sync_pb::SyncEntity> entities =
        fake_server()->GetSyncEntitiesByDataType(syncer::READING_LIST);

    std::set<std::string> actual_titles;
    for (const sync_pb::SyncEntity& entity : entities) {
      actual_titles.insert(entity.specifics().reading_list().title());
    }

    testing::StringMatchResultListener result_listener;
    const bool matches =
        ExplainMatchResult(testing::ContainerEq(expected_titles_),
                           actual_titles, &result_listener);
    *os << result_listener.str();
    return matches;
  }

  ServerReadingListTitlesEqualityChecker(
      const ServerReadingListTitlesEqualityChecker&) = delete;
  ServerReadingListTitlesEqualityChecker& operator=(
      const ServerReadingListTitlesEqualityChecker&) = delete;

  ~ServerReadingListTitlesEqualityChecker() override = default;

 private:
  const std::set<std::string> expected_titles_;
};

void WaitForReadingListModelLoaded(ReadingListModel* reading_list_model) {
  testing::NiceMock<MockReadingListModelObserver> observer_;
  base::RunLoop run_loop;
  EXPECT_CALL(observer_, ReadingListModelLoaded).WillOnce([&run_loop] {
    run_loop.Quit();
  });
  reading_list_model->AddObserver(&observer_);
  run_loop.Run();
  reading_list_model->RemoveObserver(&observer_);
}

std::unique_ptr<syncer::LoopbackServerEntity> CreateTestReadingListEntity(
    const GURL& url,
    const std::string& entry_title) {
  sync_pb::EntitySpecifics specifics;
  *specifics.mutable_reading_list() = *base::MakeRefCounted<ReadingListEntry>(
                                           url, entry_title, base::Time::Now())
                                           ->AsReadingListSpecifics()
                                           .get();
  return syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
      "non_unique_name", url.spec(), specifics,
      /*creation_time=*/syncer::TimeToProtoTime(base::Time::Now()),
      /*last_modified_time=*/syncer::TimeToProtoTime(base::Time::Now()));
}

class SingleClientReadingListSyncTest : public SyncTest {
 public:
  SingleClientReadingListSyncTest() : SyncTest(SINGLE_CLIENT) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {syncer::kReadingListEnableSyncTransportModeUponSignIn},
        /*disabled_features=*/{});
  }

  SingleClientReadingListSyncTest(const SingleClientReadingListSyncTest&) =
      delete;
  SingleClientReadingListSyncTest& operator=(
      const SingleClientReadingListSyncTest&) = delete;

  ~SingleClientReadingListSyncTest() override = default;

  bool SetupClients() override {
    if (!SyncTest::SetupClients()) {
      return false;
    }

    WaitForReadingListModelLoaded(model());
    return true;
  }

  void SignInAndWaitForReadingListActive() {
    ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
    // Note: Depending on the state of feature flags (specifically
    // kReplaceSyncPromosWithSignInPromos), ReadingList may or may not be
    // considered selected by default.
    GetSyncService(0)->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kReadingList, true);
    ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
    ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
    ASSERT_TRUE(
        GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));
  }

  raw_ptr<ReadingListModel> model() {
    return ReadingListModelFactory::GetForBrowserContext(GetProfile(0));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SingleClientReadingListSyncTest,
                       ShouldDownloadAccountDataUponSignin) {
  const GURL kUrl("http://url.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "entry_title"));

  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  SignInAndWaitForReadingListActive();

  EXPECT_THAT(model()->size(), Eq(1ul));
  EXPECT_FALSE(model()->NeedsExplicitUploadToSyncServer(kUrl));
}

IN_PROC_BROWSER_TEST_F(SingleClientReadingListSyncTest,
                       ShouldUploadOnlyEntriesCreatedAfterSignin) {
  ASSERT_TRUE(SetupClients());
  ASSERT_THAT(model()->size(), Eq(0ul));

  const GURL kLocalUrl("http://local_url.com/");
  model()->AddOrReplaceEntry(kLocalUrl, "local_title",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/base::TimeDelta());

  ASSERT_THAT(model()->size(), Eq(1ul));

  SignInAndWaitForReadingListActive();
  ASSERT_THAT(model()->size(), Eq(1ul));

  const GURL kAccountUrl("http://account_url.com/");
  model()->AddOrReplaceEntry(kAccountUrl, "account_title",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/base::TimeDelta());

  ASSERT_THAT(model()->size(), Eq(2ul));

  EXPECT_TRUE(model()->NeedsExplicitUploadToSyncServer(kLocalUrl));
  EXPECT_FALSE(model()->NeedsExplicitUploadToSyncServer(kAccountUrl));
  EXPECT_TRUE(ServerReadingListURLsEqualityChecker({{kAccountUrl}}).Wait());
  EXPECT_TRUE(model()->NeedsExplicitUploadToSyncServer(kLocalUrl));
  EXPECT_FALSE(model()->NeedsExplicitUploadToSyncServer(kAccountUrl));
}

IN_PROC_BROWSER_TEST_F(SingleClientReadingListSyncTest,
                       ShouldDeleteTheDeletedEntryFromTheServer) {
  const GURL kUrl("http://url.com/");
  const base::Location kLocation = FROM_HERE;

  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "entry_title"));

  ASSERT_TRUE(SetupClients());
  ASSERT_THAT(model()->size(), Eq(0ul));
  SignInAndWaitForReadingListActive();
  ASSERT_THAT(model()->size(), Eq(1ul));

  model()->RemoveEntryByURL(kUrl, kLocation);
  ASSERT_THAT(model()->size(), Eq(0ul));
  EXPECT_TRUE(ServerReadingListURLsEqualityChecker({}).Wait());

  EXPECT_THAT(GetFakeServer()->GetCommittedDeletionOrigins(
                  syncer::DataType::READING_LIST),
              ElementsAre(MatchesDeletionOrigin(
                  version_info::GetVersionNumber(), kLocation)));
}

IN_PROC_BROWSER_TEST_F(SingleClientReadingListSyncTest,
                       ShouldDeleteAllEntriesFromTheServer) {
  const base::Location kLocation = FROM_HERE;

  fake_server_->InjectEntity(
      CreateTestReadingListEntity(GURL("http://url1.com/"), "entry_title1"));
  fake_server_->InjectEntity(
      CreateTestReadingListEntity(GURL("http://url2.com/"), "entry_title2"));

  ASSERT_TRUE(SetupClients());
  ASSERT_THAT(model()->size(), Eq(0ul));
  SignInAndWaitForReadingListActive();
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
#if !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(SingleClientReadingListSyncTest,
                       ShouldDeleteAccountDataUponSignout) {
  const GURL kUrl("http://url.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "entry_title"));

  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  SignInAndWaitForReadingListActive();

  ASSERT_THAT(model()->size(), Eq(1ul));

  GetClient(0)->SignOutPrimaryAccount();
  EXPECT_THAT(model()->size(), Eq(0ul));
}

IN_PROC_BROWSER_TEST_F(SingleClientReadingListSyncTest,
                       ShouldUpdateEntriesLocallyAndServerSide) {
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
                             /*estimated_read_time=*/base::TimeDelta());
  const GURL kLocalUrl("http://local_url.com/");
  model()->AddOrReplaceEntry(kLocalUrl, "local_title",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/base::TimeDelta());

  ASSERT_THAT(model()->size(), Eq(2ul));

  SignInAndWaitForReadingListActive();

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

IN_PROC_BROWSER_TEST_F(SingleClientReadingListSyncTest,
                       ShouldUploadAllEntriesToTheSyncServer) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_THAT(model()->size(), Eq(0ul));

  const GURL kUrlA("http://url_a.com/");
  model()->AddOrReplaceEntry(kUrlA, "title_a",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/base::TimeDelta());

  const GURL kUrlB("http://url_b.com/");
  model()->AddOrReplaceEntry(kUrlB, "title_b",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/base::TimeDelta());

  ASSERT_THAT(model()->size(), Eq(2ul));

  SignInAndWaitForReadingListActive();

  ASSERT_THAT(model()->size(), Eq(2ul));
  ASSERT_TRUE(ServerReadingListURLsEqualityChecker({}).Wait());

  model()->MarkAllForUploadToSyncServerIfNeeded();

  EXPECT_TRUE(ServerReadingListURLsEqualityChecker({kUrlA, kUrlB}).Wait());
  EXPECT_THAT(model()->size(), Eq(2ul));

  GetClient(0)->SignOutPrimaryAccount();
  EXPECT_THAT(model()->size(), Eq(0ul));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientReadingListSyncTest,
    ShouldFilterEntriesWithEmptyEntryIdUponIncrementalRemoteCreation) {
  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  SignInAndWaitForReadingListActive();

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

IN_PROC_BROWSER_TEST_F(
    SingleClientReadingListSyncTest,
    ShouldFilterEntriesWithEmptyUrlUponIncrementalRemoteCreation) {
  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  SignInAndWaitForReadingListActive();

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

IN_PROC_BROWSER_TEST_F(
    SingleClientReadingListSyncTest,
    ShouldFilterEntriesWithUnequalEntryIdAndUrlUponIncrementalRemoteCreation) {
  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  SignInAndWaitForReadingListActive();

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

IN_PROC_BROWSER_TEST_F(
    SingleClientReadingListSyncTest,
    ShouldFilterEntriesWithInvalidUrlUponIncrementalRemoteCreation) {
  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  SignInAndWaitForReadingListActive();

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

// TODO: crbug.com/41490059 - Flaky on Android
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ShouldFilterEntriesWithEmptyEntryIdUponIncrementalRemoteUpdate \
  DISABLED_ShouldFilterEntriesWithEmptyEntryIdUponIncrementalRemoteUpdate
#else
#define MAYBE_ShouldFilterEntriesWithEmptyEntryIdUponIncrementalRemoteUpdate \
  ShouldFilterEntriesWithEmptyEntryIdUponIncrementalRemoteUpdate
#endif
IN_PROC_BROWSER_TEST_F(
    SingleClientReadingListSyncTest,
    MAYBE_ShouldFilterEntriesWithEmptyEntryIdUponIncrementalRemoteUpdate) {
  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  const GURL kEntryWithCorruptRemoteUpdateId(
      "http://EntryThatWillHaveCorruptRemoteUpdate.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(
      kEntryWithCorruptRemoteUpdateId, "entry_title"));

  SignInAndWaitForReadingListActive();

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

IN_PROC_BROWSER_TEST_F(
    SingleClientReadingListSyncTest,
    ShouldFilterEntriesWithEmptyUrlUponIncrementalRemoteUpdate) {
  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  const GURL kEntryWithCorruptRemoteUpdateUrl(
      "http://EntryThatWillHaveCorruptRemoteUpdate.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(
      kEntryWithCorruptRemoteUpdateUrl, "entry_title"));

  SignInAndWaitForReadingListActive();

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

IN_PROC_BROWSER_TEST_F(
    SingleClientReadingListSyncTest,
    ShouldFilterEntriesWithUnequalEntryIdAndUrlUponIncrementalRemoteUpdate) {
  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  const GURL kEntryWithCorruptRemoteUpdateId(
      "http://EntryThatWillHaveCorruptRemoteUpdate.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(
      kEntryWithCorruptRemoteUpdateId, "entry_title"));

  SignInAndWaitForReadingListActive();

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

IN_PROC_BROWSER_TEST_F(
    SingleClientReadingListSyncTest,
    ShouldFilterEntriesWithInvalidUrlUponIncrementalRemoteUpdate) {
  ASSERT_TRUE(SetupClients());

  ASSERT_THAT(model()->size(), Eq(0ul));

  const GURL kEntryWithCorruptRemoteUpdateUrl(
      "http://EntryThatWillHaveCorruptRemoteUpdate.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(
      kEntryWithCorruptRemoteUpdateUrl, "entry_title"));

  SignInAndWaitForReadingListActive();

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

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
