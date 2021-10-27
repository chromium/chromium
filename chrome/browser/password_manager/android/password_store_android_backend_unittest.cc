// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend.h"

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::Return;
using testing::StrictMock;
using testing::WithArg;
using JobId = PasswordStoreAndroidBackendBridge::JobId;

std::vector<std::unique_ptr<PasswordForm>> CreateTestLogins() {
  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(CreateEntry("Todd Tester", "S3cr3t",
                              GURL(u"https://example.com"),
                              /*psl=*/false,
                              /*affiliation=*/false));
  forms.push_back(CreateEntry("Marcus McSpartanGregor", "S0m3th1ngCr34t1v3",
                              GURL(u"https://m.example.com"), /*psl=*/true,
                              /*affiliation=*/false));
  return forms;
}

std::vector<PasswordForm> UnwrapForms(
    std::vector<std::unique_ptr<PasswordForm>> password_ptrs) {
  std::vector<PasswordForm> forms;
  forms.reserve(password_ptrs.size());
  for (auto& password : password_ptrs) {
    forms.push_back(std::move(*password));
  }
  return forms;
}

class MockPasswordStoreAndroidBackendBridge
    : public PasswordStoreAndroidBackendBridge {
 public:
  MOCK_METHOD(void, SetConsumer, (base::WeakPtr<Consumer>), (override));
  MOCK_METHOD(JobId, GetAllLogins, (), (override));
};

}  // namespace

class PasswordStoreAndroidBackendTest : public testing::Test {
 protected:
  PasswordStoreAndroidBackendTest() {
    backend_ =
        std::make_unique<PasswordStoreAndroidBackend>(CreateMockBridge());
  }

  ~PasswordStoreAndroidBackendTest() override {
    testing::Mock::VerifyAndClearExpectations(bridge_);
  }

  PasswordStoreBackend& backend() { return *backend_; }
  PasswordStoreAndroidBackendBridge::Consumer& consumer() { return *backend_; }
  MockPasswordStoreAndroidBackendBridge* bridge() { return bridge_; }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<PasswordStoreAndroidBackendBridge> CreateMockBridge() {
    auto unique_bridge =
        std::make_unique<StrictMock<MockPasswordStoreAndroidBackendBridge>>();
    bridge_ = unique_bridge.get();
    EXPECT_CALL(*bridge_, SetConsumer);
    return unique_bridge;
  }

  std::unique_ptr<PasswordStoreAndroidBackend> backend_;
  StrictMock<MockPasswordStoreAndroidBackendBridge>* bridge_;
};

TEST_F(PasswordStoreAndroidBackendTest, CallsCompletionCallbackAfterInit) {
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;
  EXPECT_CALL(completion_callback, Run(true));
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), completion_callback.Get());
}

TEST_F(PasswordStoreAndroidBackendTest, CallsBridgeForLogins) {
  backend().InitBackend(PasswordStoreAndroidBackend::RemoteChangesReceived(),
                        base::RepeatingClosure(), base::DoNothing());
  const JobId kJobId{1337};
  base::MockCallback<LoginsReply> mock_reply;
  EXPECT_CALL(*bridge(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());

  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply,
              Run(UnorderedPasswordFormElementsAre(&expected_logins)));
  consumer().OnCompleteWithLogins(kJobId, UnwrapForms(CreateTestLogins()));
  RunUntilIdle();
}

class PasswordStoreAndroidBackendTestForMetrics
    : public PasswordStoreAndroidBackendTest,
      public testing::WithParamInterface<bool> {
 public:
  bool ShouldSucceed() const { return GetParam(); }
};

// Tests the PasswordManager.PasswordStore.GetAllLoginsAsync metric.
TEST_P(PasswordStoreAndroidBackendTestForMetrics, GetAllLoginsAsyncMetrics) {
  backend().InitBackend(
      PasswordStoreAndroidBackend::RemoteChangesReceived(),
      /*sync_enabled_or_disabled_cb=*/base::RepeatingClosure(),
      /*completion=*/base::DoNothing());
  constexpr auto kLatencyDelta = base::Milliseconds(123u);
  constexpr JobId kJobId{1337};
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.GetAllLoginsAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreAndroidBackend.GetAllLoginsAsync.Success";
  base::HistogramTester histogram_tester;
  base::MockCallback<LoginsReply> mock_reply;
  EXPECT_CALL(*bridge(), GetAllLogins).WillOnce(Return(kJobId));
  backend().GetAllLoginsAsync(mock_reply.Get());
  EXPECT_CALL(mock_reply, Run(_)).Times(ShouldSucceed() ? 1 : 0);
  task_environment_.FastForwardBy(kLatencyDelta);
  if (ShouldSucceed())
    consumer().OnCompleteWithLogins(kJobId, {});
  else
    consumer().OnError(kJobId);
  RunUntilIdle();
  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, ShouldSucceed());
  histogram_tester.ExpectBucketCount(kSuccessMetric, false, !ShouldSucceed());
}

INSTANTIATE_TEST_SUITE_P(,
                         PasswordStoreAndroidBackendTestForMetrics,
                         testing::Bool());

}  // namespace password_manager
