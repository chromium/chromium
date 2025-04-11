// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_service.h"

#include <memory>
#include <string>
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
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrEq;
using ::testing::Test;
using Event = PrivacySandboxNoticeEvent;
using enum PrivacySandboxNotice;
using enum SurfaceType;

BASE_FEATURE(kTestFeatureA, "TestFeatureA", base::FEATURE_DISABLED_BY_DEFAULT);

std::unique_ptr<Notice> MakeNotice(NoticeId id) {
  return std::make_unique<Notice>(id);
}

class PrivacySandboxNoticeServiceTest : public Test {
 public:
  PrivacySandboxNoticeServiceTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    auto storage = std::make_unique<MockNoticeStorage>();
    mock_storage_ = storage.get();
    auto catalog = std::make_unique<MockNoticeCatalog>();
    mock_catalog_ = catalog.get();

    notice_service_ = std::make_unique<PrivacySandboxNoticeService>(
        profile_.get(), std::move(catalog), std::move(storage));

    Mock::VerifyAndClearExpectations(mock_catalog_);
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
  std::unique_ptr<PrivacySandboxNoticeService> notice_service_;
  raw_ptr<MockNoticeStorage> mock_storage_ = nullptr;
  raw_ptr<MockNoticeCatalog> mock_catalog_ = nullptr;
};

TEST_F(PrivacySandboxNoticeServiceTest,
       EventOccurred_NoticeFound_CallsRecordEvent) {
  // 1. Create the Notice object that we expect the service to find.
  auto test_notice = MakeNotice({kThreeAdsApisNotice, kDesktopNewTab});
  test_notice->SetFeature(&kTestFeatureA);

  // 2. Create the NoticeMap containing the notice.
  NoticeMap test_notice_map;
  test_notice_map[{kThreeAdsApisNotice, kDesktopNewTab}] =
      std::move(test_notice);

  // 3. Mock GetNoticeMap to return our prepared map.
  EXPECT_CALL(*mock_catalog(), GetNoticeMap())
      .WillRepeatedly(ReturnRef(test_notice_map));

  // 4. Set expectations on the storage mock.
  PrefService* expected_prefs = profile()->GetPrefs();
  base::Time expected_time = base::Time::Now();
  EXPECT_CALL(*mock_storage(),
              RecordEvent(Eq(expected_prefs), StrEq("TestFeatureA"),
                          Eq(Event::kAck), Eq(expected_time)))
      .Times(1);

  // 5. Execute
  notice_service()->EventOccurred({kThreeAdsApisNotice, kDesktopNewTab},
                                  Event::kAck);

  // Ensure mock expectations are met.
  Mock::VerifyAndClearExpectations(mock_catalog());
  Mock::VerifyAndClearExpectations(mock_storage());
}

TEST_F(PrivacySandboxNoticeServiceTest, EventOccurred_NoticeNotFound_Crashes) {
  NoticeId unregistered_notice_id{kTopicsConsentNotice, kDesktopNewTab};

  // 1. Prepare an empty NoticeMap.
  NoticeMap empty_notice_map;

  // 2. Mock GetNoticeMap to return the empty map.
  EXPECT_CALL(*mock_catalog(), GetNoticeMap())
      .WillRepeatedly(ReturnRef(empty_notice_map));

  EXPECT_DEATH(
      notice_service()->EventOccurred(unregistered_notice_id, Event::kShown),
      "");

  // Ensure mock expectations are met.
  Mock::VerifyAndClearExpectations(mock_catalog());
}

// TODO(crbug.com/392612108): Write tests when GetRequiredNotices is
// implemented.

}  // namespace
}  // namespace privacy_sandbox
