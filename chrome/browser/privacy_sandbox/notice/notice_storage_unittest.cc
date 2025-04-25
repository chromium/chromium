// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"

#include "base/json/values_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_catalog.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

using notice::mojom::PrivacySandboxNotice;
using notice::mojom::PrivacySandboxNoticeEvent;

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Pointee;

using enum notice::mojom::PrivacySandboxNoticeEvent;

using EventTimePair = NoticeEventTimestampPair;

// Feature providing the storage name for the default notice in the catalog.
BASE_FEATURE(kTestFeature1,
             "TopicsConsentDesktopModal",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeature2,
             "TopicsConsentModalClankCCT",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Notice ID for the default notice in the catalog.
constexpr NoticeId kNotice1InCatalog = {
    PrivacySandboxNotice::kTopicsConsentNotice, SurfaceType::kDesktopNewTab};
constexpr NoticeId kNotice2InCatalog = {
    PrivacySandboxNotice::kTopicsConsentNotice, SurfaceType::kClankCustomTab};

// A notice ID *not* expected in the default catalog.
constexpr NoticeId kNoticeIdNotInCatalog = {
    PrivacySandboxNotice::kMeasurementNotice, SurfaceType::kClankCustomTab};

base::Time UnixMs(int64_t ms) {
  return base::Time::FromMillisecondsSinceUnixEpoch(ms);
}

// TODO(crbug.com/333406690): Make a test notice name list injectable so tests
// don't have to use actual notice names.
class PrivacySandboxNoticeStorageTest : public testing::Test {
 public:
  PrivacySandboxNoticeStorageTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    PrivacySandboxNoticeStorage::RegisterProfilePrefs(prefs()->registry());
    catalog_ = std::make_unique<MockNoticeCatalog>();
    notice_storage_ =
        std::make_unique<PrivacySandboxNoticeStorage>(prefs(), catalog_.get());
    scoped_feature_list_.InitAndEnableFeature(
        kPrivacySandboxMigratePrefsToSchemaV2);

    default_notice_map_ = BuildDefaultNoticeMap();
    ON_CALL(*mock_catalog(), GetNoticeMap())
        .WillByDefault(testing::ReturnRef(default_notice_map_));
  }

  PrivacySandboxNoticeStorage* notice_storage() {
    return notice_storage_.get();
  }

  MockNoticeCatalog* mock_catalog() { return catalog_.get(); }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 protected:
  virtual NoticeMap BuildDefaultNoticeMap() {
    NoticeMap map;

    std::unique_ptr<Notice> notice_1 =
        std::make_unique<Consent>(kNotice1InCatalog);
    notice_1->SetFeature(&kTestFeature1);
    map.emplace(kNotice1InCatalog, std::move(notice_1));

    std::unique_ptr<Notice> notice_2 =
        std::make_unique<Consent>(kNotice2InCatalog);
    notice_2->SetFeature(&kTestFeature2);
    map.emplace(kNotice2InCatalog, std::move(notice_2));

    return map;
  }

  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_env_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<MockNoticeCatalog> catalog_;
  std::unique_ptr<PrivacySandboxNoticeStorage> notice_storage_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  NoticeMap default_notice_map_;
};

TEST_F(PrivacySandboxNoticeStorageTest, NoticePathNotFound) {
  const auto actual =
      notice_storage()->ReadNoticeData("TopicsConsentDesktopModal");
  EXPECT_FALSE(actual.has_value());
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateDoesNotExist) {
  notice_storage()->RecordStartupHistograms();
  const std::string histograms = histogram_tester_.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms, testing::Not(testing::AnyOf(
                              "PrivacySandbox.Notice.NoticeStartupState."
                              "TopicsConsentDesktopModal")));
  EXPECT_THAT(histograms, testing::Not(testing::AnyOf(
                              "PrivacySandbox.Notice.NoticeStartupState2."
                              "TopicsConsentDesktopModal")));
}

TEST_F(PrivacySandboxNoticeStorageTest, NoNoticeNameExpectCrash) {
  EXPECT_DEATH(notice_storage()->RecordEvent(kNoticeIdNotInCatalog, kShown),
               "");
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateEmitsPromptWaiting) {
  notice_storage()->RecordEvent(kNotice1InCatalog, kShown);

  notice_storage()->RecordStartupHistograms();
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.TopicsConsentDesktopModal",
      NoticeStartupState::kPromptWaiting, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState2.TopicsConsentDesktopModal",
      NoticeStartupState::kPromptWaiting, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateEmitsUnknownState) {
  // Migrate actions without shown.
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath("TopicsConsentDesktopModal.schema_version", 1);
  update.Get().SetByDottedPath("TopicsConsentDesktopModal.notice_action_taken",
                               static_cast<int>(NoticeActionTaken::kAck));
  update.Get().SetByDottedPath(
      "TopicsConsentDesktopModal.notice_action_taken_time",
      base::TimeToValue(UnixMs(200)));
  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  notice_storage()->RecordStartupHistograms();
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.TopicsConsentDesktopModal",
      NoticeStartupState::kUnknownState, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState2.TopicsConsentDesktopModal",
      NoticeStartupState::kUnknownState, 1);
}

const auto kStartupTestValues = std::vector<
    std::tuple<std::vector<PrivacySandboxNoticeEvent>, NoticeStartupState>>{
    {{kShown, kClosed}, NoticeStartupState::kFlowCompleted},
    {{kShown, kSettings, kShown, kOptIn},
     NoticeStartupState::kFlowCompletedWithOptIn},
    {{kShown, kOptOut}, NoticeStartupState::kFlowCompletedWithOptOut},
    {{kShown, kAck}, NoticeStartupState::kFlowCompleted},
    {{kShown, kClosed, kShown}, NoticeStartupState::kPromptWaiting}};

class PrivacySandboxNoticeStorageStartupTest
    : public PrivacySandboxNoticeStorageTest,
      public testing::WithParamInterface<
          std::tuple<std::vector<PrivacySandboxNoticeEvent>,
                     NoticeStartupState>> {};

TEST_P(PrivacySandboxNoticeStorageStartupTest, StartupStateEmitsSuccessfully) {
  for (auto event : std::get<0>(GetParam())) {
    notice_storage()->RecordEvent(kNotice1InCatalog, event);
    task_env_.AdvanceClock(base::Milliseconds(10));
  }

  notice_storage()->RecordStartupHistograms();
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.TopicsConsentDesktopModal",
      std::get<1>(GetParam()), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState2.TopicsConsentDesktopModal",
      std::get<1>(GetParam()), 1);
}

INSTANTIATE_TEST_SUITE_P(PrivacySandboxNoticeStorageStartupTest,
                         PrivacySandboxNoticeStorageStartupTest,
                         testing::ValuesIn(kStartupTestValues));

TEST_F(PrivacySandboxNoticeStorageTest, SetsValuesAndReadsData) {
  base::Time t0 = base::Time::Now();
  notice_storage()->RecordEvent(kNotice1InCatalog, kShown);
  task_env_.AdvanceClock(base::Milliseconds(100));
  base::Time t1 = base::Time::Now();
  notice_storage()->RecordEvent(kNotice1InCatalog, kAck);

  const auto actual =
      notice_storage()->ReadNoticeData("TopicsConsentDesktopModal");
  ASSERT_TRUE(actual.has_value());

  EXPECT_THAT(actual->notice_events,
              ElementsAre(Pointee(Eq(EventTimePair{kShown, t0})),
                          Pointee(Eq(EventTimePair{kAck, t1}))));

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentDesktopModal", kAck, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kAck, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "TopicsConsentDesktopModal_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "TopicsConsentDesktopModal_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.TopicsConsentDesktopModal", true, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentDesktopModal", kShown, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest,
       ReActionDoesNotRegisterAndEmitsHistogram) {
  notice_storage()->RecordEvent(kNotice1InCatalog, kShown);
  task_env_.AdvanceClock(base::Milliseconds(100));
  base::Time t1 = base::Time::Now();
  notice_storage()->RecordEvent(kNotice1InCatalog, kSettings);

  auto actual = notice_storage()->ReadNoticeData("TopicsConsentDesktopModal");
  ASSERT_TRUE(actual.has_value());

  EXPECT_THAT(
      actual->notice_events,
      ElementsAre(
          Pointee(Eq(EventTimePair{kShown, t1 - base::Milliseconds(100)})),
          Pointee(Eq(EventTimePair{kSettings, t1}))));

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kSettings, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentDesktopModal", kSettings,
      1);

  // Tries to override action, should not override and emits histograms.
  task_env_.AdvanceClock(base::Milliseconds(50));
  notice_storage()->RecordEvent(kNotice1InCatalog, kAck);
  actual = notice_storage()->ReadNoticeData(
      "TopicsConsentDesktopModal");  // Re-read data after potential change
  ASSERT_TRUE(actual.has_value());

  EXPECT_THAT(
      actual->notice_events,
      ElementsAre(
          Pointee(Eq(EventTimePair{kShown, t1 - base::Milliseconds(100)})),
          Pointee(Eq(EventTimePair{kSettings, t1}))));

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentDesktopModal", kAck, 0);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kAck, 0);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kSettings, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeActionTakenBehavior."
      "TopicsConsentDesktopModal",
      NoticeActionBehavior::kDuplicateActionTaken, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest,
       MultipleNoticeShownValuesRegisterSuccessfully) {
  base::Time t0 = base::Time::Now();
  notice_storage()->RecordEvent(kNotice1InCatalog, kShown);
  task_env_.AdvanceClock(base::Milliseconds(100));
  notice_storage()->RecordEvent(kNotice1InCatalog, kSettings);

  auto actual = notice_storage()->ReadNoticeData("TopicsConsentDesktopModal");
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(t0, GetNoticeFirstShownFromEvents(*actual));
  EXPECT_EQ(t0, GetNoticeLastShownFromEvents(*actual));

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShownForFirstTime.TopicsConsentDesktopModal",
      true, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentDesktopModal", kSettings,
      1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kSettings, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "TopicsConsentDesktopModal_Settings",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "TopicsConsentDesktopModal_Settings",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.TopicsConsentDesktopModal", true, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentDesktopModal", kShown, 1);

  // Set notice shown value again.
  task_env_.AdvanceClock(base::Milliseconds(50));
  base::Time t1 = base::Time::Now();
  notice_storage()->RecordEvent(kNotice1InCatalog, kShown);
  actual = notice_storage()->ReadNoticeData("TopicsConsentDesktopModal");
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(t1, GetNoticeLastShownFromEvents(*actual));
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShownForFirstTime.TopicsConsentDesktopModal",
      false, 1);
  EXPECT_EQ(t0, GetNoticeFirstShownFromEvents(*actual));
}

TEST_F(PrivacySandboxNoticeStorageTest, SetMultipleNotices) {
  // Notice data 1.
  notice_storage()->RecordEvent(kNotice1InCatalog, kShown);
  task_env_.AdvanceClock(base::Milliseconds(100));
  notice_storage()->RecordEvent(kNotice1InCatalog, kSettings);
  const auto actual_notice1 =
      notice_storage()->ReadNoticeData("TopicsConsentDesktopModal");
  ASSERT_TRUE(actual_notice1.has_value());

  // Notice data 2.
  notice_storage()->RecordEvent(kNotice2InCatalog, kShown);
  task_env_.AdvanceClock(base::Milliseconds(20));
  notice_storage()->RecordEvent(kNotice2InCatalog, kAck);
  const auto actual_notice2 =
      notice_storage()->ReadNoticeData("TopicsConsentModalClankCCT");
  ASSERT_TRUE(actual_notice2.has_value());

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kSettings, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentDesktopModal", kSettings,
      1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "TopicsConsentDesktopModal_Settings",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "TopicsConsentDesktopModal_Settings",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentDesktopModal", kShown, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.TopicsConsentDesktopModal", true, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentModalClankCCT",
      NoticeActionTaken::kAck, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentModalClankCCT", kAck, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "TopicsConsentModalClankCCT_"
      "Ack",
      base::Milliseconds(20), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "TopicsConsentModalClankCCT_Ack",
      base::Milliseconds(20), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.TopicsConsentModalClankCCT", true, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentModalClankCCT", kShown,
      1);
}

using NoticeEvents = base::span<const std::unique_ptr<EventTimePair>>;

class PrivacySandboxNoticeStorageV2Test
    : public PrivacySandboxNoticeStorageTest {};

TEST_F(PrivacySandboxNoticeStorageV2Test,
       PrivacySandboxMigratePrefsToSchemaV2FlagDisabledDoesNotMigrate) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.Reset();
  scoped_feature_list.InitAndDisableFeature(
      kPrivacySandboxMigratePrefsToSchemaV2);
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath("TopicsConsentDesktopModal.schema_version", 1);
  update.Get().SetByDottedPath("TopicsConsentDesktopModal.notice_last_shown",
                               base::TimeToValue(UnixMs(100)));
  update.Get().SetByDottedPath("TopicsConsentDesktopModal.notice_action_taken",
                               static_cast<int>(NoticeActionTaken::kAck));
  update.Get().SetByDottedPath(
      "TopicsConsentDesktopModal.notice_action_taken_time",
      base::TimeToValue(UnixMs(200)));

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  auto notice_data =
      notice_storage()->ReadNoticeData("TopicsConsentDesktopModal");
  ASSERT_TRUE(notice_data.has_value());
  EXPECT_EQ(notice_data->schema_version, 1);
  EXPECT_THAT(notice_data->notice_events, ElementsAre());
}

TEST_F(PrivacySandboxNoticeStorageV2Test,
       AllEventsPopulatedMigrateSuccessfully) {
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath("TopicsConsentDesktopModal.schema_version", 1);
  update.Get().SetByDottedPath("TopicsConsentDesktopModal.notice_last_shown",
                               base::TimeToValue(UnixMs(100)));
  update.Get().SetByDottedPath("TopicsConsentDesktopModal.notice_action_taken",
                               static_cast<int>(NoticeActionTaken::kAck));
  update.Get().SetByDottedPath(
      "TopicsConsentDesktopModal.notice_action_taken_time",
      base::TimeToValue(UnixMs(200)));

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  auto notice_data =
      notice_storage()->ReadNoticeData("TopicsConsentDesktopModal");
  ASSERT_TRUE(notice_data.has_value());
  EXPECT_EQ(notice_data->schema_version, 2);

  const NoticeEvents& events = notice_data->notice_events;
  EXPECT_THAT(events,
              ElementsAre(Pointee(Eq(EventTimePair{kShown, UnixMs(100)})),
                          Pointee(Eq(EventTimePair{kAck, UnixMs(200)}))));
}

TEST_F(PrivacySandboxNoticeStorageV2Test,
       NoticeShownPopulatedMigrateSuccessfully) {
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath("TopicsConsentDesktopModal.schema_version", 1);
  update.Get().SetByDottedPath("TopicsConsentDesktopModal.notice_last_shown",
                               base::TimeToValue(UnixMs(500)));

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  auto notice_data =
      notice_storage()->ReadNoticeData("TopicsConsentDesktopModal");
  ASSERT_TRUE(notice_data.has_value());
  EXPECT_EQ(notice_data->schema_version, 2);

  EXPECT_THAT(notice_data->notice_events,
              ElementsAre(Pointee(Eq(EventTimePair{kShown, UnixMs(500)}))));
}

TEST_F(PrivacySandboxNoticeStorageV2Test, SchemaAlreadyUpToDateDoesNotMigrate) {
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath("TopicsConsentDesktopModal.schema_version", 2);

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());
  const NoticeEvents& events = notice_storage()
                                   ->ReadNoticeData("TopicsConsentDesktopModal")
                                   ->notice_events;
  EXPECT_THAT(events, ElementsAre());
}

class PrivacySandboxNoticeStorageV2ActionsTest
    : public PrivacySandboxNoticeStorageTest,
      public testing::WithParamInterface<
          std::tuple<NoticeActionTaken,
                     std::optional<PrivacySandboxNoticeEvent>>> {};

TEST_P(PrivacySandboxNoticeStorageV2ActionsTest,
       NoticeActionWithoutShownPopulatedMigrateSuccessfully) {
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath("TopicsConsentDesktopModal.schema_version", 1);
  update.Get().SetByDottedPath("TopicsConsentDesktopModal.notice_action_taken",
                               static_cast<int>(std::get<0>(GetParam())));
  update.Get().SetByDottedPath(
      "TopicsConsentDesktopModal.notice_action_taken_time",
      base::TimeToValue(UnixMs(200)));
  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  auto notice_data =
      notice_storage()->ReadNoticeData("TopicsConsentDesktopModal");
  ASSERT_TRUE(notice_data.has_value());
  EXPECT_EQ(notice_data->schema_version, 2);

  const NoticeEvents& events = notice_data->notice_events;
  auto notice_event = std::get<1>(GetParam());
  if (notice_event) {
    EXPECT_THAT(
        events,
        ElementsAre(Pointee(Eq(EventTimePair{*notice_event, UnixMs(200)}))));
  } else {
    EXPECT_THAT(events, ElementsAre());
  }
}

TEST_P(PrivacySandboxNoticeStorageV2ActionsTest,
       NoticeActionPopulatedWithoutTimestampMigrateSuccessfully) {
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath("TopicsConsentDesktopModal.schema_version", 1);
  update.Get().SetByDottedPath("TopicsConsentDesktopModal.notice_action_taken",
                               static_cast<int>(std::get<0>(GetParam())));

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  auto notice_data =
      notice_storage()->ReadNoticeData("TopicsConsentDesktopModal");
  ASSERT_TRUE(notice_data.has_value());
  EXPECT_EQ(notice_data->schema_version, 2);

  const NoticeEvents& events = notice_data->notice_events;
  auto notice_event = std::get<1>(GetParam());
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
    testing::ValuesIn(
        std::vector<std::tuple<NoticeActionTaken,
                               std::optional<PrivacySandboxNoticeEvent>>>{
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

std::vector<std::unique_ptr<EventTimePair>> BuildEvents(
    std::initializer_list<std::pair<PrivacySandboxNoticeEvent, int64_t>>
        raw_events) {
  std::vector<std::unique_ptr<EventTimePair>> events;
  events.reserve(raw_events.size());
  for (const auto& raw_event : raw_events) {
    events.emplace_back(std::make_unique<EventTimePair>(
        EventTimePair{raw_event.first, UnixMs(raw_event.second)}));
  }
  return events;
}

class PrivacySandboxNoticeDataTest : public testing::Test {};

TEST_F(PrivacySandboxNoticeDataTest, NoPrivacySandboxNoticeDataReturnsNothing) {
  NoticeStorageData data;
  EXPECT_EQ(GetNoticeFirstShownFromEvents(data), std::nullopt);
  EXPECT_EQ(GetNoticeLastShownFromEvents(data), std::nullopt);
  EXPECT_EQ(GetNoticeActionTakenForFirstShownFromEvents(data), std::nullopt);
}

TEST_F(PrivacySandboxNoticeDataTest,
       NoticeShownEvent_AccessorReturnsFirstShownSuccessfully) {
  NoticeStorageData data;
  data.notice_events = BuildEvents({
      {kShown, 100},
      {kAck, 150},
      {kShown, 200},
  });

  EXPECT_EQ(GetNoticeFirstShownFromEvents(data), UnixMs(100));
}

TEST_F(PrivacySandboxNoticeDataTest,
       NoticeShownEvent_AccessorReturnsLastShownSuccessfully) {
  NoticeStorageData data;
  data.notice_events = BuildEvents({
      {kShown, 100},
      {kAck, 150},
      {kShown, 200},
  });

  EXPECT_EQ(GetNoticeLastShownFromEvents(data), UnixMs(200));
}

TEST_F(PrivacySandboxNoticeDataTest,
       NoNoticeActionTakenEvent_AccessorReturnsNoValue) {
  NoticeStorageData data;
  data.notice_events = BuildEvents({
      {kShown, 100},
      {kShown, 200},
  });

  EXPECT_EQ(GetNoticeActionTakenForFirstShownFromEvents(data), std::nullopt);
}

TEST_F(PrivacySandboxNoticeDataTest,
       NoticeActionTakenEvent_AccessorReturnsActionSuccessfully) {
  NoticeStorageData data;
  data.notice_events = BuildEvents({
      {kShown, 100},
      {kAck, 120},
      {kShown, 200},
      {kOptIn, 250},
  });

  EXPECT_EQ(GetNoticeActionTakenForFirstShownFromEvents(data),
            (EventTimePair{kAck, UnixMs(120)}));
}

TEST_F(
    PrivacySandboxNoticeDataTest,
    NoticeActionTakenEvent_AccessorReturnsActionSuccessfullyMultipleActions) {
  NoticeStorageData data;
  data.notice_events = BuildEvents({
      {kShown, 100},
      {kAck, 120},
      {kSettings, 150},
      {kShown, 200},
      {kOptIn, 250},
  });

  EXPECT_EQ(GetNoticeActionTakenForFirstShownFromEvents(data),
            (EventTimePair{kSettings, UnixMs(150)}));
}

TEST_F(
    PrivacySandboxNoticeDataTest,
    NoticeActionTakenEvent_AccessorReturnsActionSuccessfullyWithMultipleShownValues) {
  NoticeStorageData data;
  data.notice_events = BuildEvents({
      {kShown, 100},
      {kShown, 110},
      {kAck, 120},
      {kSettings, 150},
      {kShown, 200},
      {kShown, 220},
      {kOptIn, 250},
  });

  EXPECT_EQ(GetNoticeActionTakenForFirstShownFromEvents(data),
            (EventTimePair{kSettings, UnixMs(150)}));
}

}  // namespace
}  // namespace privacy_sandbox
