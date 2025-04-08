// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_model.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MockCallback;
using privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using privacy_sandbox::notice::mojom::PrivacySandboxNoticeEvent;
using testing::_;
using testing::Contains;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::Mock;
using testing::Return;
using testing::StrictMock;

using enum privacy_sandbox::EligibilityLevel;
using enum privacy_sandbox::notice::mojom::PrivacySandboxNoticeEvent;
using enum privacy_sandbox::NoticeType;
using enum privacy_sandbox::SurfaceType;

namespace privacy_sandbox {
namespace {

BASE_FEATURE(kTestFeatureA, "TestFeatureA", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeatureB, "TestFeatureB", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeatureC, "TestFeatureC", base::FEATURE_DISABLED_BY_DEFAULT);

// Test notice & Consent ID, with arbitrary NoticeType and SurfaceType.
constexpr NoticeId kTestNoticeId = {PrivacySandboxNotice::kThreeAdsApisNotice,
                                    kDesktopNewTab};

constexpr NoticeId kTestConsentId = {PrivacySandboxNotice::kTopicsConsentNotice,
                                     kClankBrApp};

// Helper function to create Notice/Consent objects easily in tests
template <typename T>
std::unique_ptr<Notice> Make(NoticeId notice_id) {
  return std::make_unique<T>(notice_id);
}

//-----------------------------------------------------------------------------
// Notice / Consent Tests
//-----------------------------------------------------------------------------

class PrivacySandboxNoticeModelTest : public testing::Test {};

TEST_F(PrivacySandboxNoticeModelTest, InitializeNotice) {
  Notice notice(kTestNoticeId);
  Consent consent(kTestConsentId);
  EXPECT_EQ(notice.GetNoticeId(), kTestNoticeId);
  EXPECT_EQ(notice.GetNoticeType(), kNotice);
}

TEST_F(PrivacySandboxNoticeModelTest, InitializeConsent) {
  Consent consent(kTestConsentId);
  EXPECT_EQ(consent.GetNoticeId(), kTestConsentId);
  EXPECT_EQ(consent.GetNoticeType(), kConsent);
}

TEST_F(PrivacySandboxNoticeModelTest, SetAndGetFeature) {
  Notice notice(kTestNoticeId);
  EXPECT_EQ(notice.GetFeature(), nullptr);
  notice.SetFeature(&kTestFeatureA);
  EXPECT_EQ(notice.GetFeature(), &kTestFeatureA);
}

TEST_F(PrivacySandboxNoticeModelTest, SetAndGetTargetApis) {
  Notice notice(kTestNoticeId);
  EXPECT_THAT(notice.GetTargetApis(), IsEmpty());

  NoticeApi api1, api2;
  notice.SetTargetApis({&api1, &api2});

  EXPECT_THAT(notice.GetTargetApis(), ElementsAre(&api1, &api2));
  EXPECT_THAT(api1.GetLinkedNotices(), Contains(&notice));
  EXPECT_THAT(api2.GetLinkedNotices(), Contains(&notice));
}

TEST_F(PrivacySandboxNoticeModelTest, SetAndGetPreReqApis) {
  Notice notice(kTestNoticeId);
  EXPECT_THAT(notice.GetPreReqApis(), IsEmpty());

  NoticeApi api1, api2;
  notice.SetPreReqApis({&api1, &api2});

  EXPECT_THAT(notice.GetPreReqApis(), ElementsAre(&api1, &api2));
}

TEST_F(PrivacySandboxNoticeModelTest, WasFulfilledInitialState) {
  // TODO(crbug.com/392612108): Update this test when WasFulfilled is
  // implemented.
  Notice notice(kTestNoticeId);
  Consent consent(kTestConsentId);
  EXPECT_FALSE(notice.WasFulfilled());
  EXPECT_FALSE(consent.WasFulfilled());
}

struct NoticeTestParam {
  std::unique_ptr<Notice> (*create)(NoticeId);
  PrivacySandboxNoticeEvent event;
  enum class Result {
    kOutcomeTrue,
    kOutcomeFalse,
    kNotFulfillment,
    kUnexpected,
  };
  Result expected_result;
};

using enum NoticeTestParam::Result;

class PrivacySandboxNoticeModelResultCallbackTest
    : public PrivacySandboxNoticeModelTest,
      public testing::WithParamInterface<NoticeTestParam> {};

TEST_P(PrivacySandboxNoticeModelResultCallbackTest, UpdateTargetApiResults) {
  const auto& param = GetParam();

  StrictMock<MockCallback<base::OnceCallback<void(bool)>>> callback_1,
      callback_2;
  NoticeApi target_1, target_2;
  target_1.SetResultCallback(callback_1.Get());
  target_2.SetResultCallback(callback_2.Get());

  auto notice = param.create(kTestNoticeId);
  notice->SetTargetApis({&target_1, &target_2});

  switch (param.expected_result) {
    case kOutcomeTrue:
      // Validate all result callbacks are called when a true fulfilment event.
      EXPECT_CALL(callback_1, Run(Eq(true))).Times(1);
      EXPECT_CALL(callback_2, Run(Eq(true))).Times(1);
      notice->UpdateTargetApiResults(param.event);
      break;
    case kOutcomeFalse:
      // Validate all result callbacks are called when a false fulfilment event.
      EXPECT_CALL(callback_1, Run(Eq(false))).Times(1);
      EXPECT_CALL(callback_2, Run(Eq(false))).Times(1);
      notice->UpdateTargetApiResults(param.event);
      break;
    case kNotFulfillment:
      // Some Events that aren't fulfilment events, will not call the Result
      // callback (validated via the StrictMock).
      notice->UpdateTargetApiResults(param.event);
      break;
    case kUnexpected:
      // Certain Unexpected Events should trigger a crash depending on the
      // Notice Type.
      EXPECT_DEATH(notice->UpdateTargetApiResults(param.event), "");
      break;
  }
  Mock::VerifyAndClearExpectations(&callback_1);
  Mock::VerifyAndClearExpectations(&callback_2);
}

std::vector<NoticeTestParam> notice_test_params = {
    // Notice
    {&Make<Notice>, kAck, kOutcomeTrue},
    {&Make<Notice>, kSettings, kOutcomeTrue},
    {&Make<Notice>, kShown, kNotFulfillment},
    {&Make<Notice>, kClosed, kUnexpected},
    {&Make<Notice>, kOptIn, kUnexpected},
    {&Make<Notice>, kOptOut, kUnexpected},
    // Consent
    {&Make<Consent>, kOptIn, kOutcomeTrue},
    {&Make<Consent>, kOptOut, kOutcomeFalse},
    {&Make<Consent>, kShown, kNotFulfillment},
    {&Make<Consent>, kAck, kUnexpected},
    {&Make<Consent>, kSettings, kUnexpected},
    {&Make<Consent>, kClosed, kUnexpected},
};

INSTANTIATE_TEST_SUITE_P(PrivacySandboxNoticeModelResultCallbackTest,
                         PrivacySandboxNoticeModelResultCallbackTest,
                         testing::ValuesIn(notice_test_params));

//-----------------------------------------------------------------------------
// NoticeApi Tests
//-----------------------------------------------------------------------------

class PrivacySandboxNoticeApiTest : public testing::Test {
 protected:
  NoticeApi api_;
  std::unique_ptr<Notice> notice_ = std::make_unique<Notice>(kTestNoticeId);
  std::unique_ptr<Consent> consent_ = std::make_unique<Consent>(kTestConsentId);
  StrictMock<MockCallback<base::RepeatingCallback<EligibilityLevel()>>>
      mock_eligibility_callback_;
  StrictMock<MockCallback<base::OnceCallback<void(bool)>>>
      mock_result_callback_;
};

TEST_F(PrivacySandboxNoticeApiTest, InitialState) {
  EXPECT_THAT(api_.GetLinkedNotices(), IsEmpty());
  EXPECT_EQ(api_.GetEligibilityLevel(), kNotEligible);
  EXPECT_FALSE(api_.IsFulfilled());
}

TEST_F(PrivacySandboxNoticeApiTest, SetAndGetEligibilityCallback) {
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  // Callback triggeerd via GetEligibilityLevel.
  EXPECT_CALL(mock_eligibility_callback_, Run())
      .Times(1)
      .WillOnce(Return(kEligibleNotice));
  EXPECT_EQ(api_.GetEligibilityLevel(), kEligibleNotice);
  Mock::VerifyAndClearExpectations(&mock_result_callback_);

  // Callback triggeered via IsFulfilled.
  EXPECT_CALL(mock_eligibility_callback_, Run()).Times(1);
  api_.IsFulfilled();
  Mock::VerifyAndClearExpectations(&mock_result_callback_);
}

TEST_F(PrivacySandboxNoticeApiTest, SetAndCallResultCallback) {
  EXPECT_CALL(mock_result_callback_, Run(Eq(true))).Times(1);
  api_.SetResultCallback(mock_result_callback_.Get());
  api_.UpdateResult(true);
  Mock::VerifyAndClearExpectations(&mock_result_callback_);

  EXPECT_CALL(mock_result_callback_, Run(Eq(false))).Times(1);
  api_.SetResultCallback(mock_result_callback_.Get());
  api_.UpdateResult(false);
  Mock::VerifyAndClearExpectations(&mock_result_callback_);
}

TEST_F(PrivacySandboxNoticeApiTest, UpdateResultWithoutCallback) {
  api_.UpdateResult(true);
  SUCCEED();
}

TEST_F(PrivacySandboxNoticeApiTest, CanBeFulfilledByAndGetLinkedNotices) {
  EXPECT_THAT(api_.GetLinkedNotices(), IsEmpty());
  api_.CanBeFulfilledBy(notice_.get());
  EXPECT_THAT(api_.GetLinkedNotices(), ElementsAre(notice_.get()));
  api_.CanBeFulfilledBy(consent_.get());
  EXPECT_THAT(api_.GetLinkedNotices(),
              ElementsAre(notice_.get(), consent_.get()));
}

TEST_F(PrivacySandboxNoticeApiTest, IsFulfilledNoLinkedNotices) {
  EXPECT_CALL(mock_eligibility_callback_, Run())
      .WillOnce(Return(kEligibleNotice));
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  EXPECT_FALSE(api_.IsFulfilled());
}

TEST_F(PrivacySandboxNoticeApiTest, IsFulfilledNotEligible) {
  EXPECT_CALL(mock_eligibility_callback_, Run()).WillOnce(Return(kNotEligible));
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  api_.CanBeFulfilledBy(notice_.get());
  EXPECT_FALSE(api_.IsFulfilled());
}

TEST_F(PrivacySandboxNoticeApiTest,
       IsFulfilledEligibleNoticeWithUnfulfilledNotice) {
  EXPECT_CALL(mock_eligibility_callback_, Run())
      .WillOnce(Return(kEligibleNotice));
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  api_.CanBeFulfilledBy(notice_.get());
  EXPECT_FALSE(api_.IsFulfilled());
}

TEST_F(PrivacySandboxNoticeApiTest,
       IsFulfilledEligibleConsentWithUnfulfilledConsent) {
  EXPECT_CALL(mock_eligibility_callback_, Run())
      .WillOnce(Return(kEligibleConsent));
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  api_.CanBeFulfilledBy(consent_.get());
  EXPECT_FALSE(api_.IsFulfilled());
}

TEST_F(PrivacySandboxNoticeApiTest,
       IsFulfilledEligibleConsentWithOnlyNoticeLinked) {
  EXPECT_CALL(mock_eligibility_callback_, Run())
      .WillOnce(Return(kEligibleConsent));
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  api_.CanBeFulfilledBy(notice_.get());
  EXPECT_FALSE(api_.IsFulfilled());
}

TEST_F(PrivacySandboxNoticeApiTest,
       IsFulfilledEligibleNoticeWithMixedNoticesUnfulfilled) {
  EXPECT_CALL(mock_eligibility_callback_, Run())
      .WillOnce(Return(kEligibleNotice));
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  api_.CanBeFulfilledBy(consent_.get());
  api_.CanBeFulfilledBy(notice_.get());
  EXPECT_FALSE(api_.IsFulfilled());
}

TEST_F(PrivacySandboxNoticeApiTest,
       IsFulfilledEligibleConsentWithMixedNoticesUnfulfilled) {
  EXPECT_CALL(mock_eligibility_callback_, Run())
      .WillOnce(Return(kEligibleConsent));
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  api_.CanBeFulfilledBy(notice_.get());
  api_.CanBeFulfilledBy(consent_.get());
  EXPECT_FALSE(api_.IsFulfilled());
}

//-----------------------------------------------------------------------------
// Catalog Tests
//-----------------------------------------------------------------------------

class PrivacySandboxNoticeCatalogTest : public testing::Test {
 protected:
  NoticeCatalog catalog_;
};

TEST_F(PrivacySandboxNoticeCatalogTest, InitialState) {
  EXPECT_THAT(catalog_.GetNoticeApis(), IsEmpty());
  EXPECT_THAT(catalog_.GetNoticeMap(), IsEmpty());
}

TEST_F(PrivacySandboxNoticeCatalogTest, RegisterAndRetrieveNewApi) {
  NoticeApi* api1 = catalog_.RegisterAndRetrieveNewApi();
  EXPECT_NE(api1, nullptr);
  EXPECT_EQ(catalog_.GetNoticeApis().size(), 1u);
  EXPECT_EQ(catalog_.GetNoticeApis()[0].get(), api1);

  NoticeApi* api2 = catalog_.RegisterAndRetrieveNewApi();
  EXPECT_NE(api2, nullptr);
  EXPECT_NE(api1, api2);
  EXPECT_EQ(catalog_.GetNoticeApis().size(), 2u);
  EXPECT_EQ(catalog_.GetNoticeApis()[1].get(), api2);
}

TEST_F(PrivacySandboxNoticeCatalogTest, RegisterAndRetrieveNewNotice) {
  NoticeApi* target_api = catalog_.RegisterAndRetrieveNewApi();
  NoticeApi* prereq_api = catalog_.RegisterAndRetrieveNewApi();

  Notice* consent_notice =
      catalog_.RegisterAndRetrieveNewNotice(&Make<Consent>, kTestConsentId);

  ASSERT_NE(consent_notice, nullptr);
  EXPECT_EQ(consent_notice->GetNoticeId(), kTestConsentId);
  EXPECT_EQ(consent_notice->GetNoticeType(), kConsent);
  EXPECT_EQ(catalog_.GetNoticeMap().size(), 1u);
  ASSERT_TRUE(catalog_.GetNoticeMap().contains(kTestConsentId));
  EXPECT_EQ(catalog_.GetNoticeMap().at(kTestConsentId).get(), consent_notice);

  consent_notice->SetFeature(&kTestFeatureA)
      ->SetTargetApis({target_api})
      ->SetPreReqApis({prereq_api});

  EXPECT_EQ(consent_notice->GetFeature(), &kTestFeatureA);
  EXPECT_THAT(consent_notice->GetTargetApis(), ElementsAre(target_api));
  EXPECT_THAT(consent_notice->GetPreReqApis(), ElementsAre(prereq_api));
  EXPECT_THAT(target_api->GetLinkedNotices(), Contains(consent_notice));
}

TEST_F(PrivacySandboxNoticeCatalogTest, RegisterNoticeGroup) {
  NoticeApi* target_api1 = catalog_.RegisterAndRetrieveNewApi();
  NoticeApi* target_api2 = catalog_.RegisterAndRetrieveNewApi();
  NoticeApi* prereq_api = catalog_.RegisterAndRetrieveNewApi();

  const NoticeId consent_id_desktop = {
      PrivacySandboxNotice::kTopicsConsentNotice, kDesktopNewTab};
  const NoticeId consent_id_clank = {PrivacySandboxNotice::kTopicsConsentNotice,
                                     kClankBrApp};
  const NoticeId notice_id_desktop = {
      PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
      kDesktopNewTab};

  catalog_.RegisterNoticeGroup(&Make<Consent>,
                               {{consent_id_desktop, &kTestFeatureA},
                                {consent_id_clank, &kTestFeatureB}},
                               {target_api1});

  catalog_.RegisterNoticeGroup(&Make<Notice>,
                               {{notice_id_desktop, &kTestFeatureC}},
                               {target_api1, target_api2}, {prereq_api});

  EXPECT_EQ(catalog_.GetNoticeApis().size(), 3u);
  EXPECT_EQ(catalog_.GetNoticeMap().size(), 3u);

  ASSERT_TRUE(catalog_.GetNoticeMap().contains(consent_id_desktop));
  Notice* consent_desktop =
      catalog_.GetNoticeMap().at(consent_id_desktop).get();
  EXPECT_EQ(consent_desktop->GetNoticeType(), kConsent);
  EXPECT_EQ(consent_desktop->GetFeature(), &kTestFeatureA);
  EXPECT_THAT(consent_desktop->GetTargetApis(), ElementsAre(target_api1));
  EXPECT_THAT(consent_desktop->GetPreReqApis(), IsEmpty());
  EXPECT_THAT(target_api1->GetLinkedNotices(), Contains(consent_desktop));

  ASSERT_TRUE(catalog_.GetNoticeMap().contains(consent_id_clank));
  Notice* consent_clank = catalog_.GetNoticeMap().at(consent_id_clank).get();
  EXPECT_EQ(consent_clank->GetNoticeType(), kConsent);
  EXPECT_EQ(consent_clank->GetFeature(), &kTestFeatureB);
  EXPECT_THAT(consent_clank->GetTargetApis(), ElementsAre(target_api1));
  EXPECT_THAT(consent_clank->GetPreReqApis(), IsEmpty());
  EXPECT_THAT(target_api1->GetLinkedNotices(), Contains(consent_clank));

  ASSERT_TRUE(catalog_.GetNoticeMap().contains(notice_id_desktop));
  Notice* notice_desktop = catalog_.GetNoticeMap().at(notice_id_desktop).get();
  EXPECT_EQ(notice_desktop->GetNoticeType(), kNotice);
  EXPECT_EQ(notice_desktop->GetFeature(), &kTestFeatureC);
  EXPECT_THAT(notice_desktop->GetTargetApis(),
              ElementsAre(target_api1, target_api2));
  EXPECT_THAT(notice_desktop->GetPreReqApis(), ElementsAre(prereq_api));
  EXPECT_THAT(target_api1->GetLinkedNotices(), Contains(notice_desktop));
  EXPECT_THAT(target_api2->GetLinkedNotices(), Contains(notice_desktop));
}
}  // namespace
}  // namespace privacy_sandbox
