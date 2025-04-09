// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_service.h"

#include "base/time/time.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_storage.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/privacy_sandbox/notice/notice_catalog.h"
#include "chrome/browser/privacy_sandbox/notice/notice_features.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

using testing::Combine;
using testing::Eq;
using testing::StrEq;
using testing::Test;
using testing::Values;
using testing::WithParamInterface;

using enum privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using enum privacy_sandbox::SurfaceType;
using privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using Event = privacy_sandbox::notice::mojom::PrivacySandboxNoticeEvent;

class PrivacySandboxNoticeServiceTest
    : public Test,
      public WithParamInterface<
          std::tuple<SurfaceType, std::pair<PrivacySandboxNotice, Event>>> {
 public:
  PrivacySandboxNoticeServiceTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    auto storage = std::make_unique<MockNoticeStorage>();
    mock_storage_ = storage.get();

    notice_service_ = std::make_unique<PrivacySandboxNoticeService>(
        profile_.get(), std::make_unique<NoticeCatalog>(), std::move(storage));
  }

 protected:
  PrivacySandboxNoticeService* notice_service() {
    return notice_service_.get();
  }
  MockNoticeStorage* mock_storage() { return mock_storage_; }
  TestingProfile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<PrivacySandboxNoticeService> notice_service_;
  raw_ptr<MockNoticeStorage> mock_storage_ = nullptr;
};

TEST_P(PrivacySandboxNoticeServiceTest, EventOccurredCallsRecordEvent) {
  auto [surface, notice_id_event] = GetParam();
  auto [notice, event] = notice_id_event;
  NoticeId notice_id = {notice, surface};

  std::string expected_notice_name;
  auto& notice_map = notice_service()->GetCatalog()->GetNoticeMap();
  auto it = notice_map.find(notice_id);
  ASSERT_NE(it, notice_map.end());
  ASSERT_NE(it->second, nullptr);
  expected_notice_name = it->second->GetStorageName();

  PrefService* expected_prefs = profile()->GetPrefs();
  base::Time expected_time = base::Time::Now();

  EXPECT_CALL(*mock_storage(),
              RecordEvent(Eq(expected_prefs), StrEq(expected_notice_name),
                          Eq(event), Eq(expected_time)))
      .Times(1);

  notice_service()->EventOccurred(notice_id, event);
}

INSTANTIATE_TEST_SUITE_P(
    PrivacySandboxNoticeServiceTest,
    PrivacySandboxNoticeServiceTest,
    Combine(Values(kDesktopNewTab, kClankBrApp, kClankCustomTab),
            Values(std::make_pair(kTopicsConsentNotice, Event::kOptIn),
                   std::make_pair(kThreeAdsApisNotice, Event::kShown),
                   std::make_pair(kThreeAdsApisNotice, Event::kAck),
                   std::make_pair(kProtectedAudienceMeasurementNotice,
                                  Event::kAck),
                   std::make_pair(kMeasurementNotice, Event::kAck))));
}  // namespace
}  // namespace privacy_sandbox
