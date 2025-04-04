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

using testing::Combine;
using testing::Test;
using testing::Values;
using testing::WithParamInterface;

using enum privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using Event = privacy_sandbox::notice::mojom::PrivacySandboxNoticeEvent;

class PrivacySandboxNoticeServiceTest
    : public Test,
      public WithParamInterface<
          std::tuple<SurfaceType, std::pair<PrivacySandboxNotice, Event>>> {
 public:
  PrivacySandboxNoticeServiceTest() {
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

TEST_P(PrivacySandboxNoticeServiceTest,
       EventOccurredRegisteredInNoticeStorage) {
  auto [surface, notice_id_event] = GetParam();
  auto [notice, event] = notice_id_event;

  notice_service()->EventOccurred({notice, surface}, Event::kShown);
  notice_service()->EventOccurred({notice, surface}, event);

  std::string_view notice_name = notice_service()
                                     ->GetCatalog()
                                     ->GetNoticeMap()
                                     .find({notice, surface})
                                     ->second->GetFeature()
                                     ->name;
  // Pref
  auto actual = notice_service()->GetNoticeStorage()->ReadNoticeData(
      notice_service()->GetPrefService(), notice_name);
  EXPECT_EQ(actual->GetNoticeActionTakenForFirstShownFromEvents()->first,
            event);
  EXPECT_EQ(actual->GetNoticeEvents().size(), 2u);
}

INSTANTIATE_TEST_SUITE_P(
    PrivacySandboxNoticeServiceTest,
    PrivacySandboxNoticeServiceTest,
    Combine(Values(kDesktopNewTab, kClankBrApp, kClankCustomTab),
            Values(std::make_pair(kTopicsConsentNotice, Event::kOptIn),
                   std::make_pair(kThreeAdsApisNotice, Event::kAck),
                   std::make_pair(kProtectedAudienceMeasurementNotice,
                                  Event::kAck),
                   std::make_pair(kMeasurementNotice, Event::kAck))));
}  // namespace
}  // namespace privacy_sandbox
