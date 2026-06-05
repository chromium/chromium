// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"

#include <array>

#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/memory/raw_span.h"
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

void ParseDict(base::DictValue* dict, std::string&& json_string) {
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
    base::DictValue notice_data_dict;
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

struct StartupTestParam {
  base::raw_span<const Event> events;
  std::optional<Event> expected;
};

constexpr auto kStartupShownOnly = std::to_array<Event>({kShown});
constexpr auto kStartupClosed = std::to_array<Event>({kShown, kClosed});
constexpr auto kStartupOptIn =
    std::to_array<Event>({kShown, kSettings, kShown, kOptIn});
constexpr auto kStartupOptOut = std::to_array<Event>({kShown, kOptOut});
constexpr auto kStartupAck = std::to_array<Event>({kShown, kAck});
constexpr auto kStartupSettings = std::to_array<Event>({kShown, kSettings});

constexpr auto kStartupTestValues = std::to_array<StartupTestParam>({
    {.events = base::raw_span<const Event>(), .expected = std::nullopt},
    {.events = kStartupShownOnly, .expected = kShown},
    {.events = kStartupClosed, .expected = kClosed},
    {.events = kStartupOptIn, .expected = kOptIn},
    {.events = kStartupOptOut, .expected = kOptOut},
    {.events = kStartupAck, .expected = kAck},
    {.events = kStartupSettings, .expected = kSettings},
});

class PrivacySandboxNoticeStorageStartupTest
    : public PrivacySandboxNoticeStorageTest,
      public testing::WithParamInterface<StartupTestParam> {};

TEST_P(PrivacySandboxNoticeStorageStartupTest, StartupStateEmitsSuccessfully) {
  const auto& param = GetParam();
  for (auto event : param.events) {
    notice_storage()->RecordEvent(notice_1(), event);
    AdvanceMs(10);
  }

  notice_storage()->RecordStartupHistograms();
  if (param.expected) {
    histogram_tester_.ExpectBucketCount(
        "PrivacySandbox.Notice.Startup.LastRecordedEvent.Notice1StorageName",
        *param.expected, 1);
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

TEST_F(PrivacySandboxNoticeStorageTest, CleanupDeprecatedNotices) {
  // Since kDeprecatedNotices is empty by default, this verifies it doesn't
  // crash and doesn't remove active notices.
  SetNoticeStateFromJSON("Notice1StorageName", R"({"schema_version": 2})");

  notice_storage()->CleanupDeprecatedNotices();

  const base::DictValue* actual_stored_prefs =
      prefs()
          ->GetDict("privacy_sandbox.notices")
          .FindDict("Notice1StorageName");
  EXPECT_NE(nullptr, actual_stored_prefs);
}

}  // namespace
}  // namespace privacy_sandbox
