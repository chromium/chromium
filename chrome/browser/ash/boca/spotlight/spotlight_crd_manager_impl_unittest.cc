// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/spotlight/spotlight_crd_manager_impl.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/crd_session_result_codes.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/shared_crd_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::TestFuture;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace ash::boca {
namespace {
constexpr char kSpotlightConnectionCode[] = "123";
constexpr char kUserEmail[] = "cat@gmail.com";

class MockSharedCrdSession : public policy::SharedCrdSession {
 public:
  ~MockSharedCrdSession() override = default;

  MOCK_METHOD(void,
              StartCrdHost,
              (const SessionParameters& parameters,
               AccessCodeCallback success_callback,
               ErrorCallback error_callback),
              (override));
  MOCK_METHOD(void, TerminateSession, (), (override));
};

class SpotlightCrdManagerImplTest : public testing::Test {
 public:
  SpotlightCrdManagerImplTest() = default;
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({ash::features::kBocaSpotlight},
                                          /*disabled_features=*/{});
    auto crd_session = std::make_unique<NiceMock<MockSharedCrdSession>>();
    crd_session_ = crd_session.get();
    manager_ =
        std::make_unique<SpotlightCrdManagerImpl>(std::move(crd_session));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<SpotlightCrdManagerImpl> manager_;
  raw_ptr<NiceMock<MockSharedCrdSession>> crd_session_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SpotlightCrdManagerImplTest,
       InitiateSpotlightSessionShouldStartCrdHost) {
  EXPECT_CALL(*crd_session_, StartCrdHost)
      .WillOnce(WithArg<1>(
          Invoke([&](auto callback) { std::move(callback).Run("123"); })));
  TestFuture<const std::string&> success_future;

  manager_->OnSessionStarted(kUserEmail);
  manager_->InitiateSpotlightSession(success_future.GetCallback());
  ::testing::Mock::VerifyAndClearExpectations(crd_session_);

  EXPECT_EQ(kSpotlightConnectionCode, success_future.Get());
}

TEST_F(SpotlightCrdManagerImplTest,
       InitiateSpotlightSessionWithCrdFailureShouldRunErrorCallback) {
  TestFuture<void> error_callback_future;
  EXPECT_CALL(*crd_session_, StartCrdHost)
      .WillOnce(WithArg<2>(
          Invoke([&](auto callback) { error_callback_future.SetValue(); })));

  manager_->OnSessionStarted(kUserEmail);
  manager_->InitiateSpotlightSession(
      base::BindOnce([](const std::string& result) {
        GTEST_FAIL() << "Unexpected call to success callback";
      }));
  ::testing::Mock::VerifyAndClearExpectations(crd_session_);

  EXPECT_TRUE(error_callback_future.Wait());
}

TEST_F(SpotlightCrdManagerImplTest,
       InitiateSpotlightSessionShouldFailIfNotInActiveSession) {
  EXPECT_CALL(*crd_session_, StartCrdHost).Times(0);

  manager_->InitiateSpotlightSession(
      base::BindOnce([](const std::string& result) {
        GTEST_FAIL() << "Unexpected call to success callback";
      }));
  ::testing::Mock::VerifyAndClearExpectations(crd_session_);
}

TEST_F(SpotlightCrdManagerImplTest,
       OnSessionEndedShouldClearTeacherEmailAndTerminateSession) {
  EXPECT_CALL(*crd_session_, StartCrdHost).Times(1);
  EXPECT_CALL(*crd_session_, TerminateSession()).Times(1);
  TestFuture<const std::string&> success_future;

  manager_->OnSessionStarted(kUserEmail);
  manager_->InitiateSpotlightSession(success_future.GetCallback());

  manager_->OnSessionEnded();
  ::testing::Mock::VerifyAndClearExpectations(crd_session_);

  // Starting another session should end before calling crd since the
  // teacher_email_ is now be empty.

  EXPECT_CALL(*crd_session_, StartCrdHost).Times(0);
  manager_->InitiateSpotlightSession(
      base::BindOnce([](const std::string& result) {
        GTEST_FAIL() << "Unexpected call to success callback";
      }));
  ::testing::Mock::VerifyAndClearExpectations(crd_session_);
}

}  // namespace
}  // namespace ash::boca
