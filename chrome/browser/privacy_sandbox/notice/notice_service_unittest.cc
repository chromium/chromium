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
#include "components/prefs/pref_service.h"
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
using ::testing::ReturnRef;
using ::testing::StrEq;
using ::testing::Test;
using Event = PrivacySandboxNoticeEvent;
using enum PrivacySandboxNotice;
using enum SurfaceType;

BASE_FEATURE(kTestFeatureA, "TestFeatureA", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeatureB, "TestFeatureB", base::FEATURE_DISABLED_BY_DEFAULT);

std::unique_ptr<Notice> MakeNoticeWithFeature(NoticeId id,
                                              const base::Feature& feature) {
  auto notice = std::make_unique<Notice>(id);
  notice->SetFeature(&feature);
  return notice;
}

class PrivacySandboxNoticeServiceTest : public Test {
 public:
  PrivacySandboxNoticeServiceTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();

    storage_unique_ptr_ = std::make_unique<MockNoticeStorage>();
    mock_storage_ = storage_unique_ptr_.get();
    catalog_unique_ptr_ = std::make_unique<MockNoticeCatalog>();
    mock_catalog_ = catalog_unique_ptr_.get();
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

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  std::unique_ptr<MockNoticeCatalog> catalog_unique_ptr_;
  std::unique_ptr<MockNoticeStorage> storage_unique_ptr_;

  std::unique_ptr<PrivacySandboxNoticeService> notice_service_;

  raw_ptr<MockNoticeStorage> mock_storage_ = nullptr;
  raw_ptr<MockNoticeCatalog> mock_catalog_ = nullptr;
};

TEST_F(PrivacySandboxNoticeServiceTest,
       Construction_EmitsStartupHistogramsForEachNotice) {
  // 1. Prepare the NoticeMap
  NoticeMap test_notice_map;
  NoticeId notice_id_a = {kThreeAdsApisNotice, kDesktopNewTab};
  NoticeId notice_id_b = {kTopicsConsentNotice, kDesktopNewTab};

  test_notice_map[notice_id_a] =
      MakeNoticeWithFeature(notice_id_a, kTestFeatureA);
  test_notice_map[notice_id_b] =
      MakeNoticeWithFeature(notice_id_b, kTestFeatureB);

  // 2. Set expectation on Catalog: GetNoticeMap called during construction.
  EXPECT_CALL(*mock_catalog(), GetNoticeMap())
      .WillOnce(ReturnRef(test_notice_map));

  // 3. Set expectations on Storage: RecordHistogramsOnStartup called for each
  // notice.
  PrefService* expected_prefs = profile()->GetPrefs();
  EXPECT_CALL(*mock_storage(), RecordHistogramsOnStartup(Eq(expected_prefs),
                                                         StrEq("TestFeatureA")))
      .Times(1);
  EXPECT_CALL(*mock_storage(), RecordHistogramsOnStartup(Eq(expected_prefs),
                                                         StrEq("TestFeatureB")))
      .Times(1);

  // 4. Execute: Create the service, which should trigger the histogram calls.
  CreateNoticeService();

  // 5. Verify: Ensure mock expectations were met.
  Mock::VerifyAndClearExpectations(mock_catalog());
  Mock::VerifyAndClearExpectations(mock_storage());
}

TEST_F(PrivacySandboxNoticeServiceTest,
       EventOccurred_NoticeFound_CallsRecordEvent) {
  // 1. Create the Notice object that we expect the service to find.
  auto test_notice = MakeNoticeWithFeature(
      {kThreeAdsApisNotice, kDesktopNewTab}, kTestFeatureA);

  NoticeMap test_notice_map;
  test_notice_map[{kThreeAdsApisNotice, kDesktopNewTab}] =
      std::move(test_notice);

  // 3. Mock GetNoticeMap to return our prepared map.
  EXPECT_CALL(*mock_catalog(), GetNoticeMap())
      .WillRepeatedly(ReturnRef(test_notice_map));

  // Ignore constructor histogram calls
  EXPECT_CALL(*mock_storage(), RecordHistogramsOnStartup(_, _))
      .Times(testing::AnyNumber());

  // 4. Set expectations on the storage mock.
  PrefService* expected_prefs = profile()->GetPrefs();
  base::Time expected_time = base::Time::Now();
  EXPECT_CALL(*mock_storage(),
              RecordEvent(Eq(expected_prefs), StrEq("TestFeatureA"),
                          Eq(Event::kAck), Eq(expected_time)))
      .Times(1);

  // 5. Execute
  CreateNoticeService();

  notice_service()->EventOccurred({kThreeAdsApisNotice, kDesktopNewTab},
                                  Event::kAck);

  // Ensure mock expectations are met.
  Mock::VerifyAndClearExpectations(mock_catalog());
  Mock::VerifyAndClearExpectations(mock_storage());
}

TEST_F(PrivacySandboxNoticeServiceTest, EventOccurred_NoticeNotFound_Crashes) {
  NoticeId unregistered_notice_id{kTopicsConsentNotice, kDesktopNewTab};
  NoticeMap empty_notice_map;

  EXPECT_CALL(*mock_catalog(), GetNoticeMap())
      .WillRepeatedly(
          ReturnRef(empty_notice_map));  // Called on construction and event

  // Ignore constructor histogram calls
  EXPECT_CALL(*mock_storage(), RecordHistogramsOnStartup(_, _))
      .Times(testing::AnyNumber());

  // Create the service
  CreateNoticeService();

  EXPECT_DEATH(
      notice_service()->EventOccurred(unregistered_notice_id, Event::kShown),
      "");

  // Ensure mock expectations are met.
  Mock::VerifyAndClearExpectations(mock_catalog());
  Mock::VerifyAndClearExpectations(mock_storage());
}

// TODO(crbug.com/392612108): Write tests when GetRequiredNotices is
// implemented.

}  // namespace
}  // namespace privacy_sandbox
