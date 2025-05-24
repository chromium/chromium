// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd/shared_crd_session_impl.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_remote_command_utils.h"
#include "chrome/browser/ash/policy/remote_commands/crd/fake_start_crd_session_job_delegate.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/crd_session_result_codes.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/shared_crd_session.h"
#include "chrome/browser/ash/policy/remote_commands/crd/start_crd_session_job_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::TestFuture;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::ReturnRef;

namespace policy {

namespace {

class SharedCrdSessionImplTest : public testing::Test {
 public:
  SharedCrdSessionImplTest() = default;

  void SetUp() override {
    shared_crd_session_ =
        std::make_unique<SharedCrdSessionImpl>(delegate_, robot_account_id_);
  }

  FakeStartCrdSessionJobDelegate& delegate() { return delegate_; }

 protected:
  std::unique_ptr<SharedCrdSessionImpl> shared_crd_session_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::string robot_account_id_ = "robot@account.com";
  FakeStartCrdSessionJobDelegate delegate_;
};

TEST_F(SharedCrdSessionImplTest, StartCrdHostShouldReturnAccessCode) {
  TestFuture<const std::string&> access_code_future;
  TestFuture<ExtendedStartCrdSessionResultCode, const std::string&>
      error_callback_future;
  TestFuture<void> session_finished_future;
  SharedCrdSession::SessionParameters input_parameters;
  input_parameters.viewer_email = "admin@email.com";
  input_parameters.allow_file_transfer = false;
  input_parameters.show_confirmation_dialog = false;
  input_parameters.terminate_upon_input = false;
  input_parameters.request_origin =
      SharedCrdSession::RequestOrigin::kEnterpriseAdmin;

  ASSERT_FALSE(delegate().HasActiveSession());
  shared_crd_session_->StartCrdHost(input_parameters,
                                    access_code_future.GetCallback(),
                                    error_callback_future.GetCallback(),
                                    session_finished_future.GetCallback());
  ASSERT_TRUE(access_code_future.Wait());

  auto result = access_code_future.Get();
  EXPECT_EQ(result, "111122223333");
}

TEST_F(SharedCrdSessionImplTest, StartCrdHostShouldStartSharedSession) {
  TestFuture<const std::string&> access_code_future;
  TestFuture<ExtendedStartCrdSessionResultCode, const std::string&>
      error_callback_future;
  TestFuture<void> session_finished_future;
  SharedCrdSession::SessionParameters input_parameters;
  input_parameters.viewer_email = "admin@email.com";
  input_parameters.allow_file_transfer = false;
  input_parameters.show_confirmation_dialog = false;
  input_parameters.terminate_upon_input = false;
  input_parameters.allow_remote_input = false;
  input_parameters.allow_clipboard_sync = false;
  input_parameters.request_origin =
      SharedCrdSession::RequestOrigin::kEnterpriseAdmin;

  ASSERT_FALSE(delegate().HasActiveSession());
  shared_crd_session_->StartCrdHost(input_parameters,
                                    access_code_future.GetCallback(),
                                    error_callback_future.GetCallback(),
                                    session_finished_future.GetCallback());
  ASSERT_TRUE(access_code_future.Wait());

  StartCrdSessionJobDelegate::SessionParameters output_parameters =
      delegate().session_parameters();
  ASSERT_EQ(input_parameters.viewer_email, output_parameters.admin_email);
  ASSERT_EQ(input_parameters.allow_file_transfer,
            output_parameters.allow_file_transfer);
  ASSERT_EQ(input_parameters.show_confirmation_dialog,
            output_parameters.show_confirmation_dialog);
  ASSERT_EQ(input_parameters.terminate_upon_input,
            output_parameters.terminate_upon_input);
  ASSERT_EQ(input_parameters.allow_remote_input,
            output_parameters.allow_remote_input);
  ASSERT_EQ(input_parameters.allow_clipboard_sync,
            output_parameters.allow_clipboard_sync);
  ASSERT_EQ(ConvertToStartCrdSessionJobDelegateRequestOrigin(
                input_parameters.request_origin),
            output_parameters.request_origin);
  ASSERT_EQ("robot@account.com", output_parameters.user_name);
}

TEST_F(SharedCrdSessionImplTest, StartCrdHostFailureShouldHaveErrorCode) {
  TestFuture<const std::string&> access_code_future;
  TestFuture<ExtendedStartCrdSessionResultCode, const std::string&>
      error_callback_future;
  TestFuture<void> session_finished_future;
  SharedCrdSession::SessionParameters input_parameters;
  input_parameters.request_origin =
      SharedCrdSession::RequestOrigin::kEnterpriseAdmin;

  delegate().FailWithError(
      ExtendedStartCrdSessionResultCode::kFailureCrdHostError);
  shared_crd_session_->StartCrdHost(input_parameters,
                                    access_code_future.GetCallback(),
                                    error_callback_future.GetCallback(),
                                    session_finished_future.GetCallback());
  ASSERT_TRUE(error_callback_future.Wait());

  EXPECT_EQ(error_callback_future.Get<ExtendedStartCrdSessionResultCode>(),
            ExtendedStartCrdSessionResultCode::kFailureCrdHostError);
}

TEST_F(SharedCrdSessionImplTest, RunsConsumerCallbackOnCrdSessionEnd) {
  TestFuture<const std::string&> access_code_future;
  TestFuture<ExtendedStartCrdSessionResultCode, const std::string&>
      error_callback_future;
  TestFuture<void> session_finished_future;
  SharedCrdSession::SessionParameters input_parameters;
  input_parameters.request_origin =
      SharedCrdSession::RequestOrigin::kEnterpriseAdmin;
  shared_crd_session_->StartCrdHost(input_parameters,
                                    access_code_future.GetCallback(),
                                    error_callback_future.GetCallback(),
                                    session_finished_future.GetCallback());

  delegate().TerminateCrdSession(base::Seconds(0));
  EXPECT_TRUE(session_finished_future.Wait());
}

}  // namespace
}  // namespace policy
