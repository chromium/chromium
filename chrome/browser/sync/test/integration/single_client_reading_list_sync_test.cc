// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/reading_list/core/mock_reading_list_model_observer.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/reading_list_specifics.pb.h"
#include "content/public/test/browser_test.h"

using testing::Eq;

namespace {

// Checker used to block until the reading list URLs on the server match a
// given set of expected reading list URLs.
class ServerReadingListURLsEqualityChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  // |fake_server| must not be nullptr and must outlive this object.
  explicit ServerReadingListURLsEqualityChecker(std::set<GURL> expected_urls)
      : expected_urls_(std::move(expected_urls)) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for server-side reading list URLs to match expected.";

    std::vector<sync_pb::SyncEntity> entities =
        fake_server()->GetSyncEntitiesByModelType(syncer::READING_LIST);

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
        {syncer::kReadingListEnableDualReadingListModel,
         syncer::kReadingListEnableSyncTransportModeUponSignIn},
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

  raw_ptr<ReadingListModel> model() {
    return ReadingListModelFactory::GetForBrowserContext(GetProfile(0));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1455032): The following ifdef should be removed.
// Currently Android explicitly enables Sync-the-feature upon
// `SignInPrimaryAccount()` while the following tests are expecting the sync
// feature to be disabled.
#if !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(SingleClientReadingListSyncTest,
                       ShouldDownloadAccountDataUponSignin) {
  const GURL kUrl("http://url.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "entry_title"));

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  ASSERT_THAT(model()->size(), Eq(0ul));

  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));

  EXPECT_THAT(model()->size(), Eq(1ul));
  EXPECT_FALSE(model()->NeedsExplicitUploadToSyncServer(kUrl));
}

IN_PROC_BROWSER_TEST_F(SingleClientReadingListSyncTest,
                       ShouldUploadOnlyEntriesCreatedAfterSignin) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_THAT(model()->size(), Eq(0ul));

  const GURL kLocalUrl("http://local_url.com/");
  model()->AddOrReplaceEntry(kLocalUrl, "local_title",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/base::TimeDelta());

  ASSERT_THAT(model()->size(), Eq(1ul));

  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));
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

// ChromeOS doesn't have the concept of sign-out, so this only exists on other
// platforms.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(SingleClientReadingListSyncTest,
                       ShouldDeleteAccountDataUponSignout) {
  const GURL kUrl("http://url.com/");
  fake_server_->InjectEntity(CreateTestReadingListEntity(kUrl, "entry_title"));

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  ASSERT_THAT(model()->size(), Eq(0ul));

  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::READING_LIST));

  ASSERT_THAT(model()->size(), Eq(1ul));

  GetClient(0)->SignOutPrimaryAccount();
  EXPECT_THAT(model()->size(), Eq(0ul));
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
