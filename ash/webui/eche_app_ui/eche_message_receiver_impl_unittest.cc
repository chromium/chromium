// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_message_receiver_impl.h"

#include "ash/webui/eche_app_ui/proto/exo_messages.pb.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace eche_app {

namespace {
class FakeObserver : public EcheMessageReceiver::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t apps_access_state_response_num_calls() const {
    return apps_access_state_response_num_calls_;
  }

  size_t apps_setup_response_num_calls() const { return apps_setup_response_; }

  size_t status_change_num_calls() const { return status_change_num_calls_; }

  size_t apps_policy_state_change_num_calls() const {
    return apps_policy_state_change_num_calls_;
  }

  proto::GetAppsAccessStateResponse get_last_apps_access_state() const {
    return last_apps_access_state_response_;
  }

  proto::SendAppsSetupResponse get_last_apps_setup_response() const {
    return last_apps_setup_reponse_;
  }

  proto::StatusChangeType get_last_status_change_type() const {
    return last_status_change_type_;
  }

  proto::AppStreamingPolicy get_last_apps_policy_state() const {
    return last_apps_policy_state_;
  }

  // EcheMessageReceiver::Observer:
  void OnGetAppsAccessStateResponseReceived(
      proto::GetAppsAccessStateResponse apps_access_state_response) override {
    last_apps_access_state_response_ = apps_access_state_response;
    ++apps_access_state_response_num_calls_;
  }
  void OnSendAppsSetupResponseReceived(
      proto::SendAppsSetupResponse apps_setup_response) override {
    last_apps_setup_reponse_ = apps_setup_response;
    ++apps_setup_response_;
  }
  void OnStatusChange(proto::StatusChangeType status_change_type) override {
    last_status_change_type_ = status_change_type;
    ++status_change_num_calls_;
  }

  void OnAppPolicyStateChange(
      proto::AppStreamingPolicy app_policy_state) override {
    last_apps_policy_state_ = app_policy_state;
    ++apps_policy_state_change_num_calls_;
  }

 private:
  size_t apps_access_state_response_num_calls_ = 0;
  size_t apps_setup_response_ = 0;
  size_t status_change_num_calls_ = 0;
  size_t apps_policy_state_change_num_calls_ = 0;
  proto::GetAppsAccessStateResponse last_apps_access_state_response_;
  proto::SendAppsSetupResponse last_apps_setup_reponse_;
  proto::StatusChangeType last_status_change_type_;
  proto::AppStreamingPolicy last_apps_policy_state_;
};
}  // namespace

class EcheMessageReceiverImplTest : public testing::Test {
 protected:
  EcheMessageReceiverImplTest()
      : fake_connection_manager_(
            std::make_unique<secure_channel::FakeConnectionManager>()) {}
  EcheMessageReceiverImplTest(const EcheMessageReceiverImplTest&) = delete;
  EcheMessageReceiverImplTest& operator=(const EcheMessageReceiverImplTest&) =
      delete;
  ~EcheMessageReceiverImplTest() override = default;

  void SetUp() override {
    message_receiver_ = std::make_unique<EcheMessageReceiverImpl>(
        fake_connection_manager_.get());
    message_receiver_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    message_receiver_->RemoveObserver(&fake_observer_);
    message_receiver_.reset();
  }

  size_t GetNumAppsAccessStateResponseCalls() const {
    return fake_observer_.apps_access_state_response_num_calls();
  }

  size_t GetNumAppsSetupResponseCalls() const {
    return fake_observer_.apps_setup_response_num_calls();
  }

  size_t GetNumStatusChangeCalls() const {
    return fake_observer_.status_change_num_calls();
  }

  size_t GetNumAppsPolicyStateChangeCalls() const {
    return fake_observer_.apps_policy_state_change_num_calls();
  }

  proto::GetAppsAccessStateResponse GetLastAppsAccessState() const {
    return fake_observer_.get_last_apps_access_state();
  }

  proto::SendAppsSetupResponse GetLastAppsSetupResponse() const {
    return fake_observer_.get_last_apps_setup_response();
  }

  proto::StatusChangeType GetLastStatusChangeType() const {
    return fake_observer_.get_last_status_change_type();
  }

  proto::AppStreamingPolicy GetAppStreamingPolicyState() const {
    return fake_observer_.get_last_apps_policy_state();
  }

  FakeObserver fake_observer_;
  std::unique_ptr<secure_channel::FakeConnectionManager>
      fake_connection_manager_;
  std::unique_ptr<EcheMessageReceiver> message_receiver_;
};

TEST_F(EcheMessageReceiverImplTest, OnGetAppsAccessStateResponseReceived) {
  proto::GetAppsAccessStateResponse response;
  response.set_result(eche_app::proto::Result::RESULT_NO_ERROR);
  response.set_apps_access_state(
      eche_app::proto::AppsAccessState::ACCESS_GRANTED);
  proto::ExoMessage message;
  *message.mutable_apps_access_state_response() = std::move(response);

  fake_connection_manager_->NotifyMessageReceived(message.SerializeAsString());

  proto::GetAppsAccessStateResponse actual_apps_state =
      GetLastAppsAccessState();

  EXPECT_EQ(1u, GetNumAppsAccessStateResponseCalls());
  EXPECT_EQ(0u, GetNumAppsSetupResponseCalls());
  EXPECT_EQ(0u, GetNumStatusChangeCalls());
  EXPECT_EQ(0u, GetNumAppsPolicyStateChangeCalls());
  EXPECT_EQ(eche_app::proto::Result::RESULT_NO_ERROR,
            actual_apps_state.result());
  EXPECT_EQ(eche_app::proto::AppsAccessState::ACCESS_GRANTED,
            actual_apps_state.apps_access_state());
}

TEST_F(EcheMessageReceiverImplTest, OnSendAppsSetupResponseReceived) {
  proto::SendAppsSetupResponse response;
  response.set_result(eche_app::proto::Result::RESULT_NO_ERROR);
  response.set_apps_access_state(
      eche_app::proto::AppsAccessState::ACCESS_GRANTED);
  proto::ExoMessage message;
  *message.mutable_apps_setup_response() = std::move(response);

  fake_connection_manager_->NotifyMessageReceived(message.SerializeAsString());

  proto::SendAppsSetupResponse actual_apps_setup_response =
      GetLastAppsSetupResponse();

  EXPECT_EQ(0u, GetNumAppsAccessStateResponseCalls());
  EXPECT_EQ(1u, GetNumAppsSetupResponseCalls());
  EXPECT_EQ(0u, GetNumStatusChangeCalls());
  EXPECT_EQ(0u, GetNumAppsPolicyStateChangeCalls());
  EXPECT_EQ(eche_app::proto::Result::RESULT_NO_ERROR,
            actual_apps_setup_response.result());
  EXPECT_EQ(eche_app::proto::AppsAccessState::ACCESS_GRANTED,
            actual_apps_setup_response.apps_access_state());
}

TEST_F(EcheMessageReceiverImplTest, OnStatusChangeReceived) {
  proto::StatusChange status_change;
  status_change.set_type(proto::StatusChangeType::TYPE_STREAM_START);
  proto::ExoMessage message;
  *message.mutable_status_change() = std::move(status_change);

  fake_connection_manager_->NotifyMessageReceived(message.SerializeAsString());

  proto::StatusChangeType status_change_type = GetLastStatusChangeType();

  EXPECT_EQ(0u, GetNumAppsAccessStateResponseCalls());
  EXPECT_EQ(0u, GetNumAppsSetupResponseCalls());
  EXPECT_EQ(1u, GetNumStatusChangeCalls());
  EXPECT_EQ(0u, GetNumAppsPolicyStateChangeCalls());
  EXPECT_EQ(proto::StatusChangeType::TYPE_STREAM_START, status_change_type);
}

TEST_F(EcheMessageReceiverImplTest, OnAppPolicyStateChangeReceived) {
  proto::PolicyStateChange policy_state_change;
  policy_state_change.set_app_policy_state(
      proto::AppStreamingPolicy::APP_POLICY_DISABLED);
  proto::ExoMessage message;
  *message.mutable_policy_state_change() = std::move(policy_state_change);

  fake_connection_manager_->NotifyMessageReceived(message.SerializeAsString());

  proto::AppStreamingPolicy app_policy_state = GetAppStreamingPolicyState();

  EXPECT_EQ(0u, GetNumAppsAccessStateResponseCalls());
  EXPECT_EQ(0u, GetNumAppsSetupResponseCalls());
  EXPECT_EQ(0u, GetNumStatusChangeCalls());
  EXPECT_EQ(1u, GetNumAppsPolicyStateChangeCalls());
  EXPECT_EQ(proto::AppStreamingPolicy::APP_POLICY_DISABLED, app_policy_state);
}

}  // namespace eche_app
}  // namespace ash
