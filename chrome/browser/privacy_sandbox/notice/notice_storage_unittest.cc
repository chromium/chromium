// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"

#include "base/json/values_util.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/histogram_variants_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_catalog.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/privacy_sandbox/notice/notice_constants.h"
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

using enum notice::mojom::PrivacySandboxNoticeEvent;

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

TEST_F(PrivacySandboxNoticeStorageTest, CheckPSNoticeHistograms) {
  std::optional<base::HistogramVariantsEntryMap> notices;
  std::vector<std::string> missing_notices;
  {
    notices = base::ReadVariantsFromHistogramsXml("PSNotice", "privacy");
    ASSERT_TRUE(notices.has_value());
  }
  EXPECT_EQ(std::size(kPrivacySandboxNoticeNames), notices->size());
  for (const auto& name : kPrivacySandboxNoticeNames) {
    // TODO(crbug.com/333406690): Implement something to clean up notices that
    // don't exist.
    if (!base::Contains(*notices, std::string(name))) {
      missing_notices.emplace_back(name);
    }
  }
  ASSERT_TRUE(missing_notices.empty())
      << "Notices:\n"
      << base::JoinString(missing_notices, ", ")
      << "\nconfigured in notice_constants.h but no "
         "corresponding variants were added to PSNotice variants in "
         "//tools/metrics/histograms/metadata/privacy/histograms.xml";
}

TEST_F(PrivacySandboxNoticeStorageTest, CheckPSNoticeActionHistograms) {
  std::optional<base::HistogramVariantsEntryMap> actions;
  std::vector<std::string> missing_actions;
  {
    actions = base::ReadVariantsFromHistogramsXml("PSNoticeAction", "privacy");
    ASSERT_TRUE(actions.has_value());
  }

  for (int i = static_cast<int>(kMinValue); i <= static_cast<int>(kMaxValue);
       ++i) {
    std::string notice_name =
        PrivacySandboxNoticeStorage::GetNoticeActionStringFromEvent(
            static_cast<PrivacySandboxNoticeEvent>(i));
    if (!notice_name.empty() && !base::Contains(*actions, notice_name)) {
      missing_actions.emplace_back(notice_name);
    }
  }
  ASSERT_TRUE(missing_actions.empty())
      << "Actions:\n"
      << base::JoinString(missing_actions, ", ")
      << "\nconfigured in privacy_sandbox_notice_storage.cc but no "
         "corresponding variants were added to PSNoticeAction variants in "
         "//tools/metrics/histograms/metadata/privacy/histograms.xml";
}

TEST_F(PrivacySandboxNoticeStorageTest, NoticePathNotFound) {
  const auto actual = notice_storage()->ReadNoticeData(kTopicsConsentModal);
  EXPECT_FALSE(actual.has_value());
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateDoesNotExist) {
  notice_storage()->RecordHistogramsOnStartup(kTopicsConsentModal);
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

  notice_storage()->RecordHistogramsOnStartup(kTopicsConsentModal);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.TopicsConsentDesktopModal",
      NoticeStartupState::kPromptWaiting, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState2.TopicsConsentDesktopModal",
      NoticeStartupState::kPromptWaiting, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateEmitsUnknownState) {
  // Migrate actions without shown.
  std::string notice = kTopicsConsentModal;
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath(base::StrCat({notice, ".", "schema_version"}),
                               1);
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken"}),
      static_cast<int>(NoticeActionTaken::kAck));
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken_time"}),
      base::TimeToValue(base::Time::FromMillisecondsSinceUnixEpoch(200)));
  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  notice_storage()->RecordHistogramsOnStartup(notice);
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

  notice_storage()->RecordHistogramsOnStartup(kTopicsConsentModal);
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

  const auto actual = notice_storage()->ReadNoticeData(kTopicsConsentModal);
  ASSERT_TRUE(actual.has_value());

  ASSERT_EQ(actual->GetNoticeEvents().size(), 2u);
  EXPECT_EQ(*(actual->GetNoticeEvents()[0]),
            (NoticeEventTimestampPair{kShown, t0}));
  EXPECT_EQ(*(actual->GetNoticeEvents()[1]),
            (NoticeEventTimestampPair{kAck, t1}));

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

  auto actual = notice_storage()->ReadNoticeData(kTopicsConsentModal);
  ASSERT_TRUE(actual.has_value());

  ASSERT_EQ(actual->GetNoticeEvents().size(), 2u);
  EXPECT_EQ(*(actual->GetNoticeEvents()[1]),
            (NoticeEventTimestampPair{kSettings, t1}));

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
      kTopicsConsentModal);  // Re-read data after potential change
  ASSERT_TRUE(actual.has_value());

  EXPECT_EQ(actual->GetNoticeEvents().size(), 2u);
  EXPECT_EQ(*(actual->GetNoticeEvents()[1]),
            (NoticeEventTimestampPair{kSettings, t1}));

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

  auto actual = notice_storage()->ReadNoticeData(kTopicsConsentModal);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(t0, actual->GetNoticeFirstShownFromEvents());
  EXPECT_EQ(t0, actual->GetNoticeLastShownFromEvents());

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
  actual = notice_storage()->ReadNoticeData(kTopicsConsentModal);
  ASSERT_TRUE(actual.has_value());
  EXPECT_EQ(t1, actual->GetNoticeLastShownFromEvents());
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShownForFirstTime.TopicsConsentDesktopModal",
      false, 1);
  EXPECT_EQ(t0, actual->GetNoticeFirstShownFromEvents());
}

TEST_F(PrivacySandboxNoticeStorageTest, SetMultipleNotices) {
  // Notice data 1.
  notice_storage()->RecordEvent(kNotice1InCatalog, kShown);
  task_env_.AdvanceClock(base::Milliseconds(100));
  notice_storage()->RecordEvent(kNotice1InCatalog, kSettings);
  const auto actual_notice1 =
      notice_storage()->ReadNoticeData(kTopicsConsentModal);
  ASSERT_TRUE(actual_notice1.has_value());

  // Notice data 2.
  notice_storage()->RecordEvent(kNotice2InCatalog, kShown);
  task_env_.AdvanceClock(base::Milliseconds(20));
  notice_storage()->RecordEvent(kNotice2InCatalog, kAck);
  const auto actual_notice2 =
      notice_storage()->ReadNoticeData(kTopicsConsentModalClankCCT);
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

using NoticeEvents =
    base::span<const std::unique_ptr<NoticeEventTimestampPair>>;

class PrivacySandboxNoticeStorageV2Test
    : public PrivacySandboxNoticeStorageTest {};

TEST_F(PrivacySandboxNoticeStorageV2Test,
       PrivacySandboxMigratePrefsToSchemaV2FlagDisabledDoesNotMigrate) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.Reset();
  scoped_feature_list.InitAndDisableFeature(
      kPrivacySandboxMigratePrefsToSchemaV2);
  std::string notice = kTopicsConsentModal;
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath(base::StrCat({notice, ".", "schema_version"}),
                               1);
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_last_shown"}),
      base::TimeToValue(base::Time::FromMillisecondsSinceUnixEpoch(100)));
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken"}),
      static_cast<int>(NoticeActionTaken::kAck));
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken_time"}),
      base::TimeToValue(base::Time::FromMillisecondsSinceUnixEpoch(200)));

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  auto notice_data = notice_storage()->ReadNoticeData(notice);
  ASSERT_TRUE(notice_data.has_value());
  EXPECT_EQ(notice_data->GetSchemaVersion(), 1);
  NoticeEvents events = notice_data->GetNoticeEvents();

  EXPECT_EQ(events.size(), 0u);
}

TEST_F(PrivacySandboxNoticeStorageV2Test,
       AllEventsPopulatedMigrateSuccessfully) {
  std::string notice = kTopicsConsentModal;
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath(base::StrCat({notice, ".", "schema_version"}),
                               1);
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_last_shown"}),
      base::TimeToValue(base::Time::FromMillisecondsSinceUnixEpoch(100)));
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken"}),
      static_cast<int>(NoticeActionTaken::kAck));
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken_time"}),
      base::TimeToValue(base::Time::FromMillisecondsSinceUnixEpoch(200)));

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  auto notice_data = notice_storage()->ReadNoticeData(notice);
  ASSERT_TRUE(notice_data.has_value());
  EXPECT_EQ(notice_data->GetSchemaVersion(), 2);

  NoticeEvents events = notice_data->GetNoticeEvents();
  EXPECT_EQ(events.size(), 2u);

  EXPECT_EQ(*(events[0]),
            (NoticeEventTimestampPair{
                kShown, base::Time::FromMillisecondsSinceUnixEpoch(100)}));
  EXPECT_EQ(*(events[1]),
            (NoticeEventTimestampPair{
                kAck, base::Time::FromMillisecondsSinceUnixEpoch(200)}));
}

TEST_F(PrivacySandboxNoticeStorageV2Test,
       NoticeShownPopulatedMigrateSuccessfully) {
  std::string notice = kTopicsConsentModal;
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath(base::StrCat({notice, ".", "schema_version"}),
                               1);
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_last_shown"}),
      base::TimeToValue(base::Time::FromMillisecondsSinceUnixEpoch(500)));

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  auto notice_data = notice_storage()->ReadNoticeData(notice);
  ASSERT_TRUE(notice_data.has_value());
  EXPECT_EQ(notice_data->GetSchemaVersion(), 2);

  auto events = notice_data->GetNoticeEvents();
  EXPECT_EQ(events.size(), 1u);
  EXPECT_EQ(*(events[0]),
            (NoticeEventTimestampPair{
                kShown, base::Time::FromMillisecondsSinceUnixEpoch(500)}));
}

TEST_F(PrivacySandboxNoticeStorageV2Test, SchemaAlreadyUpToDateDoesNotMigrate) {
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath(
      base::StrCat({kTopicsConsentModal, ".schema_version"}), 2);

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());
  NoticeEvents events =
      notice_storage()->ReadNoticeData(kTopicsConsentModal)->GetNoticeEvents();
  EXPECT_EQ(events.size(), 0u);
}

class PrivacySandboxNoticeStorageV2ActionsTest
    : public PrivacySandboxNoticeStorageTest,
      public testing::WithParamInterface<
          std::tuple<NoticeActionTaken,
                     std::optional<PrivacySandboxNoticeEvent>>> {};

TEST_P(PrivacySandboxNoticeStorageV2ActionsTest,
       NoticeActionWithoutShownPopulatedMigrateSuccessfully) {
  std::string notice = kTopicsConsentModal;
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath(base::StrCat({notice, ".", "schema_version"}),
                               1);
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken"}),
      static_cast<int>(std::get<0>(GetParam())));
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken_time"}),
      base::TimeToValue(base::Time::FromMillisecondsSinceUnixEpoch(200)));
  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  auto notice_data = notice_storage()->ReadNoticeData(notice);
  ASSERT_TRUE(notice_data.has_value());
  EXPECT_EQ(notice_data->GetSchemaVersion(), 2);

  NoticeEvents events = notice_data->GetNoticeEvents();
  auto notice_event = std::get<1>(GetParam());
  if (notice_event) {
    EXPECT_EQ(events.size(), 1u);
    EXPECT_EQ(
        *(events[0]),
        (NoticeEventTimestampPair{
            *notice_event, base::Time::FromMillisecondsSinceUnixEpoch(200)}));
  } else {
    EXPECT_EQ(events.size(), 0u);
  }
}

TEST_P(PrivacySandboxNoticeStorageV2ActionsTest,
       NoticeActionPopulatedWithoutTimestampMigrateSuccessfully) {
  std::string notice = kTopicsConsentModal;
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath(base::StrCat({notice, ".", "schema_version"}),
                               1);
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken"}),
      static_cast<int>(std::get<0>(GetParam())));

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  auto notice_data = notice_storage()->ReadNoticeData(notice);
  ASSERT_TRUE(notice_data.has_value());
  EXPECT_EQ(notice_data->GetSchemaVersion(), 2);

  NoticeEvents events = notice_data->GetNoticeEvents();
  auto notice_event = std::get<1>(GetParam());
  if (notice_event) {
    EXPECT_EQ(events.size(), 1u);
    EXPECT_EQ(*(events[0]),
              (NoticeEventTimestampPair{*notice_event, base::Time()}));
  } else {
    EXPECT_EQ(events.size(), 0u);
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

std::vector<std::unique_ptr<NoticeEventTimestampPair>> BuildEvents(
    std::initializer_list<std::pair<PrivacySandboxNoticeEvent, int64_t>>
        raw_events) {
  std::vector<std::unique_ptr<NoticeEventTimestampPair>> events;
  events.reserve(raw_events.size());
  for (const auto& raw_event : raw_events) {
    events.emplace_back(
        std::make_unique<NoticeEventTimestampPair>(NoticeEventTimestampPair{
            raw_event.first,
            base::Time::FromMillisecondsSinceUnixEpoch(raw_event.second)}));
  }
  return events;
}

class PrivacySandboxNoticeDataTest : public testing::Test {};

TEST_F(PrivacySandboxNoticeDataTest, NoPrivacySandboxNoticeDataReturnsNothing) {
  PrivacySandboxNoticeData data;
  EXPECT_EQ(data.GetNoticeFirstShownFromEvents(), std::nullopt);
  EXPECT_EQ(data.GetNoticeLastShownFromEvents(), std::nullopt);
  EXPECT_EQ(data.GetNoticeActionTakenForFirstShownFromEvents(), std::nullopt);
}

TEST_F(PrivacySandboxNoticeDataTest,
       NoticeShownEvent_AccessorReturnsFirstShownSuccessfully) {
  PrivacySandboxNoticeData data;
  data.SetNoticeEvents(BuildEvents({
      {kShown, 100},
      {kAck, 150},
      {kShown, 200},
  }));

  EXPECT_EQ(data.GetNoticeFirstShownFromEvents(),
            base::Time::FromMillisecondsSinceUnixEpoch(100));
}

TEST_F(PrivacySandboxNoticeDataTest,
       NoticeShownEvent_AccessorReturnsLastShownSuccessfully) {
  PrivacySandboxNoticeData data;
  data.SetNoticeEvents(BuildEvents({
      {kShown, 100},
      {kAck, 150},
      {kShown, 200},
  }));

  EXPECT_EQ(data.GetNoticeLastShownFromEvents(),
            base::Time::FromMillisecondsSinceUnixEpoch(200));
}

TEST_F(PrivacySandboxNoticeDataTest,
       NoNoticeActionTakenEvent_AccessorReturnsNoValue) {
  PrivacySandboxNoticeData data;
  data.SetNoticeEvents(BuildEvents({
      {kShown, 100},
      {kShown, 200},
  }));

  EXPECT_EQ(data.GetNoticeActionTakenForFirstShownFromEvents(), std::nullopt);
}

TEST_F(PrivacySandboxNoticeDataTest,
       NoticeActionTakenEvent_AccessorReturnsActionSuccessfully) {
  PrivacySandboxNoticeData data;
  data.SetNoticeEvents(BuildEvents({
      {kShown, 100},
      {kAck, 120},
      {kShown, 200},
      {kOptIn, 250},
  }));

  EXPECT_EQ(data.GetNoticeActionTakenForFirstShownFromEvents(),
            (NoticeEventTimestampPair{
                kAck, base::Time::FromMillisecondsSinceUnixEpoch(120)}));
}

TEST_F(
    PrivacySandboxNoticeDataTest,
    NoticeActionTakenEvent_AccessorReturnsActionSuccessfullyMultipleActions) {
  PrivacySandboxNoticeData data;
  data.SetNoticeEvents(BuildEvents({
      {kShown, 100},
      {kAck, 120},
      {kSettings, 150},
      {kShown, 200},
      {kOptIn, 250},
  }));

  EXPECT_EQ(data.GetNoticeActionTakenForFirstShownFromEvents(),
            (NoticeEventTimestampPair{
                kSettings, base::Time::FromMillisecondsSinceUnixEpoch(150)}));
}

TEST_F(
    PrivacySandboxNoticeDataTest,
    NoticeActionTakenEvent_AccessorReturnsActionSuccessfullyWithMultipleShownValues) {
  PrivacySandboxNoticeData data;
  data.SetNoticeEvents(BuildEvents({
      {kShown, 100},
      {kShown, 110},
      {kAck, 120},
      {kSettings, 150},
      {kShown, 200},
      {kShown, 220},
      {kOptIn, 250},
  }));

  EXPECT_EQ(data.GetNoticeActionTakenForFirstShownFromEvents(),
            (NoticeEventTimestampPair{
                kSettings, base::Time::FromMillisecondsSinceUnixEpoch(150)}));
}

}  // namespace
}  // namespace privacy_sandbox
