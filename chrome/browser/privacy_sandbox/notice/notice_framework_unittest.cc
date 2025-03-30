// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_framework.h"

#include "chrome/browser/privacy_sandbox/notice/framework_features.h"
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

class NoticeFrameworkTest : public testing::Test,
                            public testing::WithParamInterface<NoticeId> {
 public:
  NoticeFrameworkTest() {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    framework_ =
        std::make_unique<PrivacySandboxNoticeFramework>(profile_.get());
  }

 protected:
  PrivacySandboxNoticeFramework* framework() { return framework_.get(); }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<PrivacySandboxNoticeFramework> framework_;
};

TEST_P(NoticeFrameworkTest, EventOccurredRegisteredInNoticeStorage) {
  NoticeId notice_id = GetParam();

  framework()->EventOccurred(notice_id, NoticeEvent::kShown);
  framework()->EventOccurred(notice_id, NoticeEvent::kAck);

  std::string_view notice_name = framework()
                                     ->GetCatalog()
                                     ->GetNoticeMap()
                                     .find(notice_id)
                                     ->second->GetFeature()
                                     ->name;
  // Pref
  auto actual = framework()->GetNoticeStorage()->ReadNoticeData(
      framework()->GetPrefService(), notice_name);
  EXPECT_EQ(actual->GetNoticeActionTakenForFirstShownFromEvents()->first,
            privacy_sandbox::NoticeEvent::kAck);
  EXPECT_EQ(actual->GetNoticeEvents().size(), 2u);
}

INSTANTIATE_TEST_SUITE_P(
    NoticeFrameworkTest,
    NoticeFrameworkTest,
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
