// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_model.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/test/mock_callback.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_storage.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

using base::MockCallback;
using privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using privacy_sandbox::notice::mojom::PrivacySandboxNoticeEvent;
using testing::_;
using testing::Contains;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::StrEq;
using testing::StrictMock;

using enum privacy_sandbox::EligibilityLevel;
using enum privacy_sandbox::notice::mojom::PrivacySandboxNoticeEvent;
using enum privacy_sandbox::NoticeType;
using enum privacy_sandbox::SurfaceType;

BASE_FEATURE(kTestFeatureA, "TestFeatureA", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeatureB, "TestFeatureB", base::FEATURE_DISABLED_BY_DEFAULT);

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

TEST_F(PrivacySandboxNoticeModelTest, GetStorageName) {
  Notice notice(kTestNoticeId);
  notice.SetFeature(&kTestFeatureA);
  EXPECT_EQ(notice.GetStorageName(), kTestFeatureA.name);
}

TEST_F(PrivacySandboxNoticeModelTest, GetStorageNameNullFeatureCrashes) {
  Notice notice(kTestNoticeId);
  EXPECT_DEATH(notice.GetStorageName(), "");
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

TEST_F(PrivacySandboxNoticeModelTest, Notice_InitialWasFulfilledIsFalse) {
  Notice notice(kTestNoticeId);
  EXPECT_FALSE(notice.was_fulfilled());
}

TEST_F(PrivacySandboxNoticeModelTest, Consent_InitialWasFulfilledIsFalse) {
  Consent consent(kTestConsentId);
  EXPECT_FALSE(consent.was_fulfilled());
}

struct NoticeResultCallbackTestParam {
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

using enum NoticeResultCallbackTestParam::Result;

class PrivacySandboxNoticeModelResultCallbackTest
    : public PrivacySandboxNoticeModelTest,
      public testing::WithParamInterface<NoticeResultCallbackTestParam> {};

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

std::vector<NoticeResultCallbackTestParam> notice_result_test_params = {
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
                         testing::ValuesIn(notice_result_test_params));

//-----------------------------------------------------------------------------
// Notice / Consent Fulfillment Tests
//-----------------------------------------------------------------------------

class PrivacySandboxNoticeFulfillmentTestBase : public testing::Test {
 protected:
  PrivacySandboxNoticeFulfillmentTestBase() = default;
  NoticeStorageData CreateNoticeStorageDataWithEvents(
      const std::vector<NoticeEventTimestampPair>& events_with_times) {
    NoticeStorageData data;
    for (const auto& event_time_pair : events_with_times) {
      data.notice_events.emplace_back(
          std::make_unique<NoticeEventTimestampPair>(event_time_pair));
    }
    return data;
  }
  NiceMock<MockNoticeStorage> mock_storage_;
};

enum class TestNoticeTypeParam {
  kTestNotice,
  kTestConsent,
};

using enum TestNoticeTypeParam;

struct FulfillmentTestParam {
  TestNoticeTypeParam notice_type_to_test;
  std::optional<std::vector<NoticeEventTimestampPair>> events_in_storage;
  bool expected_is_fulfilled = false;
};
class PrivacySandboxNoticeFulfillmentParamTest
    : public PrivacySandboxNoticeFulfillmentTestBase,
      public testing::WithParamInterface<FulfillmentTestParam> {
 protected:
  std::unique_ptr<Notice> CreateNoticeUnderTest() {
    const FulfillmentTestParam& param = GetParam();
    NoticeId id_to_use = (param.notice_type_to_test == kTestNotice)
                             ? kTestNoticeId
                             : kTestConsentId;
    std::unique_ptr<Notice> notice_obj;
    if (param.notice_type_to_test == kTestNotice) {
      notice_obj = std::make_unique<Notice>(id_to_use);
    } else {
      notice_obj = std::make_unique<Consent>(id_to_use);
    }
    notice_obj->SetFeature(&kTestFeatureA);
    return notice_obj;
  }
};
TEST_P(PrivacySandboxNoticeFulfillmentParamTest,
       RefreshStatusAndCheckFulfilled) {
  const FulfillmentTestParam& param = GetParam();
  std::unique_ptr<Notice> notice = CreateNoticeUnderTest();
  if (param.events_in_storage.has_value()) {
    NoticeStorageData storage_data =
        CreateNoticeStorageDataWithEvents(param.events_in_storage.value());
    EXPECT_CALL(mock_storage_, ReadNoticeData(StrEq(kTestFeatureA.name)))
        .WillOnce(Return(std::move(storage_data)));
  } else {
    EXPECT_CALL(mock_storage_, ReadNoticeData(StrEq(kTestFeatureA.name)))
        .WillOnce(Return(std::nullopt));
  }
  notice->RefreshFulfillmentStatus(mock_storage_);
  EXPECT_EQ(notice->was_fulfilled(), param.expected_is_fulfilled);
}

constexpr base::Time kTimeT1 = base::Time::FromTimeT(1000);
constexpr base::Time kTimeT2 = base::Time::FromTimeT(2000);
constexpr base::Time kTimeT3 = base::Time::FromTimeT(3000);
std::vector<FulfillmentTestParam> GetFulfillmentTestParams() {
  return {
      // Notice
      FulfillmentTestParam{kTestNotice, std::nullopt, false},
      FulfillmentTestParam{kTestNotice, {{}}, false},
      FulfillmentTestParam{kTestNotice, {{{kShown, kTimeT1}}}, false},
      FulfillmentTestParam{
          kTestNotice, {{{kShown, kTimeT1}, {kAck, kTimeT2}}}, true},
      FulfillmentTestParam{
          kTestNotice, {{{kShown, kTimeT1}, {kSettings, kTimeT2}}}, true},
      FulfillmentTestParam{
          kTestNotice, {{{kAck, kTimeT1}, {kShown, kTimeT2}}}, true},
      // Consent
      FulfillmentTestParam{kTestConsent, std::nullopt, false},
      FulfillmentTestParam{kTestConsent, {{}}, false},
      FulfillmentTestParam{kTestConsent, {{{kShown, kTimeT1}}}, false},
      FulfillmentTestParam{
          kTestConsent, {{{kShown, kTimeT1}, {kOptIn, kTimeT2}}}, true},
      FulfillmentTestParam{
          kTestConsent, {{{kShown, kTimeT1}, {kOptOut, kTimeT2}}}, true},
      FulfillmentTestParam{
          kTestConsent, {{{kOptIn, kTimeT1}, {kOptOut, kTimeT2}}}, true},
      FulfillmentTestParam{
          kTestConsent, {{{kOptIn, kTimeT1}, {kShown, kTimeT2}}}, true},
      FulfillmentTestParam{
          kTestConsent,
          {{{kShown, kTimeT1}, {kOptOut, kTimeT2}, {kOptIn, kTimeT3}}},
          true},
  };
}

INSTANTIATE_TEST_SUITE_P(PrivacySandboxNoticeFulfillmentParamTest,
                         PrivacySandboxNoticeFulfillmentParamTest,
                         testing::ValuesIn(GetFulfillmentTestParams()));

//-----------------------------------------------------------------------------
// NoticeApi Tests
//-----------------------------------------------------------------------------

class PrivacySandboxNoticeApiTest : public testing::Test {
 protected:
  NoticeStorageData CreateStorageData(PrivacySandboxNoticeEvent event) {
    NoticeStorageData data;
    data.notice_events.emplace_back(std::make_unique<NoticeEventTimestampPair>(
        NoticeEventTimestampPair{event, base::Time::Now()}));
    return data;
  }

  NoticeApi api_;
  std::unique_ptr<Notice> notice_ = std::make_unique<Notice>(kTestNoticeId);
  std::unique_ptr<Consent> consent_ = std::make_unique<Consent>(kTestConsentId);
  StrictMock<MockCallback<base::RepeatingCallback<EligibilityLevel()>>>
      mock_eligibility_callback_;
  StrictMock<MockCallback<base::OnceCallback<void(bool)>>>
      mock_result_callback_;
  NiceMock<MockNoticeStorage> mock_storage_;
};

TEST_F(PrivacySandboxNoticeApiTest, InitialState) {
  EXPECT_THAT(api_.GetLinkedNotices(), IsEmpty());
  EXPECT_EQ(api_.GetEligibilityLevel(), kNotEligible);
  EXPECT_FALSE(api_.IsFulfilled());
}

TEST_F(PrivacySandboxNoticeApiTest, SetAndGetEligibilityCallback) {
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  EXPECT_CALL(mock_eligibility_callback_, Run())
      .Times(1)
      .WillOnce(Return(kEligibleNotice));
  EXPECT_EQ(api_.GetEligibilityLevel(), kEligibleNotice);
  Mock::VerifyAndClearExpectations(&mock_eligibility_callback_);
  EXPECT_CALL(mock_eligibility_callback_, Run()).Times(1);
  api_.IsFulfilled();
  Mock::VerifyAndClearExpectations(&mock_eligibility_callback_);
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
  notice_->SetFeature(&kTestFeatureA);
  EXPECT_CALL(mock_storage_, ReadNoticeData(StrEq(kTestFeatureA.name)))
      .WillOnce(Return(std::nullopt));
  notice_->RefreshFulfillmentStatus(mock_storage_);

  EXPECT_CALL(mock_eligibility_callback_, Run())
      .WillOnce(Return(kEligibleNotice));
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  api_.CanBeFulfilledBy(notice_.get());
  EXPECT_FALSE(api_.IsFulfilled());
}

TEST_F(PrivacySandboxNoticeApiTest,
       IsFulfilledEligibleNoticeWithFulfilledNotice) {
  notice_->SetFeature(&kTestFeatureA);
  EXPECT_CALL(mock_storage_, ReadNoticeData(StrEq(kTestFeatureA.name)))
      .WillOnce(Return(CreateStorageData(kAck)));
  notice_->RefreshFulfillmentStatus(mock_storage_);

  EXPECT_CALL(mock_eligibility_callback_, Run())
      .WillOnce(Return(kEligibleNotice));
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  api_.CanBeFulfilledBy(notice_.get());
  EXPECT_TRUE(api_.IsFulfilled());
}

TEST_F(PrivacySandboxNoticeApiTest,
       IsFulfilledEligibleConsentWithUnfulfilledConsent) {
  consent_->SetFeature(&kTestFeatureB);
  EXPECT_CALL(mock_storage_, ReadNoticeData(StrEq(kTestFeatureB.name)))
      .WillOnce(Return(std::nullopt));
  consent_->RefreshFulfillmentStatus(mock_storage_);

  EXPECT_CALL(mock_eligibility_callback_, Run())
      .WillOnce(Return(kEligibleConsent));
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  api_.CanBeFulfilledBy(consent_.get());
  EXPECT_FALSE(api_.IsFulfilled());
}

TEST_F(PrivacySandboxNoticeApiTest,
       IsFulfilledEligibleConsentWithFulfilledConsent) {
  consent_->SetFeature(&kTestFeatureB);
  EXPECT_CALL(mock_storage_, ReadNoticeData(StrEq(kTestFeatureB.name)))
      .WillOnce(Return(CreateStorageData(kOptIn)));
  consent_->RefreshFulfillmentStatus(mock_storage_);

  EXPECT_CALL(mock_eligibility_callback_, Run())
      .WillOnce(Return(kEligibleConsent));
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  api_.CanBeFulfilledBy(consent_.get());
  EXPECT_TRUE(api_.IsFulfilled());
}

TEST_F(PrivacySandboxNoticeApiTest,
       IsFulfilledEligibleConsentWithOnlyNoticeLinked) {
  notice_->SetFeature(&kTestFeatureA);
  EXPECT_CALL(mock_storage_, ReadNoticeData(StrEq(kTestFeatureA.name)))
      .WillOnce(Return(CreateStorageData(kAck)));
  notice_->RefreshFulfillmentStatus(mock_storage_);

  EXPECT_CALL(mock_eligibility_callback_, Run())
      .WillOnce(Return(kEligibleConsent));
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  api_.CanBeFulfilledBy(notice_.get());
  EXPECT_FALSE(api_.IsFulfilled());
}

TEST_F(PrivacySandboxNoticeApiTest,
       IsFulfilledEligibleNoticeWithMixedNoticesUnfulfilled) {
  consent_->SetFeature(&kTestFeatureB);
  EXPECT_CALL(mock_storage_, ReadNoticeData(StrEq(kTestFeatureB.name)))
      .WillOnce(Return(std::nullopt));
  consent_->RefreshFulfillmentStatus(mock_storage_);

  notice_->SetFeature(&kTestFeatureA);
  EXPECT_CALL(mock_storage_, ReadNoticeData(StrEq(kTestFeatureA.name)))
      .WillOnce(Return(std::nullopt));
  notice_->RefreshFulfillmentStatus(mock_storage_);

  EXPECT_CALL(mock_eligibility_callback_, Run())
      .WillOnce(Return(kEligibleNotice));
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  api_.CanBeFulfilledBy(consent_.get());
  api_.CanBeFulfilledBy(notice_.get());
  EXPECT_FALSE(api_.IsFulfilled());
}

TEST_F(PrivacySandboxNoticeApiTest,
       IsFulfilledEligibleNoticeWithMixedNoticesFirstFulfilled) {
  consent_->SetFeature(&kTestFeatureB);
  EXPECT_CALL(mock_storage_, ReadNoticeData(StrEq(kTestFeatureB.name)))
      .WillOnce(Return(CreateStorageData(kOptIn)));
  consent_->RefreshFulfillmentStatus(mock_storage_);

  notice_->SetFeature(&kTestFeatureA);

  EXPECT_CALL(mock_eligibility_callback_, Run())
      .WillOnce(Return(kEligibleNotice));
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  api_.CanBeFulfilledBy(consent_.get());
  api_.CanBeFulfilledBy(notice_.get());
  EXPECT_TRUE(api_.IsFulfilled());
}

TEST_F(PrivacySandboxNoticeApiTest,
       IsFulfilledEligibleConsentWithMixedNoticesConsentFulfilled) {
  notice_->SetFeature(&kTestFeatureA);
  EXPECT_CALL(mock_storage_, ReadNoticeData(StrEq(kTestFeatureA.name)))
      .WillOnce(Return(CreateStorageData(kAck)));
  notice_->RefreshFulfillmentStatus(mock_storage_);

  consent_->SetFeature(&kTestFeatureB);
  EXPECT_CALL(mock_storage_, ReadNoticeData(StrEq(kTestFeatureB.name)))
      .WillOnce(Return(CreateStorageData(kOptOut)));
  consent_->RefreshFulfillmentStatus(mock_storage_);

  EXPECT_CALL(mock_eligibility_callback_, Run())
      .WillOnce(Return(kEligibleConsent));
  api_.SetEligibilityCallback(mock_eligibility_callback_.Get());
  api_.CanBeFulfilledBy(notice_.get());
  api_.CanBeFulfilledBy(consent_.get());
  EXPECT_TRUE(api_.IsFulfilled());
}

}  // namespace
}  // namespace privacy_sandbox
