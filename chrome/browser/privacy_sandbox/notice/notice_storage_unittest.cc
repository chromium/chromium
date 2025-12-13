// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"

#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

using notice::mojom::PrivacySandboxNotice;
using Event = notice::mojom::PrivacySandboxNoticeEvent;
using enum Event;

using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ValuesIn;

using EventTimePair = NoticeEventTimestampPair;

// Feature providing the storage name for the default notices.
BASE_FEATURE(kTestFeature1,
             "Notice1StorageName",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeature2,
             "Notice2StorageName",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Notice ID for the default notice in the catalog.
constexpr NoticeId kNotice1 = {PrivacySandboxNotice::kTopicsConsentNotice,
                               SurfaceType::kDesktopNewTab};
constexpr NoticeId kNotice2 = {PrivacySandboxNotice::kTopicsConsentNotice,
                               SurfaceType::kClankCustomTab};

std::unique_ptr<Notice> MakeNoticeWithFeature(NoticeId id,
                                              const base::Feature& feature) {
  auto notice = std::make_unique<Notice>(id);
  notice->SetFeature(&feature);
  return notice;
}

base::Time TimeFromMs(int64_t ms) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(ms));
}

void ParseDict(base::Value::Dict* dict, std::string&& json_string) {
  auto parsed_json_data = base::JSONReader::ReadDict(
      json_string, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(parsed_json_data.has_value());
  *dict = std::move(*parsed_json_data);
}

std::vector<std::unique_ptr<EventTimePair>> BuildEvents(
    std::initializer_list<EventTimePair> raw_events) {
  std::vector<std::unique_ptr<EventTimePair>> events;
  events.reserve(raw_events.size());
  for (const auto& raw_event : raw_events) {
    events.emplace_back(std::make_unique<EventTimePair>(raw_event));
  }
  return events;
}

class PrivacySandboxNoticeStorageTest : public testing::Test {
 public:
  PrivacySandboxNoticeStorageTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    PrivacySandboxNoticeStorage::RegisterProfilePrefs(prefs()->registry());
    notice_storage_ = std::make_unique<PrivacySandboxNoticeStorage>(prefs());
  }

  PrivacySandboxNoticeStorage* notice_storage() {
    return notice_storage_.get();
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

  const Notice& notice_1() { return *notice_1_.get(); }

  const Notice& notice_2() const { return *notice_2_.get(); }

 protected:
  void SetNoticeStateFromJSON(const std::string& notice_name,
                              std::string&& json_data_string) {
    base::Value::Dict notice_data_dict;
    ParseDict(&notice_data_dict, std::move(json_data_string));
    ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
    update->Set(notice_name, std::move(notice_data_dict));
  }

  base::Time AdvanceMs(int64_t ms) {
    task_env_.AdvanceClock(base::Milliseconds(ms));
    return base::Time::Now();
  }

  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_env_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<PrivacySandboxNoticeStorage> notice_storage_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  // Notices
  std::unique_ptr<Notice> notice_1_ =
      MakeNoticeWithFeature(kNotice1, kTestFeature1);
  std::unique_ptr<Notice> notice_2_ =
      MakeNoticeWithFeature(kNotice2, kTestFeature2);
  std::vector<Notice*> notices_{notice_1_.get(), notice_2_.get()};
};

TEST_F(PrivacySandboxNoticeStorageTest, NoticePathNotFound) {
  const auto actual = notice_storage()->ReadNoticeData("Notice1StorageName");
  EXPECT_FALSE(actual.has_value());
}

const auto kStartupTestValues =
    std::vector<std::tuple<std::vector<Event>, std::optional<Event>>>{
        {{}, std::nullopt},
        {{kShown}, kShown},
        {{kShown, kClosed}, kClosed},
        {{kShown, kSettings, kShown, kOptIn}, kOptIn},
        {{kShown, kOptOut}, kOptOut},
        {{kShown, kAck}, kAck},
        {{kShown, kSettings}, kSettings}};

class PrivacySandboxNoticeStorageStartupTest
    : public PrivacySandboxNoticeStorageTest,
      public testing::WithParamInterface<
          std::tuple<std::vector<Event>, std::optional<Event>>> {};

TEST_P(PrivacySandboxNoticeStorageStartupTest, StartupStateEmitsSuccessfully) {
  auto [events, expected] = GetParam();
  for (auto event : events) {
    notice_storage()->RecordEvent(notice_1(), event);
    AdvanceMs(10);
  }

  notice_storage()->RecordStartupHistograms();
  if (expected) {
    histogram_tester_.ExpectBucketCount(
        "PrivacySandbox.Notice.Startup.LastRecordedEvent.Notice1StorageName",
        *expected, 1);
  } else {
    const std::string histograms = histogram_tester_.GetAllHistogramsRecorded();
    EXPECT_THAT(histograms,
                Not(AnyOf("PrivacySandbox.Notice.Startup.LastRecordedEvent."
                          "Notice1StorageName")));
  }
}

INSTANTIATE_TEST_SUITE_P(PrivacySandboxNoticeStorageStartupTest,
                         PrivacySandboxNoticeStorageStartupTest,
                         ValuesIn(kStartupTestValues));

TEST_F(PrivacySandboxNoticeStorageTest, EventShownHistogramsEmitSuccessfully) {
  notice_storage()->RecordEvent(notice_1(), kShown);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.Notice1StorageName", true, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.Notice1StorageName", kShown, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, ActionEventHistogramsEmitSuccessfully) {
  notice_storage()->RecordEvent(notice_1(), kShown);
  AdvanceMs(10);
  notice_storage()->RecordEvent(notice_1(), kAck);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.Notice1StorageName", kAck, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.Notice1StorageName",
      NoticeActionTaken::kAck, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "Notice1StorageName_Ack",
      base::Milliseconds(10), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "Notice1StorageName_Ack",
      base::Milliseconds(10), 1);
}

MATCHER(EventTimestampEq, "") {
  const auto& actual = std::get<0>(arg);
  const auto& expected = std::get<1>(arg);

  if (!actual && !expected) {
    return true;
  }
  if (!actual || !expected) {
    return false;
  }

  return *actual == *expected;
}

class PrivacySandboxNoticeStorageEventPopulationTest
    : public PrivacySandboxNoticeStorageTest,
      public testing::WithParamInterface<std::vector<Event>> {};

TEST_P(PrivacySandboxNoticeStorageEventPopulationTest, SetsEventsAndReadsData) {
  auto events = GetParam();
  std::vector<testing::Matcher<const std::unique_ptr<EventTimePair>&>> expected;
  for (auto event : events) {
    base::Time timestamp = base::Time::Now();
    notice_storage()->RecordEvent(notice_1(), event);
    expected.emplace_back(Pointee(Eq(EventTimePair{event, timestamp})));
    AdvanceMs(10);
  }

  const auto actual = notice_storage()->ReadNoticeData("Notice1StorageName");
  if (events.empty()) {
    EXPECT_FALSE(actual.has_value());
  } else {
    ASSERT_TRUE(actual.has_value());
    EXPECT_EQ(actual->notice_events.size(), expected.size());
    EXPECT_THAT(actual->notice_events, ElementsAreArray(expected));
  }
}

INSTANTIATE_TEST_SUITE_P(PrivacySandboxNoticeStorageEventPopulationTest,
                         PrivacySandboxNoticeStorageEventPopulationTest,
                         ValuesIn(std::vector<std::vector<Event>>{
                             {},
                             {kShown, kAck, kShown},
                             {kShown, kShown},
                             {kShown, kAck, kShown, kOptIn},
                             {kShown, kAck, kSettings, kShown, kOptIn},
                             {kShown, kShown, kAck, kSettings, kShown, kShown,
                              kOptIn}}));

TEST_F(PrivacySandboxNoticeStorageTest, ReActionRegistersAndEmitsHistogram) {
  base::Time t0, t1, t2;
  t0 = base::Time::Now();
  notice_storage()->RecordEvent(notice_1(), kShown);
  t1 = AdvanceMs(100);
  notice_storage()->RecordEvent(notice_1(), kSettings);

  NoticeStorageData expected;
  expected.schema_version = 2;
  expected.chrome_version = version_info::GetVersionNumber();
  expected.notice_events = BuildEvents({{kShown, t0}, {kSettings, t1}});

  auto actual = notice_storage()->ReadNoticeData("Notice1StorageName");
  ASSERT_TRUE(actual.has_value());

  EXPECT_EQ(*actual, expected);

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.Notice1StorageName",
      NoticeActionTaken::kSettings, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.Notice1StorageName", kSettings, 1);

  t2 = AdvanceMs(50);

  notice_storage()->RecordEvent(notice_1(), kAck);
  actual = notice_storage()->ReadNoticeData("Notice1StorageName");
  ASSERT_TRUE(actual.has_value());

  expected.notice_events =
      BuildEvents({{kShown, t0}, {kSettings, t1}, {kAck, t2}});

  EXPECT_EQ(*actual, expected);

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.Notice1StorageName", kAck, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.Notice1StorageName",
      NoticeActionTaken::kAck, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeActionTakenBehavior."
      "Notice1StorageName",
      NoticeActionBehavior::kDuplicateActionTaken, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest,
       MultipleNoticeShownValuesRegisterSuccessfully) {
  base::Time t0 = base::Time::Now();
  notice_storage()->RecordEvent(notice_1(), kShown);
  base::Time t1 = AdvanceMs(100);
  notice_storage()->RecordEvent(notice_1(), kSettings);

  auto actual = notice_storage()->ReadNoticeData("Notice1StorageName");
  ASSERT_TRUE(actual.has_value());

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShownForFirstTime.Notice1StorageName", true,
      1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.Notice1StorageName", kSettings, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.Notice1StorageName",
      NoticeActionTaken::kSettings, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "Notice1StorageName_Settings",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "Notice1StorageName_Settings",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.Notice1StorageName", true, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.Notice1StorageName", kShown, 1);

  // Set notice shown value again.
  base::Time t2 = AdvanceMs(50);
  notice_storage()->RecordEvent(notice_1(), kShown);
  actual = notice_storage()->ReadNoticeData("Notice1StorageName");
  ASSERT_TRUE(actual.has_value());

  NoticeStorageData expected;
  expected.schema_version = 2;
  expected.chrome_version = version_info::GetVersionNumber();
  expected.notice_events =
      BuildEvents({{kShown, t0}, {kSettings, t1}, {kShown, t2}});

  EXPECT_EQ(*actual, expected);

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShownForFirstTime.Notice1StorageName", false,
      1);
}

TEST_F(PrivacySandboxNoticeStorageTest, SetMultipleNotices) {
  // Notice data 1.
  notice_storage()->RecordEvent(notice_1(), kShown);
  AdvanceMs(100);
  notice_storage()->RecordEvent(notice_1(), kSettings);
  const auto actual_notice1 =
      notice_storage()->ReadNoticeData("Notice1StorageName");
  ASSERT_TRUE(actual_notice1.has_value());

  // Notice data 2.
  notice_storage()->RecordEvent(notice_2(), kShown);
  AdvanceMs(20);
  notice_storage()->RecordEvent(notice_2(), kAck);
  const auto actual_notice2 =
      notice_storage()->ReadNoticeData("Notice2StorageName");
  ASSERT_TRUE(actual_notice2.has_value());

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.Notice1StorageName",
      NoticeActionTaken::kSettings, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.Notice1StorageName", kSettings, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "Notice1StorageName_Settings",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "Notice1StorageName_Settings",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.Notice1StorageName", kShown, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.Notice1StorageName", true, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.Notice2StorageName",
      NoticeActionTaken::kAck, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.Notice2StorageName", kAck, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "Notice2StorageName_"
      "Ack",
      base::Milliseconds(20), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "Notice2StorageName_Ack",
      base::Milliseconds(20), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.Notice2StorageName", true, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.Notice2StorageName", kShown, 1);
}

using NoticeEvents = base::span<const std::unique_ptr<EventTimePair>>;

class PrivacySandboxNoticeStorageV2Test
    : public PrivacySandboxNoticeStorageTest {};

TEST_F(PrivacySandboxNoticeStorageV2Test,
       AllEventsPopulatedMigrateSuccessfully) {
  SetNoticeStateFromJSON("Notice1StorageName", R"({
    "schema_version": 1,
    "notice_last_shown": "100",
    "notice_action_taken": 1,
    "notice_action_taken_time": "200"
    })");

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  auto notice_data = notice_storage()->ReadNoticeData("Notice1StorageName");
  ASSERT_TRUE(notice_data.has_value());
  EXPECT_EQ(notice_data->schema_version, 2);

  const NoticeEvents& events = notice_data->notice_events;
  EXPECT_THAT(events,
              ElementsAre(Pointee(Eq(EventTimePair{kShown, TimeFromMs(100)})),
                          Pointee(Eq(EventTimePair{kAck, TimeFromMs(200)}))));
}

TEST_F(PrivacySandboxNoticeStorageV2Test,
       NoticeShownPopulatedMigrateSuccessfully) {
  SetNoticeStateFromJSON("Notice1StorageName", R"({
    "schema_version": 1,
    "notice_last_shown": "500"
    })");

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  auto notice_data = notice_storage()->ReadNoticeData("Notice1StorageName");
  ASSERT_TRUE(notice_data.has_value());
  EXPECT_EQ(notice_data->schema_version, 2);

  EXPECT_THAT(notice_data->notice_events,
              ElementsAre(Pointee(Eq(EventTimePair{kShown, TimeFromMs(500)}))));
}

TEST_F(PrivacySandboxNoticeStorageV2Test, SchemaAlreadyUpToDateDoesNotMigrate) {
  SetNoticeStateFromJSON("Notice1StorageName", R"({"schema_version": 2})");

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());
  const NoticeEvents& events =
      notice_storage()->ReadNoticeData("Notice1StorageName")->notice_events;
  EXPECT_THAT(events, ElementsAre());
}

class PrivacySandboxNoticeStorageV2ActionsTest
    : public PrivacySandboxNoticeStorageTest,
      public testing::WithParamInterface<
          std::tuple<NoticeActionTaken, std::optional<Event>>> {};

TEST_P(PrivacySandboxNoticeStorageV2ActionsTest,
       NoticeActionWithoutShownPopulatedMigrateSuccessfully) {
  const auto& [action, notice_event] = GetParam();

  auto json = absl::StrFormat(R"({
                                "schema_version": 1,
                                "notice_action_taken": %d,
                                "notice_action_taken_time": "200"
                              })",
                              static_cast<int>(action));
  SetNoticeStateFromJSON("Notice1StorageName", std::move(json));

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  auto notice_data = notice_storage()->ReadNoticeData("Notice1StorageName");
  ASSERT_TRUE(notice_data.has_value());
  EXPECT_EQ(notice_data->schema_version, 2);

  const NoticeEvents& events = notice_data->notice_events;
  if (notice_event) {
    EXPECT_THAT(events, ElementsAre(Pointee(Eq(
                            EventTimePair{*notice_event, TimeFromMs(200)}))));
  } else {
    EXPECT_THAT(events, ElementsAre());
  }
}

TEST_P(PrivacySandboxNoticeStorageV2ActionsTest,
       NoticeActionPopulatedWithoutTimestampMigrateSuccessfully) {
  const auto& [action, notice_event] = GetParam();
  auto json =
      absl::StrFormat(R"({"schema_version": 1,"notice_action_taken": %d})",
                      static_cast<int>(action));
  SetNoticeStateFromJSON("Notice1StorageName", std::move(json));

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  auto notice_data = notice_storage()->ReadNoticeData("Notice1StorageName");
  ASSERT_TRUE(notice_data.has_value());
  EXPECT_EQ(notice_data->schema_version, 2);

  const NoticeEvents& events = notice_data->notice_events;
  if (notice_event) {
    EXPECT_THAT(
        events,
        ElementsAre(Pointee(Eq(EventTimePair{*notice_event, base::Time()}))));
  } else {
    EXPECT_THAT(events, ElementsAre());
  }
}

INSTANTIATE_TEST_SUITE_P(
    PrivacySandboxNoticeStorageV2ActionsTest,
    PrivacySandboxNoticeStorageV2ActionsTest,
    ValuesIn(std::vector<std::tuple<NoticeActionTaken, std::optional<Event>>>{
        {NoticeActionTaken::kNotSet, std::nullopt},
        {NoticeActionTaken::kAck, kAck},
        {NoticeActionTaken::kClosed, kClosed},
        {NoticeActionTaken::kLearnMore_Deprecated, std::nullopt},
        {NoticeActionTaken::kOptIn, kOptIn},
        {NoticeActionTaken::kOptOut, kOptOut},
        {NoticeActionTaken::kOther, std::nullopt},
        {NoticeActionTaken::kSettings, kSettings},
        {NoticeActionTaken::kUnknownActionPreMigration, std::nullopt},
        {NoticeActionTaken::kTimedOut, std::nullopt}}));

TEST_F(PrivacySandboxNoticeStorageV2Test,
       V1FieldsPresentSchemaV2_ErasesV1Fields) {
  SetNoticeStateFromJSON("Notice1StorageName", R"({
    "schema_version": 2,
    "notice_action_taken": 1,
    "notice_action_taken_time": "333333",
    "notice_last_shown": "222222",
    "events": [{"event": 5, "timestamp": "333333"}]
    })");

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  base::Value::Dict expected_stored_prefs;
  ParseDict(&expected_stored_prefs, R"({
    "schema_version": 2,
    "events": [{"event": 5, "timestamp": "333333"}]
    })");

  const base::Value::Dict* actual_stored_prefs =
      prefs()
          ->GetDict("privacy_sandbox.notices")
          .FindDict("Notice1StorageName");
  ASSERT_NE(nullptr, actual_stored_prefs);
  EXPECT_EQ(*actual_stored_prefs, expected_stored_prefs);
}

TEST_F(PrivacySandboxNoticeStorageV2Test,
       V1FieldsPresentAndDefaultWithSchemaV1_V1FieldsErased_MigratesToEmptyV2) {
  SetNoticeStateFromJSON("Notice1StorageName", R"({
    "schema_version": 1,
    "chrome_version": "1.2.3",
    "notice_action_taken": 0,    // NoticeActionTaken::kNotSet
    "notice_action_taken_time": "0",
    "notice_last_shown": "0"
    })");

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  base::Value::Dict expected_stored_prefs;
  ParseDict(&expected_stored_prefs,
            R"({"schema_version": 2, "chrome_version": "1.2.3"})");

  const base::Value::Dict* actual_stored_prefs =
      prefs()
          ->GetDict("privacy_sandbox.notices")
          .FindDict("Notice1StorageName");
  ASSERT_NE(nullptr, actual_stored_prefs);
  EXPECT_EQ(*actual_stored_prefs, expected_stored_prefs);
}

TEST_F(
    PrivacySandboxNoticeStorageV2Test,
    V1FieldsAllDefaultAndAbsentWithSchemaV1_NoFieldsErased_MigratesToEmptyV2) {
  SetNoticeStateFromJSON("Notice1StorageName", R"({"schema_version": 1})");

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  base::Value::Dict expected_stored_prefs;
  ParseDict(&expected_stored_prefs, R"({"schema_version": 2})");

  const base::Value::Dict* actual_stored_prefs =
      prefs()
          ->GetDict("privacy_sandbox.notices")
          .FindDict("Notice1StorageName");
  ASSERT_NE(nullptr, actual_stored_prefs);
  EXPECT_EQ(*actual_stored_prefs, expected_stored_prefs);
}

TEST_F(PrivacySandboxNoticeStorageV2Test, NoNoticeData_UpdateDoesNothing) {
  ASSERT_TRUE(prefs()->GetDict("privacy_sandbox.notices").empty());

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  EXPECT_TRUE(prefs()->GetDict("privacy_sandbox.notices").empty());
  const base::Value::Dict* actual_stored_prefs =
      prefs()
          ->GetDict("privacy_sandbox.notices")
          .FindDict("Notice1StorageName");
  EXPECT_EQ(nullptr, actual_stored_prefs);
}

TEST_F(PrivacySandboxNoticeStorageV2Test,
       NonDefaultV1FieldsPresentSchemaV1_ErasesV1Fields_Migrates) {
  SetNoticeStateFromJSON("Notice1StorageName", R"({
    "schema_version": 1,
    "notice_action_taken": 4,    // NoticeActionTaken::kOptIn
    "notice_last_shown": "100100",
    "notice_action_taken_time": "222222"
    })");

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  base::Value::Dict expected_stored_prefs;
  // V1 fields are erased. Events are migrated.
  ParseDict(&expected_stored_prefs, R"({
    "schema_version": 2,
    "events": [
      { "event": 5, "timestamp": "100100" }, // kShown event
      { "event": 2, "timestamp": "222222" }  // kOptIn event
    ]
    })");

  const base::Value::Dict* actual_stored_prefs =
      prefs()
          ->GetDict("privacy_sandbox.notices")
          .FindDict("Notice1StorageName");
  ASSERT_NE(nullptr, actual_stored_prefs);
  EXPECT_EQ(*actual_stored_prefs, expected_stored_prefs);
}

}  // namespace
}  // namespace privacy_sandbox
