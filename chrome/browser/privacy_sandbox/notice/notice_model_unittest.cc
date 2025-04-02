// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_model.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/privacy_sandbox/notice/notice_features.h"
#include "components/privacy_sandbox/privacy_sandbox_notice.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MockCallback;
using privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using privacy_sandbox::notice::mojom::PrivacySandboxNoticeEvent;
using testing::_;
using testing::Contains;
using testing::ValuesIn;

namespace privacy_sandbox {

template <typename T>
std::unique_ptr<Notice> Make(NoticeId id) {
  return std::make_unique<T>(id);
}
namespace {

class NoticeTest : public testing::Test {
 public:
  NoticeTest() : catalog_(std::make_unique<NoticeCatalog>()) {}

  Notice* RegisterAndRetrieveNotice(PrivacySandboxNotice notice) {
    return catalog_->RegisterAndRetrieveNewNotice(
        &Make<Notice>, {notice, SurfaceType::kDesktopNewTab});
  }

  Notice* RegisterAndRetrieveConsent(PrivacySandboxNotice notice) {
    return catalog_->RegisterAndRetrieveNewNotice(
        &Make<Consent>, {notice, SurfaceType::kDesktopNewTab});
  }

  NoticeCatalog* notice_catalog() { return catalog_.get(); }

 private:
  std::unique_ptr<NoticeCatalog> catalog_;
};

TEST_F(NoticeTest, NoticeIsFulfillmentEventCorrect) {
  Notice* notice =
      RegisterAndRetrieveNotice(PrivacySandboxNotice::kTopicsConsentNotice);
  for (const auto& event : {PrivacySandboxNoticeEvent::kAck,
                            PrivacySandboxNoticeEvent::kSettings}) {
    EXPECT_EQ(notice->IsFulfillmentEvent(event), true);
  }
  EXPECT_EQ(notice->IsFulfillmentEvent(PrivacySandboxNoticeEvent::kOptIn),
            false);
}

TEST_F(NoticeTest, ConsentIsFulfillmentEventCorrect) {
  Notice* notice =
      RegisterAndRetrieveConsent(PrivacySandboxNotice::kTopicsConsentNotice);
  for (const auto& event : {PrivacySandboxNoticeEvent::kOptIn,
                            PrivacySandboxNoticeEvent::kOptOut}) {
    EXPECT_EQ(notice->IsFulfillmentEvent(event), true);
  }
  EXPECT_EQ(notice->IsFulfillmentEvent(PrivacySandboxNoticeEvent::kAck), false);
}

TEST_F(NoticeTest, NoEligibilityCallbackReturnsNotEligible) {
  NoticeApi* api = notice_catalog()->RegisterAndRetrieveNewApi();
  EXPECT_EQ(api->GetEligibilityLevel(), EligibilityLevel::kNotEligible);
}

TEST_F(NoticeTest, SetEligibilityCallbackReturnsNoticeEligibilitySuccessfully) {
  NoticeApi* api =
      notice_catalog()->RegisterAndRetrieveNewApi()->SetEligibilityCallback(
          base::BindRepeating([]() -> EligibilityLevel {
            return EligibilityLevel::kEligibleNotice;
          }));
  RegisterAndRetrieveNotice(PrivacySandboxNotice::kTopicsConsentNotice)
      ->SetTargetApis({api});
  // TODO(crbug.com/392612108): Once WasFulfilled is implemented, change this
  // value.
  EXPECT_FALSE(api->IsFulfilled());
}

TEST_F(NoticeTest,
       SetEligibilityCallbackReturnsConsentEligibilitySuccessfully) {
  NoticeApi* api =
      notice_catalog()->RegisterAndRetrieveNewApi()->SetEligibilityCallback(
          base::BindRepeating([]() -> EligibilityLevel {
            return EligibilityLevel::kEligibleConsent;
          }));
  RegisterAndRetrieveConsent(PrivacySandboxNotice::kTopicsConsentNotice)
      ->SetTargetApis({api});
  // TODO(crbug.com/392612108): Once WasFulfilled is implemented, change this
  // value.
  EXPECT_FALSE(api->IsFulfilled());
}

TEST_F(NoticeTest, ConsentEligibilityWithNoticeTypeReturnsNotFulfilled) {
  NoticeApi* api =
      notice_catalog()->RegisterAndRetrieveNewApi()->SetEligibilityCallback(
          base::BindRepeating([]() -> EligibilityLevel {
            return EligibilityLevel::kEligibleConsent;
          }));
  RegisterAndRetrieveNotice(PrivacySandboxNotice::kTopicsConsentNotice)
      ->SetTargetApis({api});
  // TODO(crbug.com/392612108): Once WasFulfilled is implemented, change this
  // value.
  EXPECT_FALSE(api->IsFulfilled());
}

class ResultCallbackTest
    : public NoticeTest,
      public testing::WithParamInterface<PrivacySandboxNoticeEvent> {};

TEST_P(ResultCallbackTest, UpdateTargetApiResultsSuccess) {
  MockCallback<base::OnceCallback<void(bool)>> result_callback;
  NoticeApi* target =
      notice_catalog()->RegisterAndRetrieveNewApi()->SetResultCallback(
          result_callback.Get());
  MockCallback<base::OnceCallback<void(bool)>> result2_callback;
  NoticeApi* target2 =
      notice_catalog()->RegisterAndRetrieveNewApi()->SetResultCallback(
          result2_callback.Get());
  Notice* notice =
      RegisterAndRetrieveConsent(PrivacySandboxNotice::kTopicsConsentNotice)
          ->SetTargetApis({target, target2});

  // Set Expectations.
  if (notice->IsFulfillmentEvent(GetParam())) {
    EXPECT_CALL(result_callback, Run(_)).Times(1);
    EXPECT_CALL(result2_callback, Run(_)).Times(1);
  } else {
    EXPECT_CALL(result_callback, Run(_)).Times(0);
    EXPECT_CALL(result2_callback, Run(_)).Times(0);
  }
  notice->UpdateTargetApiResults(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    ResultCallbackTest,
    ResultCallbackTest,
    ValuesIn(std::vector<PrivacySandboxNoticeEvent>{
        PrivacySandboxNoticeEvent::kAck, PrivacySandboxNoticeEvent::kClosed,
        PrivacySandboxNoticeEvent::kOptIn, PrivacySandboxNoticeEvent::kOptOut,
        PrivacySandboxNoticeEvent::kSettings,
        PrivacySandboxNoticeEvent::kShown}));

class NoticeCatalogNoticeTest : public NoticeTest {};

TEST_F(NoticeCatalogNoticeTest, RegisterNewNoticeSuccessfully) {
  NoticeApi* target_api = notice_catalog()->RegisterAndRetrieveNewApi();

  NoticeApi* pre_req_api = notice_catalog()->RegisterAndRetrieveNewApi();

  EXPECT_EQ(notice_catalog()->GetNoticeApis().size(), 2u);
  Notice* notice =
      notice_catalog()
          ->RegisterAndRetrieveNewNotice(
              &Make<Consent>, {PrivacySandboxNotice::kTopicsConsentNotice,
                               SurfaceType::kDesktopNewTab})
          ->SetFeature(&kTopicsConsentDesktopModalFeature)
          ->SetTargetApis({target_api})
          ->SetPreReqApis({pre_req_api});

  EXPECT_EQ(notice->GetNoticeType(), NoticeType::kConsent);
  EXPECT_THAT(target_api->GetLinkedNotices(), Contains(notice));
  EXPECT_EQ(notice->GetNoticeId().first,
            PrivacySandboxNotice::kTopicsConsentNotice);
  EXPECT_EQ(notice->GetNoticeId().second, SurfaceType::kDesktopNewTab);
  EXPECT_THAT(notice->GetTargetApis(), Contains(target_api));
  EXPECT_THAT(notice->GetPreReqApis(), Contains(pre_req_api));
  EXPECT_EQ(notice->GetFeature(), &kTopicsConsentDesktopModalFeature);
}

TEST_F(NoticeCatalogNoticeTest, RegisterNewNoticeGroupSuccessfully) {
  NoticeApi* target_api = notice_catalog()->RegisterAndRetrieveNewApi();

  NoticeApi* pre_req_api = notice_catalog()->RegisterAndRetrieveNewApi();

  notice_catalog()->RegisterNoticeGroup(
      &Make<Consent>,
      {{{PrivacySandboxNotice::kTopicsConsentNotice,
         SurfaceType::kDesktopNewTab},
        &kTopicsConsentDesktopModalFeature},
       {{PrivacySandboxNotice::kTopicsConsentNotice, SurfaceType::kClankBrApp},
        &kTopicsConsentModalClankBrAppFeature},
       {{PrivacySandboxNotice::kTopicsConsentNotice,
         SurfaceType::kClankCustomTab},
        &kTopicsConsentModalClankCCTFeature}},
      {target_api});

  notice_catalog()->RegisterNoticeGroup(
      &Make<Notice>,
      {{{PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
         SurfaceType::kDesktopNewTab},
        &kProtectedAudienceMeasurementNoticeModalFeature},
       {{PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
         SurfaceType::kClankBrApp},
        &kProtectedAudienceMeasurementNoticeModalClankBrAppFeature},
       {{PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
         SurfaceType::kClankCustomTab},
        &kProtectedAudienceMeasurementNoticeModalClankCCTFeature}},
      {target_api}, {pre_req_api});

  const auto& notice_map = notice_catalog()->GetNoticeMap();
  EXPECT_THAT(notice_catalog()->GetNoticeApis().size(), 2u);
  EXPECT_THAT(notice_map.size(), 6u);

  Notice* topics_desktop_notice =
      notice_map
          .find(std::make_pair(PrivacySandboxNotice::kTopicsConsentNotice,
                               SurfaceType::kDesktopNewTab))
          ->second.get();
  EXPECT_EQ(topics_desktop_notice->GetNoticeType(), NoticeType::kConsent);

  Notice* pa_desktop_notice =
      notice_map
          .find(std::make_pair(
              PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
              SurfaceType::kDesktopNewTab))
          ->second.get();
  EXPECT_EQ(pa_desktop_notice->GetNoticeType(), NoticeType::kNotice);
}

TEST_F(NoticeCatalogNoticeTest,
       VerifyFeatureSetCorrectlyDuringNoticeGroupRegistration) {
  NoticeCatalog catalog;
  NoticeApi* target_api = notice_catalog()->RegisterAndRetrieveNewApi();

  notice_catalog()->RegisterNoticeGroup(
      &Make<Consent>,
      {{{PrivacySandboxNotice::kTopicsConsentNotice,
         SurfaceType::kDesktopNewTab},
        &kTopicsConsentDesktopModalFeature},
       {{PrivacySandboxNotice::kTopicsConsentNotice, SurfaceType::kClankBrApp},
        &kTopicsConsentModalClankBrAppFeature},
       {{PrivacySandboxNotice::kTopicsConsentNotice,
         SurfaceType::kClankCustomTab},
        &kTopicsConsentModalClankCCTFeature}},
      {target_api});

  const auto& notice_map = notice_catalog()->GetNoticeMap();

  Notice* topics_desktop_notice =
      notice_map
          .find(std::make_pair(PrivacySandboxNotice::kTopicsConsentNotice,
                               SurfaceType::kDesktopNewTab))
          ->second.get();
  EXPECT_EQ(topics_desktop_notice->GetFeature(),
            &kTopicsConsentDesktopModalFeature);
  Notice* topics_brapp_notice =
      notice_map
          .find(std::make_pair(PrivacySandboxNotice::kTopicsConsentNotice,
                               SurfaceType::kClankBrApp))
          ->second.get();
  EXPECT_EQ(topics_brapp_notice->GetFeature(),
            &kTopicsConsentModalClankBrAppFeature);
  Notice* topics_cct_notice =
      notice_map
          .find(std::make_pair(PrivacySandboxNotice::kTopicsConsentNotice,
                               SurfaceType::kClankCustomTab))
          ->second.get();
  EXPECT_EQ(topics_cct_notice->GetFeature(),
            &kTopicsConsentModalClankCCTFeature);
}
}  // namespace
}  // namespace privacy_sandbox
