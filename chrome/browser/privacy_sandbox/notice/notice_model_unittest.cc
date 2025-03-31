#include "notice_model.h"
class NoticeCatalogApiTest {};  // Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_features.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "components/privacy_sandbox/privacy_sandbox_notice.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

using privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using testing::Contains;

namespace privacy_sandbox {
namespace {

class NoticeCatalogNoticeTest : public testing::Test {};

TEST_F(NoticeCatalogNoticeTest, RegisterNewNoticeSuccessfully) {
  NoticeCatalog catalog;
  NoticeApi* target_api = catalog.RegisterAndRetrieveNewApi();

  NoticeApi* pre_req_api = catalog.RegisterAndRetrieveNewApi();

  EXPECT_THAT(catalog.GetNoticeApis().size(), 2u);
  Notice* notice = catalog
                       .RegisterAndRetrieveNewNotice<Notice>(
                           {PrivacySandboxNotice::kTopicsConsentNotice,
                            SurfaceType::kDesktopNewTab},
                           &privacy_sandbox::kTopicsConsentDesktopModalFeature)
                       ->SetTargetApis({target_api})
                       ->SetPreReqApis({pre_req_api});

  EXPECT_THAT(notice->GetNoticeType(), NoticeType::kNotice);
  EXPECT_THAT(target_api->GetLinkedNotices(), Contains(notice));
  EXPECT_THAT(notice->GetNoticeId().first,
              PrivacySandboxNotice::kTopicsConsentNotice);
  EXPECT_THAT(notice->GetNoticeId().second, SurfaceType::kDesktopNewTab);
  EXPECT_THAT(notice->GetTargetApis(), Contains(target_api));
  EXPECT_THAT(notice->GetPreReqApis(), Contains(pre_req_api));
  EXPECT_THAT(notice->GetFeature(),
              &privacy_sandbox::kTopicsConsentDesktopModalFeature);
}

TEST_F(NoticeCatalogNoticeTest, RegisterNewNoticeGroupSuccessfully) {
  NoticeCatalog catalog;
  NoticeApi* target_api = catalog.RegisterAndRetrieveNewApi();

  NoticeApi* pre_req_api = catalog.RegisterAndRetrieveNewApi();

  catalog.RegisterNoticeGroup<Consent>(
      {{{PrivacySandboxNotice::kTopicsConsentNotice,
         SurfaceType::kDesktopNewTab},
        &privacy_sandbox::kTopicsConsentDesktopModalFeature},
       {{PrivacySandboxNotice::kTopicsConsentNotice, SurfaceType::kClankBrApp},
        &privacy_sandbox::kTopicsConsentModalClankBrAppFeature},
       {{PrivacySandboxNotice::kTopicsConsentNotice,
         SurfaceType::kClankCustomTab},
        &privacy_sandbox::kTopicsConsentModalClankCCTFeature}},
      {target_api});

  catalog.RegisterNoticeGroup<Notice>(
      {{{PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
         SurfaceType::kDesktopNewTab},
        &privacy_sandbox::kProtectedAudienceMeasurementNoticeModalFeature},
       {{PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
         SurfaceType::kClankBrApp},
        &privacy_sandbox::
            kProtectedAudienceMeasurementNoticeModalClankBrAppFeature},
       {{PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
         SurfaceType::kClankCustomTab},
        &privacy_sandbox::
            kProtectedAudienceMeasurementNoticeModalClankCCTFeature}},
      {target_api}, {pre_req_api});

  const auto& notice_map = catalog.GetNoticeMap();
  EXPECT_THAT(catalog.GetNoticeApis().size(), 2u);
  EXPECT_THAT(notice_map.size(), 6u);

  Notice* topics_desktop_notice =
      notice_map
          .find(std::make_pair(PrivacySandboxNotice::kTopicsConsentNotice,
                               SurfaceType::kDesktopNewTab))
          ->second.get();
  EXPECT_THAT(topics_desktop_notice->GetNoticeType(), NoticeType::kConsent);

  Notice* pa_desktop_notice =
      notice_map
          .find(std::make_pair(
              PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
              SurfaceType::kDesktopNewTab))
          ->second.get();
  EXPECT_THAT(pa_desktop_notice->GetNoticeType(), NoticeType::kNotice);
}

TEST_F(NoticeCatalogNoticeTest,
       VerifyFeatureSetCorrectlyDuringNoticeGroupRegistration) {
  NoticeCatalog catalog;
  NoticeApi* target_api = catalog.RegisterAndRetrieveNewApi();

  catalog.RegisterNoticeGroup<Consent>(
      {{{PrivacySandboxNotice::kTopicsConsentNotice,
         SurfaceType::kDesktopNewTab},
        &privacy_sandbox::kTopicsConsentDesktopModalFeature},
       {{PrivacySandboxNotice::kTopicsConsentNotice, SurfaceType::kClankBrApp},
        &privacy_sandbox::kTopicsConsentModalClankBrAppFeature},
       {{PrivacySandboxNotice::kTopicsConsentNotice,
         SurfaceType::kClankCustomTab},
        &privacy_sandbox::kTopicsConsentModalClankCCTFeature}},
      {target_api});

  const auto& notice_map = catalog.GetNoticeMap();

  Notice* topics_desktop_notice =
      notice_map
          .find(std::make_pair(PrivacySandboxNotice::kTopicsConsentNotice,
                               SurfaceType::kDesktopNewTab))
          ->second.get();
  EXPECT_THAT(topics_desktop_notice->GetFeature(),
              &privacy_sandbox::kTopicsConsentDesktopModalFeature);
  Notice* topics_brapp_notice =
      notice_map
          .find(std::make_pair(PrivacySandboxNotice::kTopicsConsentNotice,
                               SurfaceType::kClankBrApp))
          ->second.get();
  EXPECT_THAT(topics_brapp_notice->GetFeature(),
              &privacy_sandbox::kTopicsConsentModalClankBrAppFeature);
  Notice* topics_cct_notice =
      notice_map
          .find(std::make_pair(PrivacySandboxNotice::kTopicsConsentNotice,
                               SurfaceType::kClankCustomTab))
          ->second.get();
  EXPECT_THAT(topics_cct_notice->GetFeature(),
              &privacy_sandbox::kTopicsConsentModalClankCCTFeature);
}
}  // namespace
}  // namespace privacy_sandbox
