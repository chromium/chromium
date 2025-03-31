// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_service.h"

#include "chrome/browser/privacy_sandbox/notice/notice_features.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/privacy_sandbox/privacy_sandbox_notice.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

using privacy_sandbox::notice::mojom::PrivacySandboxNotice;

class NoticeServiceTest : public testing::Test,
                          public testing::WithParamInterface<NoticeId> {
 public:
  NoticeServiceTest() {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    notice_service_ =
        std::make_unique<PrivacySandboxNoticeService>(profile_.get());
  }

 protected:
  PrivacySandboxNoticeService* notice_service() {
    return notice_service_.get();
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<PrivacySandboxNoticeService> notice_service_;
};

TEST_P(NoticeServiceTest, EventOccurredRegisteredInNoticeStorage) {
  NoticeId notice_id = GetParam();

  notice_service()->EventOccurred(notice_id, NoticeEvent::kShown);
  notice_service()->EventOccurred(notice_id, NoticeEvent::kAck);

  std::string_view notice_name = notice_service()
                                     ->GetCatalog()
                                     ->GetNoticeMap()
                                     .find(notice_id)
                                     ->second->GetFeature()
                                     ->name;
  // Pref
  auto actual = notice_service()->GetNoticeStorage()->ReadNoticeData(
      notice_service()->GetPrefService(), notice_name);
  EXPECT_EQ(actual->GetNoticeActionTakenForFirstShownFromEvents()->first,
            privacy_sandbox::NoticeEvent::kAck);
  EXPECT_EQ(actual->GetNoticeEvents().size(), 2u);
}

INSTANTIATE_TEST_SUITE_P(
    NoticeServiceTest,
    NoticeServiceTest,
    testing::Values(
        std::make_pair(PrivacySandboxNotice::kTopicsConsentNotice,
                       SurfaceType::kDesktopNewTab),
        std::make_pair(PrivacySandboxNotice::kTopicsConsentNotice,
                       SurfaceType::kClankBrApp),
        std::make_pair(PrivacySandboxNotice::kTopicsConsentNotice,
                       SurfaceType::kClankCustomTab),
        std::make_pair(PrivacySandboxNotice::kThreeAdsApisNotice,
                       SurfaceType::kDesktopNewTab),
        std::make_pair(PrivacySandboxNotice::kThreeAdsApisNotice,
                       SurfaceType::kClankBrApp),
        std::make_pair(PrivacySandboxNotice::kThreeAdsApisNotice,
                       SurfaceType::kClankCustomTab),
        std::make_pair(
            PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
            SurfaceType::kDesktopNewTab),
        std::make_pair(
            PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
            SurfaceType::kClankBrApp),
        std::make_pair(
            PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
            SurfaceType::kClankCustomTab),
        std::make_pair(PrivacySandboxNotice::kMeasurementNotice,
                       SurfaceType::kDesktopNewTab),
        std::make_pair(PrivacySandboxNotice::kMeasurementNotice,
                       SurfaceType::kClankBrApp),
        std::make_pair(PrivacySandboxNotice::kMeasurementNotice,
                       SurfaceType::kClankCustomTab)));
}  // namespace
}  // namespace privacy_sandbox
