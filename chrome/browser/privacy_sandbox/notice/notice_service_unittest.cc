// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_service.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
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
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrEq;
using ::testing::Test;

using Event = PrivacySandboxNoticeEvent;
using enum PrivacySandboxNotice;
using enum SurfaceType;

// Feature providing the storage name for the default notice in the catalog.
BASE_FEATURE(kTestFeatureA, "StorageNameA", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeatureB, "StorageNameB", base::FEATURE_DISABLED_BY_DEFAULT);

// Notice ID for the default notice in the catalog.
constexpr NoticeId kNotice1InCatalog = {
    PrivacySandboxNotice::kTopicsConsentNotice, SurfaceType::kDesktopNewTab};
constexpr NoticeId kNotice2InCatalog = {
    PrivacySandboxNotice::kTopicsConsentNotice, SurfaceType::kClankCustomTab};

// A notice ID *not* expected in the default catalog.
constexpr NoticeId kNoticeIdNotInCatalog = {
    PrivacySandboxNotice::kMeasurementNotice, SurfaceType::kClankCustomTab};

std::unique_ptr<Notice> MakeNoticeWithFeature(NoticeId id,
                                              const base::Feature& feature) {
  auto notice = std::make_unique<Notice>(id);
  notice->SetFeature(&feature);
  return notice;
}

NoticeStorageData BuildStorageData(Event event) {
  NoticeStorageData data;
  data.notice_events.emplace_back(std::make_unique<NoticeEventTimestampPair>(
      NoticeEventTimestampPair{event, base::Time::Now()}));
  return data;
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

    default_notice_map_ = BuildDefaultNoticeMap();
    ON_CALL(*mock_catalog(), GetNoticeMap())
        .WillByDefault(ReturnRef(default_notice_map_));
    ON_CALL(*mock_catalog(), GetNotice(kNotice1InCatalog))
        .WillByDefault(Return(default_notice_map_[kNotice1InCatalog].get()));
    ON_CALL(*mock_catalog(), GetNotice(kNotice2InCatalog))
        .WillByDefault(Return(default_notice_map_[kNotice2InCatalog].get()));
    ON_CALL(*mock_catalog(), GetNotice(kNoticeIdNotInCatalog))
        .WillByDefault(Return(nullptr));
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

  NoticeMap BuildDefaultNoticeMap() {
    NoticeMap map;
    map.emplace(kNotice1InCatalog,
                MakeNoticeWithFeature(kNotice1InCatalog, kTestFeatureA));
    map.emplace(kNotice2InCatalog,
                MakeNoticeWithFeature(kNotice2InCatalog, kTestFeatureB));
    return map;
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  std::unique_ptr<MockNoticeCatalog> catalog_unique_ptr_;
  std::unique_ptr<MockNoticeStorage> storage_unique_ptr_;

  std::unique_ptr<PrivacySandboxNoticeService> notice_service_;

  raw_ptr<MockNoticeStorage> mock_storage_ = nullptr;
  raw_ptr<MockNoticeCatalog> mock_catalog_ = nullptr;

  NoticeMap default_notice_map_;
};

TEST_F(PrivacySandboxNoticeServiceTest,
       Constructor_RefreshesAndSetsFulfilledStatus) {
  EXPECT_CALL(*mock_storage(), ReadNoticeData(StrEq("StorageNameA")))
      .WillOnce(Return(BuildStorageData(Event::kAck)));
  EXPECT_CALL(*mock_storage(), ReadNoticeData(StrEq("StorageNameB")))
      .WillOnce(Return(std::nullopt));

  EXPECT_FALSE(mock_catalog()->GetNotice(kNotice1InCatalog)->was_fulfilled());
  EXPECT_FALSE(mock_catalog()->GetNotice(kNotice2InCatalog)->was_fulfilled());

  CreateNoticeService();

  EXPECT_TRUE(mock_catalog()->GetNotice(kNotice1InCatalog)->was_fulfilled());
  EXPECT_FALSE(mock_catalog()->GetNotice(kNotice2InCatalog)->was_fulfilled());
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
                   ->GetNotice(kNotice1InCatalog)
                   ->was_fulfilled());  // Initial State.

  {
    testing::Sequence s;
    EXPECT_CALL(*mock_storage(),
                RecordEvent(Eq(kNotice1InCatalog), Eq(Event::kAck)));

    EXPECT_CALL(*mock_storage(), ReadNoticeData(StrEq("StorageNameA")))
        .WillOnce(Return(BuildStorageData(Event::kAck)));
  }

  notice_service()->EventOccurred(kNotice1InCatalog, Event::kAck);

  EXPECT_TRUE(mock_catalog()->GetNotice(kNotice1InCatalog)->was_fulfilled());
}

TEST_F(PrivacySandboxNoticeServiceTest, EventOccurred_NoticeNotFound_Crashes) {
  CreateNoticeService();

  EXPECT_DEATH(
      notice_service()->EventOccurred(kNoticeIdNotInCatalog, Event::kShown),
      "");
}

// TODO(crbug.com/392612108): Write tests when GetRequiredNotices is
// implemented.

}  // namespace
}  // namespace privacy_sandbox
