#include "notice_model.h"
class NoticeCatalogApiTest {};  // Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
                            SurfaceType::kDesktopNewTab})
                       ->SetTargetApis({target_api})
                       ->SetPreReqApis({pre_req_api});

  EXPECT_THAT(notice->GetNoticeType(), NoticeType::kNotice);
  EXPECT_THAT(target_api->GetLinkedNotices(), Contains(notice));
  EXPECT_THAT(notice->GetNoticeId().first,
              PrivacySandboxNotice::kTopicsConsentNotice);
  EXPECT_THAT(notice->GetNoticeId().second, SurfaceType::kDesktopNewTab);
  EXPECT_THAT(notice->GetTargetApis(), Contains(target_api));
  EXPECT_THAT(notice->GetPreReqApis(), Contains(pre_req_api));
}

TEST_F(NoticeCatalogNoticeTest, RegisterNewNoticeGroupSuccessfully) {
  NoticeCatalog catalog;
  NoticeApi* target_api = catalog.RegisterAndRetrieveNewApi();

  NoticeApi* pre_req_api = catalog.RegisterAndRetrieveNewApi();

  catalog.RegisterNoticeGroup<Consent>(
      {{PrivacySandboxNotice::kTopicsConsentNotice,
        SurfaceType::kDesktopNewTab},
       {PrivacySandboxNotice::kTopicsConsentNotice, SurfaceType::kClankBrApp},
       {PrivacySandboxNotice::kTopicsConsentNotice,
        SurfaceType::kClankCustomTab}},
      {target_api});

  catalog.RegisterNoticeGroup<Notice>(
      {{PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
        SurfaceType::kDesktopNewTab},
       {PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
        SurfaceType::kClankBrApp},
       {PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
        SurfaceType::kClankCustomTab}},
      {target_api}, {pre_req_api});

  const auto& notice_map = catalog.GetNoticeMap();
  EXPECT_THAT(catalog.GetNoticeApis().size(), 2u);
  EXPECT_THAT(notice_map.size(), 6u);

  EXPECT_THAT(
      notice_map
          .find(std::make_pair(PrivacySandboxNotice::kTopicsConsentNotice,
                               SurfaceType::kDesktopNewTab))
          ->second.get()
          ->GetNoticeType(),
      NoticeType::kConsent);
  EXPECT_THAT(notice_map
                  .find(std::make_pair(
                      PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
                      SurfaceType::kDesktopNewTab))
                  ->second.get()
                  ->GetNoticeType(),
              NoticeType::kNotice);
}

}  // namespace
}  // namespace privacy_sandbox
