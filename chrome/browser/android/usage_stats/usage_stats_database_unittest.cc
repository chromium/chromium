// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/usage_stats/usage_stats_database.h"

#include <utility>

#include "base/bind.h"
#include "base/time/time.h"
#include "chrome/browser/android/usage_stats/website_event.pb.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::test::FakeDB;
using testing::ElementsAre;

namespace usage_stats {

namespace {

const char kFqdn1[] = "foo.com";
const char kFqdn2[] = "bar.org";

const char kToken1[] = "token1";
const char kToken2[] = "token2";

const WebsiteEvent CreateWebsiteEvent(const std::string& fqdn,
                                      int64_t seconds,
                                      const WebsiteEvent::EventType& type) {
  WebsiteEvent event;
  event.set_fqdn(fqdn);
  Timestamp* timestamp = event.mutable_timestamp();
  timestamp->set_seconds(seconds);
  event.set_type(type);
  return event;
}

MATCHER_P(EqualsWebsiteEvent, other, "") {
  return other.fqdn() == arg.fqdn() &&
         other.timestamp().seconds() == arg.timestamp().seconds() &&
         other.timestamp().nanos() == arg.timestamp().nanos() &&
         other.type() == arg.type();
}

}  // namespace

class MockUsageStatsDatabase : public UsageStatsDatabase {
 public:
  MockUsageStatsDatabase(
      std::unique_ptr<ProtoDatabase<WebsiteEvent>> website_event_db,
      std::unique_ptr<ProtoDatabase<Suspension>> suspension_db,
      std::unique_ptr<ProtoDatabase<TokenMapping>> token_mapping_db)
      : UsageStatsDatabase(std::move(website_event_db),
                           std::move(suspension_db),
                           std::move(token_mapping_db)) {}
};

class UsageStatsDatabaseTest : public testing::Test {
 public:
  UsageStatsDatabaseTest() {
    auto fake_website_event_db =
        std::make_unique<FakeDB<WebsiteEvent>>(&website_event_store_);
    auto fake_suspension_db =
        std::make_unique<FakeDB<Suspension>>(&suspension_store_);
    auto fake_token_mapping_db =
        std::make_unique<FakeDB<TokenMapping>>(&token_mapping_store_);

    // Maintain pointers to the FakeDBs for test callback execution.
    website_event_db_unowned_ = fake_website_event_db.get();
    suspension_db_unowned_ = fake_suspension_db.get();
    token_mapping_db_unowned_ = fake_token_mapping_db.get();

    usage_stats_database_ = std::make_unique<MockUsageStatsDatabase>(
        std::move(fake_website_event_db), std::move(fake_suspension_db),
        std::move(fake_token_mapping_db));
  }

  UsageStatsDatabase* usage_stats_database() {
    return usage_stats_database_.get();
  }

  FakeDB<WebsiteEvent>* fake_website_event_db() {
    return website_event_db_unowned_;
  }

  FakeDB<Suspension>* fake_suspension_db() { return suspension_db_unowned_; }

  FakeDB<TokenMapping>* fake_token_mapping_db() {
    return token_mapping_db_unowned_;
  }

  MOCK_METHOD1(OnUpdateDone, void(UsageStatsDatabase::Error));
  MOCK_METHOD2(OnGetEventsDone,
               void(UsageStatsDatabase::Error, std::vector<WebsiteEvent>));
  MOCK_METHOD2(OnGetSuspensionsDone,
               void(UsageStatsDatabase::Error, std::vector<std::string>));
  MOCK_METHOD2(OnGetTokenMappingsDone,
               void(UsageStatsDatabase::Error, UsageStatsDatabase::TokenMap));

 private:
  std::map<std::string, WebsiteEvent> website_event_store_;
  std::map<std::string, Suspension> suspension_store_;
  std::map<std::string, TokenMapping> token_mapping_store_;

  FakeDB<WebsiteEvent>* website_event_db_unowned_;
  FakeDB<Suspension>* suspension_db_unowned_;
  FakeDB<TokenMapping>* token_mapping_db_unowned_;

  std::unique_ptr<UsageStatsDatabase> usage_stats_database_;

  DISALLOW_COPY_AND_ASSIGN(UsageStatsDatabaseTest);
};

TEST_F(UsageStatsDatabaseTest, Initialization) {
  ASSERT_NE(nullptr, usage_stats_database());
  ASSERT_NE(nullptr, fake_website_event_db());
  ASSERT_NE(nullptr, fake_suspension_db());
  ASSERT_NE(nullptr, fake_token_mapping_db());

  // Expect that Init has been called on all ProtoDatabases. If it hasn't, then
  // InitStatusCallback will cause the test to crash.
  fake_website_event_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
  fake_suspension_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
  fake_token_mapping_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
}

// Website Event Tests
TEST_F(UsageStatsDatabaseTest, GetAllEventsSuccess) {
  fake_website_event_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  usage_stats_database()->GetAllEvents(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetEventsDone, base::Unretained(this)));

  EXPECT_CALL(*this, OnGetEventsDone(UsageStatsDatabase::Error::kNoError,
                                     ElementsAre()));

  fake_website_event_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, GetAllEventsFailure) {
  fake_website_event_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  usage_stats_database()->GetAllEvents(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetEventsDone, base::Unretained(this)));

  EXPECT_CALL(*this, OnGetEventsDone(UsageStatsDatabase::Error::kUnknownError,
                                     ElementsAre()));

  fake_website_event_db()->LoadCallback(false);
}

TEST_F(UsageStatsDatabaseTest, AddEventsEmpty) {
  fake_website_event_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  std::vector<WebsiteEvent> events;

  usage_stats_database()->AddEvents(
      events, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                             base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_website_event_db()->UpdateCallback(true);
}

TEST_F(UsageStatsDatabaseTest, AddAndGetOneEvent) {
  fake_website_event_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  // Add 1 event.
  WebsiteEvent event1 =
      CreateWebsiteEvent(kFqdn1, 1, WebsiteEvent::START_BROWSING);
  std::vector<WebsiteEvent> events({event1});

  usage_stats_database()->AddEvents(
      events, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                             base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_website_event_db()->UpdateCallback(true);

  // Get 1 event.
  usage_stats_database()->GetAllEvents(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetEventsDone, base::Unretained(this)));

  EXPECT_CALL(*this, OnGetEventsDone(UsageStatsDatabase::Error::kNoError,
                                     ElementsAre(EqualsWebsiteEvent(event1))));

  fake_website_event_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, AddAndQueryEventsInRange) {
  fake_website_event_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  // Add 2 events at time 5s and 10s.
  WebsiteEvent event1 =
      CreateWebsiteEvent(kFqdn1, 5, WebsiteEvent::START_BROWSING);
  WebsiteEvent event2 =
      CreateWebsiteEvent(kFqdn2, 10, WebsiteEvent::STOP_BROWSING);
  std::vector<WebsiteEvent> events({event1, event2});

  usage_stats_database()->AddEvents(
      events, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                             base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_website_event_db()->UpdateCallback(true);

  // Get events between time 0 (inclusive) and 9 (exclusive).
  // This test validates the correct lexicographic ordering of timestamps such
  // that key(0) <= key(5) < key(9) <= key(10).
  usage_stats_database()->QueryEventsInRange(
      base::Time::FromDoubleT(0), base::Time::FromDoubleT(9),
      base::BindOnce(&UsageStatsDatabaseTest::OnGetEventsDone,
                     base::Unretained(this)));

  EXPECT_CALL(*this, OnGetEventsDone(UsageStatsDatabase::Error::kNoError,
                                     ElementsAre(EqualsWebsiteEvent(event1))));

  fake_website_event_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, AddAndDeleteAllEvents) {
  fake_website_event_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  // Add 1 event.
  WebsiteEvent event1 =
      CreateWebsiteEvent(kFqdn1, 1, WebsiteEvent::START_BROWSING);
  std::vector<WebsiteEvent> events({event1});

  usage_stats_database()->AddEvents(
      events, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                             base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_website_event_db()->UpdateCallback(true);

  // Delete all events.
  usage_stats_database()->DeleteAllEvents(base::BindOnce(
      &UsageStatsDatabaseTest::OnUpdateDone, base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_website_event_db()->UpdateCallback(true);

  // Get all events (expecting none).
  usage_stats_database()->GetAllEvents(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetEventsDone, base::Unretained(this)));

  EXPECT_CALL(*this, OnGetEventsDone(UsageStatsDatabase::Error::kNoError,
                                     ElementsAre()));

  fake_website_event_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, AddAndDeleteEventsInRange) {
  fake_website_event_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  // Add 3 events.
  WebsiteEvent event1 =
      CreateWebsiteEvent(kFqdn1, 1, WebsiteEvent::START_BROWSING);
  WebsiteEvent event2 =
      CreateWebsiteEvent(kFqdn1, 2, WebsiteEvent::START_BROWSING);
  WebsiteEvent event3 =
      CreateWebsiteEvent(kFqdn1, 10, WebsiteEvent::START_BROWSING);
  std::vector<WebsiteEvent> events({event1, event2, event3});

  usage_stats_database()->AddEvents(
      events, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                             base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_website_event_db()->UpdateCallback(true);

  // Delete events between time 1 (inclusive) and 10 (exclusive).
  usage_stats_database()->DeleteEventsInRange(
      base::Time::FromDoubleT(1), base::Time::FromDoubleT(10),
      base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                     base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_website_event_db()->LoadCallback(true);
  fake_website_event_db()->UpdateCallback(true);

  // Get 1 remaining event outside range (at time 10).
  usage_stats_database()->GetAllEvents(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetEventsDone, base::Unretained(this)));

  EXPECT_CALL(*this, OnGetEventsDone(UsageStatsDatabase::Error::kNoError,
                                     ElementsAre(EqualsWebsiteEvent(event3))));

  fake_website_event_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, ExpiryDeletesOldEvents) {
  fake_website_event_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  // Add 3 events.
  base::Time now = base::Time::NowFromSystemTime();
  int64_t now_in_seconds = (int64_t)now.ToDoubleT();
  WebsiteEvent event1 = CreateWebsiteEvent(kFqdn1, now_in_seconds + 1,
                                           WebsiteEvent::START_BROWSING);
  WebsiteEvent event2 = CreateWebsiteEvent(kFqdn1, now_in_seconds + 2,
                                           WebsiteEvent::START_BROWSING);
  WebsiteEvent event3 = CreateWebsiteEvent(kFqdn1, now_in_seconds + 10,
                                           WebsiteEvent::START_BROWSING);
  std::vector<WebsiteEvent> events({event1, event2, event3});

  usage_stats_database()->AddEvents(
      events, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                             base::Unretained(this)));
  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_website_event_db()->UpdateCallback(true);

  // Advance "now" by 7 days + 9 seconds so that the first two events are > 7
  // days old.
  now = now +
        base::TimeDelta::FromDays(UsageStatsDatabase::EXPIRY_THRESHOLD_DAYS) +
        base::TimeDelta::FromSeconds(9);
  usage_stats_database()->ExpireEvents(now);

  fake_website_event_db()->LoadCallback(true);
  fake_website_event_db()->UpdateCallback(true);

  usage_stats_database()->GetAllEvents(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetEventsDone, base::Unretained(this)));

  EXPECT_CALL(*this, OnGetEventsDone(UsageStatsDatabase::Error::kNoError,
                                     ElementsAre(EqualsWebsiteEvent(event3))));

  fake_website_event_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, AddAndDeleteEventsMatchingDomain) {
  fake_website_event_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  // Add 3 events.
  WebsiteEvent event1 =
      CreateWebsiteEvent(kFqdn1, 1, WebsiteEvent::START_BROWSING);
  WebsiteEvent event2 =
      CreateWebsiteEvent(kFqdn1, 1, WebsiteEvent::STOP_BROWSING);
  WebsiteEvent event3 =
      CreateWebsiteEvent(kFqdn2, 1, WebsiteEvent::START_BROWSING);
  std::vector<WebsiteEvent> events({event1, event2, event3});

  usage_stats_database()->AddEvents(
      events, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                             base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_website_event_db()->UpdateCallback(true);

  // Delete 2 events by FQDN.
  base::flat_set<std::string> domains({kFqdn1});

  usage_stats_database()->DeleteEventsWithMatchingDomains(
      domains, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                              base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_website_event_db()->UpdateCallback(true);

  // Get 1 remaining event with non-matching FQDN.
  usage_stats_database()->GetAllEvents(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetEventsDone, base::Unretained(this)));

  EXPECT_CALL(*this, OnGetEventsDone(UsageStatsDatabase::Error::kNoError,
                                     ElementsAre(EqualsWebsiteEvent(event3))));

  fake_website_event_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, GetAllEventsDeferred) {
  // Don't complete the database initialization yet.

  // Make request to database.
  usage_stats_database()->GetAllEvents(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetEventsDone, base::Unretained(this)));

  // Expect callback to be run after initialization succeeds.
  EXPECT_CALL(*this, OnGetEventsDone(UsageStatsDatabase::Error::kNoError,
                                     ElementsAre()));

  fake_website_event_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  fake_website_event_db()->LoadCallback(true);
}

// Suspension Tests
TEST_F(UsageStatsDatabaseTest, SetSuspensionsSuccess) {
  fake_suspension_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  base::flat_set<std::string> domains({kFqdn1, kFqdn2});

  usage_stats_database()->SetSuspensions(
      domains, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                              base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_suspension_db()->UpdateCallback(true);
}

TEST_F(UsageStatsDatabaseTest, SetSuspensionsFailure) {
  fake_suspension_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  base::flat_set<std::string> domains({kFqdn1, kFqdn2});

  usage_stats_database()->SetSuspensions(
      domains, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                              base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kUnknownError));

  fake_suspension_db()->UpdateCallback(false);
}

TEST_F(UsageStatsDatabaseTest, GetAllSuspensionsSuccess) {
  fake_suspension_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  std::vector<std::string> expected;

  usage_stats_database()->GetAllSuspensions(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetSuspensionsDone, base::Unretained(this)));

  EXPECT_CALL(*this, OnGetSuspensionsDone(UsageStatsDatabase::Error::kNoError,
                                          expected));

  fake_suspension_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, GetAllSuspensionsFailure) {
  fake_suspension_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  std::vector<std::string> expected;

  usage_stats_database()->GetAllSuspensions(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetSuspensionsDone, base::Unretained(this)));

  EXPECT_CALL(*this, OnGetSuspensionsDone(
                         UsageStatsDatabase::Error::kUnknownError, expected));

  fake_suspension_db()->LoadCallback(false);
}

TEST_F(UsageStatsDatabaseTest, SetAndGetSuspension) {
  fake_suspension_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  // Insert 1 suspension.
  base::flat_set<std::string> domains({kFqdn1});

  usage_stats_database()->SetSuspensions(
      domains, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                              base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_suspension_db()->UpdateCallback(true);

  // Get 1 suspension.
  std::vector<std::string> expected({kFqdn1});

  usage_stats_database()->GetAllSuspensions(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetSuspensionsDone, base::Unretained(this)));

  EXPECT_CALL(*this, OnGetSuspensionsDone(UsageStatsDatabase::Error::kNoError,
                                          expected));

  fake_suspension_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, SetRemoveAndGetSuspension) {
  fake_suspension_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  // Insert 2 suspensions.
  base::flat_set<std::string> domains1({kFqdn1, kFqdn2});

  usage_stats_database()->SetSuspensions(
      domains1, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                               base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_suspension_db()->UpdateCallback(true);

  // Insert 1 suspension, and remove the other.
  base::flat_set<std::string> domains2({kFqdn1});

  usage_stats_database()->SetSuspensions(
      domains2, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                               base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_suspension_db()->UpdateCallback(true);

  // Get 1 suspension.
  std::vector<std::string> expected({kFqdn1});

  usage_stats_database()->GetAllSuspensions(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetSuspensionsDone, base::Unretained(this)));

  EXPECT_CALL(*this, OnGetSuspensionsDone(UsageStatsDatabase::Error::kNoError,
                                          expected));

  fake_suspension_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, SetAndGetSuspensionDeferred) {
  // Fail to initialize the database.
  fake_suspension_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kError);

  // Insert 1 suspension.
  base::flat_set<std::string> domains({kFqdn1});

  usage_stats_database()->SetSuspensions(
      domains, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                              base::Unretained(this)));

  // Get 1 suspension.
  std::vector<std::string> expected({kFqdn1});

  usage_stats_database()->GetAllSuspensions(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetSuspensionsDone, base::Unretained(this)));

  // Now successfully initialize database, and expect previous callbacks to be
  // run.
  fake_suspension_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_suspension_db()->UpdateCallback(true);

  EXPECT_CALL(*this, OnGetSuspensionsDone(UsageStatsDatabase::Error::kNoError,
                                          expected));

  fake_suspension_db()->LoadCallback(true);
}

// Token Mapping Tests
TEST_F(UsageStatsDatabaseTest, SetTokenMappingsSuccess) {
  fake_token_mapping_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  UsageStatsDatabase::TokenMap mappings({{kToken1, kFqdn1}, {kToken2, kFqdn2}});

  usage_stats_database()->SetTokenMappings(
      mappings, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                               base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_token_mapping_db()->UpdateCallback(true);
}

TEST_F(UsageStatsDatabaseTest, SetTokenMappingsFailure) {
  fake_token_mapping_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  UsageStatsDatabase::TokenMap mappings({{kToken1, kFqdn1}, {kToken2, kFqdn2}});

  usage_stats_database()->SetTokenMappings(
      mappings, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                               base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kUnknownError));

  fake_token_mapping_db()->UpdateCallback(false);
}

TEST_F(UsageStatsDatabaseTest, GetAllTokenMappingsSuccess) {
  fake_token_mapping_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  UsageStatsDatabase::TokenMap expected;

  usage_stats_database()->GetAllTokenMappings(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetTokenMappingsDone, base::Unretained(this)));

  EXPECT_CALL(*this, OnGetTokenMappingsDone(UsageStatsDatabase::Error::kNoError,
                                            expected));

  fake_token_mapping_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, GetAllTokenMappingsFailure) {
  fake_token_mapping_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  UsageStatsDatabase::TokenMap expected;

  usage_stats_database()->GetAllTokenMappings(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetTokenMappingsDone, base::Unretained(this)));

  EXPECT_CALL(*this, OnGetTokenMappingsDone(
                         UsageStatsDatabase::Error::kUnknownError, expected));

  fake_token_mapping_db()->LoadCallback(false);
}

TEST_F(UsageStatsDatabaseTest, SetAndGetTokenMapping) {
  fake_token_mapping_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  UsageStatsDatabase::TokenMap mapping({{kToken1, kFqdn1}});

  // Insert 1 token mapping.
  usage_stats_database()->SetTokenMappings(
      mapping, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                              base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_token_mapping_db()->UpdateCallback(true);

  // Get 1 token mapping.
  usage_stats_database()->GetAllTokenMappings(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetTokenMappingsDone, base::Unretained(this)));

  EXPECT_CALL(*this, OnGetTokenMappingsDone(UsageStatsDatabase::Error::kNoError,
                                            mapping));

  fake_token_mapping_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, SetRemoveAndGetTokenMapping) {
  fake_token_mapping_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);

  // Insert 2 token mappings.
  UsageStatsDatabase::TokenMap mappings1(
      {{kToken1, kFqdn1}, {kToken2, kFqdn2}});

  usage_stats_database()->SetTokenMappings(
      mappings1, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                                base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_token_mapping_db()->UpdateCallback(true);

  // Re-insert 1 token mapping, and remove the other.apping) {
  UsageStatsDatabase::TokenMap mappings2({{kToken1, kFqdn1}});

  usage_stats_database()->SetTokenMappings(
      mappings2, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                                base::Unretained(this)));

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  fake_token_mapping_db()->UpdateCallback(true);

  // Get 1 remaining token mapping.
  usage_stats_database()->GetAllTokenMappings(base::BindOnce(
      &UsageStatsDatabaseTest::OnGetTokenMappingsDone, base::Unretained(this)));

  EXPECT_CALL(*this, OnGetTokenMappingsDone(UsageStatsDatabase::Error::kNoError,
                                            mappings2));

  fake_token_mapping_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, SetTokenMappingsUninitialized) {
  // Fail to initialize database.
  fake_token_mapping_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kError);

  UsageStatsDatabase::TokenMap mappings({{kToken1, kFqdn1}, {kToken2, kFqdn2}});

  // Expect callback will not be run.
  EXPECT_CALL(*this, OnUpdateDone).Times(0);

  usage_stats_database()->SetTokenMappings(
      mappings, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                               base::Unretained(this)));
}

}  // namespace usage_stats
