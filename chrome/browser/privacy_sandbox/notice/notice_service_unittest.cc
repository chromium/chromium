// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_catalog.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_storage.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/privacy_sandbox/notice/notice_catalog.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

using ::privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using ::privacy_sandbox::notice::mojom::PrivacySandboxNoticeEvent;
using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrEq;
using ::testing::Test;

using Event = PrivacySandboxNoticeEvent;
using enum PrivacySandboxNotice;
using enum SurfaceType;

// API Features
BASE_FEATURE(kEnabledApiFeature1, "1", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEnabledApiFeature2, "2", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEnabledApiFeature3, "3", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDisabledApiFeature, "0", base::FEATURE_DISABLED_BY_DEFAULT);

// Feature providing the storage name for the default notice in the catalog.
BASE_FEATURE(kNoticeFeatureEnabled,
             "StorageNameA",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kNoticeFeatureDisabled,
             "StorageNameB",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kNoticeFeatureEnabled2,
             "StorageNameA2",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kNoticeFeatureEnabled3,
             "StorageNameA3",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Default Notices
constexpr PrivacySandboxNotice kNotice1 =
    PrivacySandboxNotice::kTopicsConsentNotice;
constexpr PrivacySandboxNotice kNotice2 =
    PrivacySandboxNotice::kProtectedAudienceMeasurementNotice;
constexpr PrivacySandboxNotice kNotice3 =
    PrivacySandboxNotice::kThreeAdsApisNotice;

NoticeStorageData BuildStorageData(Event event) {
  NoticeStorageData data;
  data.notice_events.emplace_back(std::make_unique<NoticeEventTimestampPair>(
      NoticeEventTimestampPair{event, base::Time::Now()}));
  return data;
}

auto NoticeEligibleCallback() {
  return base::BindRepeating(
      []() { return EligibilityLevel::kEligibleNotice; });
}

auto ConsentEligibleCallback() {
  return base::BindRepeating(
      []() { return EligibilityLevel::kEligibleConsent; });
}

auto NotEligibleCallback() {
  return base::BindRepeating([]() { return EligibilityLevel::kNotEligible; });
}

class PrivacySandboxNoticeServiceTest : public Test {
 public:
  PrivacySandboxNoticeServiceTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();

    storage_unique_ptr_ = std::make_unique<NiceMock<MockNoticeStorage>>();
    mock_storage_ = storage_unique_ptr_.get();
    catalog_unique_ptr_ = std::make_unique<NiceMock<MockNoticeCatalog>>();
    mock_catalog_ = catalog_unique_ptr_.get();

    ON_CALL(*mock_catalog(), GetNotices())
        .WillByDefault(
            Invoke(this, &PrivacySandboxNoticeServiceTest::notice_ptrs));
    ON_CALL(*mock_catalog(), GetNoticeApis())
        .WillByDefault(
            Invoke(this, &PrivacySandboxNoticeServiceTest::api_ptrs));

    ON_CALL(*mock_catalog(), GetNotice(_))
        .WillByDefault(Invoke(
            this,
            &PrivacySandboxNoticeServiceTest::FindNoticeByIdInTestStorage));
  }

  NoticeId Notice1InCatalog() { return {kNotice1, kDesktopNewTab}; }
  NoticeId Notice2InCatalog() { return {kNotice2, kClankCustomTab}; }

  template <typename T>
  Notice* Make(NoticeId id, const base::Feature& feature) {
    std::unique_ptr<Notice> notice = std::make_unique<T>(id);
    Notice* notice_ptr = notice.get();
    notice->SetFeature(&feature);
    StoreObject(notice);
    return notice_ptr;
  }

  NoticeApi* MakeApi(const base::Feature& feature) {
    std::unique_ptr<NoticeApi> api = std::make_unique<NoticeApi>();
    NoticeApi* api_ptr = api.get();
    api_ptr->SetFeature(&feature);
    StoreObject(api);
    return api_ptr;
  }

  Notice* FindNoticeByIdInTestStorage(NoticeId notice_id) {
    for (Notice* notice_in_list : notice_ptrs()) {
      if (notice_in_list->notice_id() == notice_id) {
        return notice_in_list;
      }
    }
    return nullptr;
  }

  void SetUp() override {
    Make<Notice>(Notice1InCatalog(), kNoticeFeatureEnabled);
    Make<Notice>(Notice2InCatalog(), kNoticeFeatureDisabled);
  }

  void TearDown() override {
    internal_storage_.clear();
    notice_ptrs_.clear();
    api_ptrs_.clear();
  }

  void CreateNoticeService() {
    notice_service_ = std::make_unique<PrivacySandboxNoticeService>(
        profile_.get(), std::move(catalog_unique_ptr_),
        std::move(storage_unique_ptr_));
  }

 protected:
  PrivacySandboxNoticeService* notice_service() {
    return notice_service_.get();
  }
  MockNoticeStorage* mock_storage() { return mock_storage_; }
  TestingProfile* profile() { return profile_.get(); }
  MockNoticeCatalog* mock_catalog() { return mock_catalog_; }
  content::BrowserTaskEnvironment& task_environment() {
    return browser_task_environment_;
  }
  base::span<Notice*> notice_ptrs() { return base::span(notice_ptrs_); }
  base::span<NoticeApi*> api_ptrs() { return base::span(api_ptrs_); }

 private:
  template <typename Arg>
  void StoreObject(Arg&& arg) {
    KeepPointer(arg.get());
    internal_storage_.push_back(std::move(arg));
  }
  void KeepPointer(Notice* notice) { notice_ptrs_.push_back(notice); }
  void KeepPointer(NoticeApi* api) { api_ptrs_.push_back(api); }

  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  std::unique_ptr<MockNoticeCatalog> catalog_unique_ptr_;
  std::unique_ptr<MockNoticeStorage> storage_unique_ptr_;

  std::unique_ptr<PrivacySandboxNoticeService> notice_service_;

  raw_ptr<MockNoticeStorage> mock_storage_ = nullptr;
  raw_ptr<MockNoticeCatalog> mock_catalog_ = nullptr;

  // Notices
  std::vector<std::variant<std::unique_ptr<NoticeApi>, std::unique_ptr<Notice>>>
      internal_storage_;
  std::vector<Notice*> notice_ptrs_;
  std::vector<NoticeApi*> api_ptrs_;
};

TEST_F(PrivacySandboxNoticeServiceTest,
       Constructor_RefreshesAndSetsFulfilledStatus) {
  EXPECT_CALL(*mock_storage(), ReadNoticeData(StrEq("StorageNameA")))
      .WillOnce(Return(BuildStorageData(Event::kAck)));
  EXPECT_CALL(*mock_storage(), ReadNoticeData(StrEq("StorageNameB")))
      .WillOnce(Return(std::nullopt));

  EXPECT_FALSE(mock_catalog()->GetNotice(Notice1InCatalog())->was_fulfilled());
  EXPECT_FALSE(mock_catalog()->GetNotice(Notice2InCatalog())->was_fulfilled());

  CreateNoticeService();

  EXPECT_TRUE(mock_catalog()->GetNotice(Notice1InCatalog())->was_fulfilled());
  EXPECT_FALSE(mock_catalog()->GetNotice(Notice2InCatalog())->was_fulfilled());
}

TEST_F(PrivacySandboxNoticeServiceTest, Construction_EmitsStartupHistograms) {
  EXPECT_CALL(*mock_storage(), RecordStartupHistograms()).Times(1);

  CreateNoticeService();

  Mock::VerifyAndClearExpectations(mock_storage());
}

TEST_F(PrivacySandboxNoticeServiceTest,
       EventOccurred_NoticeFound_CallsRecordEvent) {
  // Set expectations on the storage mock For Startup.
  EXPECT_CALL(*mock_storage(), ReadNoticeData(StrEq("StorageNameA")))
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(*mock_storage(), ReadNoticeData(StrEq("StorageNameB")))
      .WillOnce(Return(std::nullopt));

  CreateNoticeService();

  EXPECT_FALSE(mock_catalog()
                   ->GetNotice(Notice1InCatalog())
                   ->was_fulfilled());  // Initial State.

  {
    testing::Sequence s;
    EXPECT_CALL(*mock_storage(), RecordEvent(Property(&Notice::notice_id,
                                                      Eq(Notice1InCatalog())),
                                             Eq(Event::kAck)));

    EXPECT_CALL(*mock_storage(), ReadNoticeData(StrEq("StorageNameA")))
        .WillOnce(Return(BuildStorageData(Event::kAck)));
  }

  notice_service()->EventOccurred(Notice1InCatalog(), Event::kAck);

  EXPECT_TRUE(mock_catalog()->GetNotice(Notice1InCatalog())->was_fulfilled());
}

TEST_F(PrivacySandboxNoticeServiceTest, EventOccurred_NoticeNotFound_Crashes) {
  CreateNoticeService();
  NoticeId notice_id_not_in_catalog = {kNotice3, kClankCustomTab};
  EXPECT_DEATH(
      notice_service()->EventOccurred(notice_id_not_in_catalog, Event::kShown),
      "");
}

class PrivacySandboxNoticeServiceGetRequiredNoticesTest
    : public PrivacySandboxNoticeServiceTest {
 public:
  PrivacySandboxNoticeServiceGetRequiredNoticesTest() {
    // Intentionally use a real Storage implementation, and forward the Mock
    // calls to it.
    storage_impl_ =
        std::make_unique<PrivacySandboxNoticeStorage>(profile()->GetPrefs());

    ON_CALL(*mock_storage(), RecordEvent(_, _))
        .WillByDefault(
            Invoke(storage_impl_.get(), &NoticeStorage::RecordEvent));

    ON_CALL(*mock_storage(), ReadNoticeData(_))
        .WillByDefault(
            Invoke(storage_impl_.get(), &NoticeStorage::ReadNoticeData));
  }
  void SetUp() override {}

 private:
  std::unique_ptr<NoticeStorage> storage_impl_;
};

TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       NoEligibleNotices_ReturnsEmpty) {
  // No notices or APIs stored.
  CreateNoticeService();
  EXPECT_THAT(notice_service()->GetRequiredNotices(kDesktopNewTab), IsEmpty());
}

TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       ApiIsDisabled_ReturnsEmpty) {
  NoticeApi* api = MakeApi(kDisabledApiFeature)
                       ->SetEligibilityCallback(NoticeEligibleCallback());
  Make<Notice>({kNotice1, kDesktopNewTab}, kNoticeFeatureEnabled)
      ->SetTargetApis({api});

  CreateNoticeService();

  EXPECT_THAT(notice_service()->GetRequiredNotices(kDesktopNewTab), IsEmpty());
}

TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       ApiIsNotEligible_ReturnsEmpty) {
  NoticeApi* api = MakeApi(kEnabledApiFeature1)
                       ->SetEligibilityCallback(NotEligibleCallback());
  Make<Notice>({kNotice1, kDesktopNewTab}, kNoticeFeatureEnabled)
      ->SetTargetApis({api});

  CreateNoticeService();

  EXPECT_THAT(notice_service()->GetRequiredNotices(kDesktopNewTab), IsEmpty());
}

TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       ApiIsAlreadyFulfilled_ReturnsEmpty) {
  NoticeApi* api = MakeApi(kEnabledApiFeature1)
                       ->SetEligibilityCallback(NoticeEligibleCallback());
  Notice* fulfilling_notice =
      Make<Notice>({kNotice2, kDesktopNewTab}, kNoticeFeatureEnabled);
  fulfilling_notice->SetTargetApis({api});

  Make<Notice>({kNotice1, kDesktopNewTab}, kNoticeFeatureEnabled2)
      ->SetTargetApis({api});

  CreateNoticeService();

  // Fulfilling the "fulfilling_notice" Notice by sending a kAck action.
  notice_service()->EventOccurred(fulfilling_notice->notice_id(), Event::kAck);

  EXPECT_THAT(notice_service()->GetRequiredNotices(kDesktopNewTab), IsEmpty());
}

TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       SingleApiEligibleAndUnfulfilled_ReturnsNotice) {
  NoticeApi* api = MakeApi(kEnabledApiFeature1)
                       ->SetEligibilityCallback(NoticeEligibleCallback());
  Make<Notice>({kNotice1, kDesktopNewTab}, kNoticeFeatureEnabled)
      ->SetTargetApis({api});

  CreateNoticeService();

  EXPECT_THAT(notice_service()->GetRequiredNotices(kDesktopNewTab),
              Eq(std::vector<PrivacySandboxNotice>{kNotice1}));
}

TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       NoticeTargetsDifferentSurface_ReturnsEmpty) {
  NoticeApi* api = MakeApi(kEnabledApiFeature1)
                       ->SetEligibilityCallback(NoticeEligibleCallback());
  Make<Notice>({kNotice1, kClankCustomTab},
               kNoticeFeatureEnabled)  // Different surface
      ->SetTargetApis({api});

  CreateNoticeService();

  EXPECT_THAT(notice_service()->GetRequiredNotices(kDesktopNewTab), IsEmpty());
}

TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       NoticeFeatureIsDisabled_ReturnsEmpty) {
  NoticeApi* api = MakeApi(kEnabledApiFeature1)
                       ->SetEligibilityCallback(NoticeEligibleCallback());
  Make<Notice>({kNotice1, kDesktopNewTab}, kNoticeFeatureDisabled)
      ->SetTargetApis({api});

  CreateNoticeService();

  EXPECT_THAT(notice_service()->GetRequiredNotices(kDesktopNewTab), IsEmpty());
}

TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       NoticeTargetsDisabledApi_ReturnsEmpty) {
  NoticeApi* api_enabled =
      MakeApi(kEnabledApiFeature1)
          ->SetEligibilityCallback(NoticeEligibleCallback());
  NoticeApi* api_disabled =
      MakeApi(kDisabledApiFeature)  // Will be filtered out
          ->SetEligibilityCallback(NoticeEligibleCallback());
  Make<Notice>({kNotice1, kDesktopNewTab}, kNoticeFeatureEnabled)
      ->SetTargetApis({api_enabled, api_disabled});  // Targets a filtered API

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kDisabledApiFeature);

  CreateNoticeService();

  EXPECT_THAT(notice_service()->GetRequiredNotices(kDesktopNewTab), IsEmpty());
}

TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       NoticeTargetsMultipleEligibleApis_ReturnsNotice) {
  NoticeApi* api1 = MakeApi(kEnabledApiFeature1)
                        ->SetEligibilityCallback(NoticeEligibleCallback());
  NoticeApi* api2 = MakeApi(kEnabledApiFeature2)
                        ->SetEligibilityCallback(NoticeEligibleCallback());
  Make<Notice>({kNotice1, kDesktopNewTab}, kNoticeFeatureEnabled)
      ->SetTargetApis({api1, api2});

  CreateNoticeService();

  EXPECT_THAT(notice_service()->GetRequiredNotices(kDesktopNewTab),
              Eq(std::vector<PrivacySandboxNotice>{kNotice1}));
}

// This test is intentionally similar to what typically happens with the Ads
// Notice in EEA.
TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       EeaLikeScenario_GroupedConsentAndNotice_ReturnsGroupSorted) {
  NoticeApi* consent_api1 =
      MakeApi(kEnabledApiFeature1)
          ->SetEligibilityCallback(ConsentEligibleCallback());
  NoticeApi* notice_api2 =
      MakeApi(kEnabledApiFeature2)
          ->SetEligibilityCallback(NoticeEligibleCallback());
  NoticeApi* notice_api3 =
      MakeApi(kEnabledApiFeature3)
          ->SetEligibilityCallback(NoticeEligibleCallback());
  Make<Consent>({kNotice1, kDesktopNewTab}, kNoticeFeatureEnabled)
      ->SetTargetApis({consent_api1})
      ->SetViewGroup({NoticeViewGroup::kAdsNoticeEeaGroup, 1});
  Make<Notice>({kNotice2, kDesktopNewTab}, kNoticeFeatureEnabled2)
      ->SetTargetApis({notice_api2, notice_api3})
      ->SetViewGroup({NoticeViewGroup::kAdsNoticeEeaGroup, 4});
  Make<Notice>({kNotice3, kDesktopNewTab}, kNoticeFeatureEnabled3)
      ->SetTargetApis({consent_api1, notice_api2, notice_api3});

  CreateNoticeService();

  EXPECT_THAT(notice_service()->GetRequiredNotices(kDesktopNewTab),
              Eq(std::vector<PrivacySandboxNotice>{kNotice1, kNotice2}));
}

// This test is intentionally similar to what typically happens with the Ads
// Notice in EEA.
TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       EeaLikeScenario_PartialFulfillment_ReturnsRemainingConsentNotice) {
  NoticeApi* consent_api1 =
      MakeApi(kEnabledApiFeature1)
          ->SetEligibilityCallback(ConsentEligibleCallback());
  NoticeApi* notice_api2 =
      MakeApi(kEnabledApiFeature2)
          ->SetEligibilityCallback(NoticeEligibleCallback());
  NoticeApi* notice_api3 =
      MakeApi(kEnabledApiFeature3)
          ->SetEligibilityCallback(NoticeEligibleCallback());
  Make<Consent>({kNotice1, kDesktopNewTab}, kNoticeFeatureEnabled)
      ->SetTargetApis({consent_api1})
      ->SetViewGroup({NoticeViewGroup::kAdsNoticeEeaGroup, 1});
  Make<Notice>({kNotice2, kDesktopNewTab}, kNoticeFeatureEnabled2)
      ->SetTargetApis({notice_api2, notice_api3})
      ->SetViewGroup({NoticeViewGroup::kAdsNoticeEeaGroup, 2});
  Make<Notice>({kNotice3, kDesktopNewTab}, kNoticeFeatureEnabled3)
      ->SetTargetApis({consent_api1, notice_api2, notice_api3});

  CreateNoticeService();

  notice_service()->EventOccurred({kNotice3, kDesktopNewTab}, Event::kAck);

  EXPECT_THAT(notice_service()->GetRequiredNotices(kDesktopNewTab),
              Eq(std::vector<PrivacySandboxNotice>{kNotice1}));
}

// This test is intentionally similar to what typically happens with the Ads
// Notice in ROW.
TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       RowLikeScenario_SingleNoticeSufficient_ReturnsSingleNotice) {
  NoticeApi* notice_api1 =
      MakeApi(kEnabledApiFeature1)
          ->SetEligibilityCallback(NoticeEligibleCallback());
  NoticeApi* notice_api2 =
      MakeApi(kEnabledApiFeature2)
          ->SetEligibilityCallback(NoticeEligibleCallback());
  NoticeApi* notice_api3 =
      MakeApi(kEnabledApiFeature3)
          ->SetEligibilityCallback(NoticeEligibleCallback());
  Make<Notice>({kNotice1, kDesktopNewTab}, kNoticeFeatureEnabled)
      ->SetTargetApis({notice_api1})
      ->SetViewGroup({NoticeViewGroup::kAdsNoticeEeaGroup, 1});
  Make<Notice>({kNotice2, kDesktopNewTab}, kNoticeFeatureEnabled)
      ->SetTargetApis({notice_api2, notice_api3})
      ->SetViewGroup({NoticeViewGroup::kAdsNoticeEeaGroup, 2});
  Make<Notice>({kNotice3, kDesktopNewTab}, kNoticeFeatureEnabled)
      ->SetTargetApis({notice_api1, notice_api2, notice_api3});

  CreateNoticeService();

  EXPECT_THAT(notice_service()->GetRequiredNotices(kDesktopNewTab),
              Eq(std::vector<PrivacySandboxNotice>{kNotice3}));
}

TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       Scoring_GroupTargetingMoreApisWins) {
  // Group A: Notice1 targets api1, api2
  NoticeApi* api1 = MakeApi(kEnabledApiFeature1)
                        ->SetEligibilityCallback(NoticeEligibleCallback());
  NoticeApi* api2 = MakeApi(kEnabledApiFeature2)
                        ->SetEligibilityCallback(NoticeEligibleCallback());
  Make<Notice>({kNotice1, kDesktopNewTab}, kNoticeFeatureEnabled)
      ->SetTargetApis({api1, api2});

  // Group B: Notice2 targets api3
  NoticeApi* api3 = MakeApi(kEnabledApiFeature3)
                        ->SetEligibilityCallback(NoticeEligibleCallback());
  Make<Notice>({kNotice2, kDesktopNewTab}, kNoticeFeatureEnabled2)
      ->SetTargetApis({api3});

  CreateNoticeService();

  EXPECT_THAT(notice_service()->GetRequiredNotices(kDesktopNewTab),
              Eq(std::vector<PrivacySandboxNotice>{kNotice1}));
}

TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       Scoring_GivenSameApiCountGroupWithFewerNoticesWins) {
  // Notice1 targets api1 (1 notice, 2 API)
  NoticeApi* api1 = MakeApi(kEnabledApiFeature1)
                        ->SetEligibilityCallback(NoticeEligibleCallback());
  NoticeApi* api2 = MakeApi(kEnabledApiFeature2)
                        ->SetEligibilityCallback(NoticeEligibleCallback());
  Make<Notice>({kNotice1, kDesktopNewTab}, kNoticeFeatureEnabled)
      ->SetTargetApis({api1, api2});

  // Group: Notice2 and Notice3 target api1 and api2 respectively

  Make<Notice>({kNotice2, kDesktopNewTab}, kNoticeFeatureDisabled)
      ->SetTargetApis({api1})
      ->SetViewGroup({NoticeViewGroup::kAdsNoticeEeaGroup, 1});
  Make<Notice>({kNotice3, kDesktopNewTab}, kNoticeFeatureEnabled2)
      ->SetTargetApis({api2})
      ->SetViewGroup({NoticeViewGroup::kAdsNoticeEeaGroup, 2});

  CreateNoticeService();

  EXPECT_THAT(notice_service()->GetRequiredNotices(kDesktopNewTab),
              Eq(std::vector<PrivacySandboxNotice>{kNotice1}));
}

TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       Scoring_WinningGroupWithMultipleNotices_ReturnsSortedNotices) {
  NoticeApi* api1 = MakeApi(kEnabledApiFeature1)
                        ->SetEligibilityCallback(NoticeEligibleCallback());
  NoticeApi* api2 = MakeApi(kEnabledApiFeature2)
                        ->SetEligibilityCallback(NoticeEligibleCallback());

  // Group A: Notice1 targets api1, Notice2 targets api2. Same ViewGroup.
  Make<Notice>({kNotice1, kDesktopNewTab}, kNoticeFeatureEnabled)
      ->SetTargetApis({api1})
      ->SetViewGroup({NoticeViewGroup::kAdsNoticeEeaGroup, 1});
  Make<Notice>({kNotice2, kDesktopNewTab}, kNoticeFeatureEnabled2)
      ->SetTargetApis({api2})
      ->SetViewGroup({NoticeViewGroup::kAdsNoticeEeaGroup, 2});

  // Group B (implicit): Notice3 targets api1. (Fewer APIs than group A)
  Make<Notice>({kNotice3, kDesktopNewTab}, kNoticeFeatureEnabled3)
      ->SetTargetApis({api1});

  CreateNoticeService();
  // Group A wins. returns the notices in the correct order.
  EXPECT_THAT(notice_service()->GetRequiredNotices(kDesktopNewTab),
              Eq(std::vector<PrivacySandboxNotice>{kNotice1, kNotice2}));
}

// Sorting Tests
TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       Sorting_NoticesInGroupAreSortedByViewOrder) {
  NoticeApi* api1 = MakeApi(kEnabledApiFeature1)
                        ->SetEligibilityCallback(NoticeEligibleCallback());
  // Notices in the same group, different order.
  Make<Notice>({kNotice1, kDesktopNewTab}, kNoticeFeatureEnabled)
      ->SetTargetApis({api1})
      ->SetViewGroup({NoticeViewGroup::kAdsNoticeEeaGroup, 2});  // Order 2
  Make<Notice>({kNotice2, kDesktopNewTab}, kNoticeFeatureEnabled2)
      ->SetTargetApis({api1})
      ->SetViewGroup({NoticeViewGroup::kAdsNoticeEeaGroup, 1});  // Order 1

  CreateNoticeService();

  EXPECT_THAT(
      notice_service()->GetRequiredNotices(kDesktopNewTab),
      Eq(std::vector<PrivacySandboxNotice>{
          kNotice2, kNotice1}));  // kNotice2 (order 1) then kNotice1 (order 2)
}

TEST_F(PrivacySandboxNoticeServiceGetRequiredNoticesTest,
       ApiFulfilledOnDifferentSurface_ReturnsEmpty) {
  // Setup an API that is targeted by two notices on different surfaces.
  // Fulfill the notice on one surface, then check if the notice on the other
  // surface (targeting the now fulfilled API) is returned.
  NoticeApi* api = MakeApi(kEnabledApiFeature1)
                       ->SetEligibilityCallback(NoticeEligibleCallback());

  Notice* notice_desktop =
      Make<Notice>({kNotice1, kClankBrApp}, kNoticeFeatureEnabled);
  notice_desktop->SetTargetApis({api});

  Notice* notice_clank = Make<Notice>(
      {kNotice1, kClankCustomTab},
      kNoticeFeatureEnabled2);  // Same notice type, different feature/storage
  notice_clank->SetTargetApis({api});

  CreateNoticeService();
  notice_service()->EventOccurred({kNotice1, kClankCustomTab},
                                  Event::kAck);  // API is now fulfilled.

  // Now, when we ask for notices on kClankBrApp, the API is already
  // fulfilled.
  EXPECT_THAT(notice_service()->GetRequiredNotices(kClankBrApp), IsEmpty());
}

}  // namespace
}  // namespace privacy_sandbox
