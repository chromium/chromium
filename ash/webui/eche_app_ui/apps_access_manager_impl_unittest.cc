// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/apps_access_manager_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/apps_access_setup_operation.h"
#include "ash/webui/eche_app_ui/fake_eche_connector.h"
#include "ash/webui/eche_app_ui/fake_eche_message_receiver.h"
#include "ash/webui/eche_app_ui/fake_feature_status_provider.h"
#include "ash/webui/eche_app_ui/pref_names.h"
#include "ash/webui/eche_app_ui/proto/exo_messages.pb.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace eche_app {

using AccessStatus =
    ash::phonehub::MultideviceFeatureAccessManager::AccessStatus;

using OnboardingUserActionMetric =
    AppsAccessManagerImpl::OnboardingUserActionMetric;

namespace {
class FakeObserver : public AppsAccessManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // AppsAccessManager::Observer:
  void OnAppsAccessChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

class FakeOperationDelegate : public AppsAccessSetupOperation::Delegate {
 public:
  FakeOperationDelegate() = default;
  ~FakeOperationDelegate() override = default;

  AppsAccessSetupOperation::Status status() const { return status_; }

  // AppsAccessSetupOperation::Delegate:
  void OnAppsStatusChange(
      AppsAccessSetupOperation::Status new_status) override {
    status_ = new_status;
  }

 private:
  AppsAccessSetupOperation::Status status_ =
      AppsAccessSetupOperation::Status::kConnecting;
};
}  // namespace

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;

class AppsAccessManagerImplTest : public testing::Test {
 protected:
  AppsAccessManagerImplTest() = default;
  AppsAccessManagerImplTest(const AppsAccessManagerImplTest&) = delete;
  AppsAccessManagerImplTest& operator=(const AppsAccessManagerImplTest&) =
      delete;
  ~AppsAccessManagerImplTest() override = default;

  void SetUp() override {
    AppsAccessManagerImpl::RegisterPrefs(pref_service_.registry());
    multidevice_setup::RegisterFeaturePrefs(pref_service_.registry());

    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA},
        /*disabled_features=*/{});

    fake_eche_connector_ = std::make_unique<FakeEcheConnector>();
    fake_eche_message_receiver_ = std::make_unique<FakeEcheMessageReceiver>();
    fake_feature_status_provider_ = std::make_unique<FakeFeatureStatusProvider>(
        FeatureStatus::kDependentFeature);
  }

  void TearDown() override {
    apps_access_manager_->RemoveObserver(&fake_observer_);
    apps_access_manager_.reset();
    fake_eche_connector_.reset();
    fake_eche_message_receiver_.reset();
    fake_feature_status_provider_.reset();
  }

  void Initialize(AccessStatus expected_status) {
    pref_service_.SetInteger(prefs::kAppsAccessStatus,
                             static_cast<int>(expected_status));
    apps_access_manager_ = std::make_unique<AppsAccessManagerImpl>(
        fake_eche_connector_.get(), fake_eche_message_receiver_.get(),
        fake_feature_status_provider_.get(), &pref_service_,
        &fake_multidevice_setup_client_, &fake_connection_manager_);
    apps_access_manager_->AddObserver(&fake_observer_);
  }

  void FakeGetAppsAccessStateResponse(eche_app::proto::Result result,
                                      eche_app::proto::AppsAccessState status) {
    fake_eche_message_receiver_->FakeGetAppsAccessStateResponse(result, status);
  }

  void FakeSendAppsSetupResponse(eche_app::proto::Result result,
                                 eche_app::proto::AppsAccessState status) {
    fake_eche_message_receiver_->FakeSendAppsSetupResponse(result, status);
  }

  void FakeSendAppsPolicyStateChange(
      eche_app::proto::AppStreamingPolicy app_policy_state) {
    fake_eche_message_receiver_->FakeAppPolicyStateChange(app_policy_state);
  }

  void SetFeatureStatus(FeatureStatus status) {
    fake_feature_status_provider_->SetStatus(status);
  }

  FeatureStatus GetFeatureStatus() {
    return fake_feature_status_provider_->GetStatus();
  }

  void SetConnectionStatus(secure_channel::ConnectionManager::Status status) {
    fake_connection_manager_.SetStatus(status);
  }

  void VerifyAppsAccessGrantedState(AccessStatus expected_status) {
    EXPECT_EQ(static_cast<int>(expected_status),
              pref_service_.GetInteger(prefs::kAppsAccessStatus));
    EXPECT_EQ(expected_status, apps_access_manager_->GetAccessStatus());
  }

  AppsAccessSetupOperation::Status GetAppsAccessSetupOperationStatus() {
    return fake_delegate_.status();
  }

  std::unique_ptr<AppsAccessSetupOperation> StartSetupOperation() {
    return apps_access_manager_->AttemptAppsAccessSetup(&fake_delegate_);
  }

  bool IsSetupOperationInProgress() {
    return apps_access_manager_->IsSetupOperationInProgress();
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

  size_t GetAppsSetupRequestCount() const {
    return fake_eche_connector_->send_apps_setup_request_count();
  }

  size_t GetAppsAccessStateRequestCount() const {
    return fake_eche_connector_->get_apps_access_state_request_count();
  }

  size_t GetAttemptNearbyConnectionCount() const {
    return fake_eche_connector_->attempt_nearby_connection_count();
  }

  void SetFeatureState(Feature feature, FeatureState feature_state) {
    fake_multidevice_setup_client_.SetFeatureState(feature, feature_state);
  }

  void NotifyAppsAccessCanceled() {
    return apps_access_manager_->NotifyAppsAccessCanceled();
  }

  void SetFeatureEnabledState(const std::string& pref_name, bool enabled) {
    pref_service_.SetBoolean(pref_name, enabled);
  }

  multidevice_setup::FakeMultiDeviceSetupClient*
  fake_multidevice_setup_client() {
    return &fake_multidevice_setup_client_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeObserver fake_observer_;
  FakeOperationDelegate fake_delegate_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<AppsAccessManager> apps_access_manager_;
  std::unique_ptr<FakeEcheConnector> fake_eche_connector_;
  std::unique_ptr<FakeEcheMessageReceiver> fake_eche_message_receiver_;
  std::unique_ptr<FakeFeatureStatusProvider> fake_feature_status_provider_;
  secure_channel::FakeConnectionManager fake_connection_manager_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
};

TEST_F(AppsAccessManagerImplTest, InitiallyGranted) {
  Initialize(AccessStatus::kAccessGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAccessGranted);

  // Cannot start the apps access setup flow if access has already been
  // granted.
  auto operation = StartSetupOperation();
  EXPECT_FALSE(operation);
}

TEST_F(AppsAccessManagerImplTest, OnFeatureStatusChanged) {
  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Set initial state to kIneligible.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  SetFeatureStatus(FeatureStatus::kIneligible);
  EXPECT_EQ(0u, GetAttemptNearbyConnectionCount());
  EXPECT_EQ(0u, GetAppsAccessStateRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnecting,
            GetAppsAccessSetupOperationStatus());

  //  Simulate feature status to be enabled and disconnected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  SetFeatureStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetAttemptNearbyConnectionCount());
  EXPECT_EQ(0u, GetAppsAccessStateRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnecting,
            GetAppsAccessSetupOperationStatus());

  // Simulate feature status to be enabled and connected. SetupOperation is also
  // not in progress, so expect no new requests to be sent.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  SetFeatureStatus(FeatureStatus::kConnected);
  EXPECT_EQ(1u, GetAppsAccessStateRequestCount());
  EXPECT_EQ(0u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnecting,
            GetAppsAccessSetupOperationStatus());

  // Simulate setup operation is in progress. This will trigger a sent request.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);
  EXPECT_EQ(1u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetAppsAccessSetupOperationStatus());
  EXPECT_TRUE(IsSetupOperationInProgress());

  // Set another feature status, expect status to be updated.
  SetFeatureStatus(FeatureStatus::kIneligible);
  SetFeatureStatus(FeatureStatus::kConnected);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);
  SetFeatureStatus(FeatureStatus::kConnecting);
  EXPECT_EQ(2u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnectionDisconnected,
            GetAppsAccessSetupOperationStatus());
  EXPECT_FALSE(IsSetupOperationInProgress());

  auto operation1 = StartSetupOperation();
  EXPECT_TRUE(operation1);
  EXPECT_EQ(2u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnecting,
            GetAppsAccessSetupOperationStatus());
  EXPECT_TRUE(IsSetupOperationInProgress());

  SetFeatureStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(2u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::kTimedOutConnecting,
            GetAppsAccessSetupOperationStatus());
  EXPECT_FALSE(IsSetupOperationInProgress());
}

TEST_F(AppsAccessManagerImplTest, StartDisconnectedAndNoAccess) {
  // Set initial state to disconnected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  SetFeatureStatus(FeatureStatus::kDisconnected);

  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled but disconnected status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);
  EXPECT_EQ(1u, GetAttemptNearbyConnectionCount());

  // Simulate changing states from connecting to connected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);
  SetFeatureStatus(FeatureStatus::kConnecting);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  SetFeatureStatus(FeatureStatus::kConnected);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetAppsAccessSetupOperationStatus());
  EXPECT_TRUE(IsSetupOperationInProgress());

  // Simulate getting a response back from the phone.
  FakeSendAppsSetupResponse(eche_app::proto::Result::RESULT_NO_ERROR,
                            eche_app::proto::AppsAccessState::ACCESS_GRANTED);
  VerifyAppsAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kCompletedSuccessfully,
            GetAppsAccessSetupOperationStatus());
  EXPECT_FALSE(IsSetupOperationInProgress());
}

TEST_F(AppsAccessManagerImplTest, StartConnectingAndNoAccess) {
  base::HistogramTester histograms;

  // Set initial state to connecting.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);
  SetFeatureStatus(FeatureStatus::kConnecting);

  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled and connecting status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate changing states from connecting to connected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  SetFeatureStatus(FeatureStatus::kConnected);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetAppsAccessSetupOperationStatus());
  EXPECT_TRUE(IsSetupOperationInProgress());

  // Simulate getting a response back from the phone.
  FakeSendAppsSetupResponse(eche_app::proto::Result::RESULT_NO_ERROR,
                            eche_app::proto::AppsAccessState::ACCESS_GRANTED);
  VerifyAppsAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kCompletedSuccessfully,
            GetAppsAccessSetupOperationStatus());
  EXPECT_FALSE(IsSetupOperationInProgress());
  histograms.ExpectBucketCount(
      kEcheOnboardingHistogramName,
      OnboardingUserActionMetric::kUserActionPermissionGranted, 1);
}

TEST_F(AppsAccessManagerImplTest, StartConnectedAndNoAccess) {
  // Set initial state to connected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  SetFeatureStatus(FeatureStatus::kConnected);

  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled and connected status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetAppsAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  FakeSendAppsSetupResponse(eche_app::proto::Result::RESULT_NO_ERROR,
                            eche_app::proto::AppsAccessState::ACCESS_GRANTED);
  VerifyAppsAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kCompletedSuccessfully,
            GetAppsAccessSetupOperationStatus());
  EXPECT_FALSE(IsSetupOperationInProgress());
}

TEST_F(AppsAccessManagerImplTest, SimulateUserRejectedError) {
  base::HistogramTester histograms;

  // Set initial state to connected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  SetFeatureStatus(FeatureStatus::kConnected);

  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled and connected status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetAppsAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  FakeSendAppsSetupResponse(
      eche_app::proto::Result::RESULT_ERROR_USER_REJECTED,
      eche_app::proto::AppsAccessState::ACCESS_NOT_GRANTED);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kCompletedUserRejected,
            GetAppsAccessSetupOperationStatus());
  EXPECT_FALSE(IsSetupOperationInProgress());
  histograms.ExpectBucketCount(
      kEcheOnboardingHistogramName,
      OnboardingUserActionMetric::kUserActionPermissionRejected, 1);
}

TEST_F(AppsAccessManagerImplTest, SimulateReceivesAppsSetupAck) {
  base::HistogramTester histograms;

  // Set initial state to connected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  SetFeatureStatus(FeatureStatus::kConnected);

  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled and connected status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetAppsAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  FakeSendAppsSetupResponse(
      eche_app::proto::Result::RESULT_ACK_BY_EXO,
      eche_app::proto::AppsAccessState::ACCESS_NOT_GRANTED);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  EXPECT_TRUE(IsSetupOperationInProgress());
  histograms.ExpectBucketCount(kEcheOnboardingHistogramName,
                               OnboardingUserActionMetric::kAckByExo, 1);
}

TEST_F(AppsAccessManagerImplTest, SimulateOperationFailedOrCanceled) {
  base::HistogramTester histograms;

  // Set initial state to connected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  SetFeatureStatus(FeatureStatus::kConnected);

  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled and connected status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetAppsAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  FakeSendAppsSetupResponse(
      eche_app::proto::Result::RESULT_ERROR_ACTION_TIMEOUT,
      eche_app::proto::AppsAccessState::ACCESS_NOT_GRANTED);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kOperationFailedOrCancelled,
            GetAppsAccessSetupOperationStatus());
  EXPECT_FALSE(IsSetupOperationInProgress());
  histograms.ExpectBucketCount(kEcheOnboardingHistogramName,
                               OnboardingUserActionMetric::kUserActionTimeout,
                               1);

  // Simulate flipping the access state to not granted.
  FakeGetAppsAccessStateResponse(
      eche_app::proto::Result::RESULT_NO_ERROR,
      eche_app::proto::AppsAccessState::ACCESS_NOT_GRANTED);

  // Start a setup operation with enabled and connected status and access
  // not granted.
  auto operation1 = StartSetupOperation();
  EXPECT_TRUE(operation1);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(2u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetAppsAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  FakeSendAppsSetupResponse(
      eche_app::proto::Result::RESULT_ERROR_ACTION_CANCELED,
      eche_app::proto::AppsAccessState::ACCESS_NOT_GRANTED);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kOperationFailedOrCancelled,
            GetAppsAccessSetupOperationStatus());
  EXPECT_FALSE(IsSetupOperationInProgress());
  histograms.ExpectBucketCount(
      kEcheOnboardingHistogramName,
      OnboardingUserActionMetric::kUserActionRemoteInterrupt, 1);

  // Simulate flipping the access state to not granted.
  FakeGetAppsAccessStateResponse(
      eche_app::proto::Result::RESULT_NO_ERROR,
      eche_app::proto::AppsAccessState::ACCESS_NOT_GRANTED);

  // Start a setup operation with enabled and connected status and access
  // not granted.
  auto operation2 = StartSetupOperation();
  EXPECT_TRUE(operation2);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(3u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetAppsAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  FakeSendAppsSetupResponse(
      eche_app::proto::Result::RESULT_ERROR_SYSTEM,
      eche_app::proto::AppsAccessState::ACCESS_NOT_GRANTED);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kOperationFailedOrCancelled,
            GetAppsAccessSetupOperationStatus());
  EXPECT_FALSE(IsSetupOperationInProgress());
  histograms.ExpectBucketCount(kEcheOnboardingHistogramName,
                               OnboardingUserActionMetric::kSystemError, 1);
}

TEST_F(AppsAccessManagerImplTest, SimulateConnectingToDisconnected) {
  // Set initial state to disconnected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  SetFeatureStatus(FeatureStatus::kDisconnected);
  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate a disconnection and expect that status has been updated.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);
  SetFeatureStatus(FeatureStatus::kConnecting);
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  SetFeatureStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kTimedOutConnecting,
            GetAppsAccessSetupOperationStatus());
  EXPECT_FALSE(IsSetupOperationInProgress());
}

TEST_F(AppsAccessManagerImplTest, SimulateConnectedToDisconnected) {
  // Set initial state to disconnected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  SetFeatureStatus(FeatureStatus::kDisconnected);
  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate connected state.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  SetFeatureStatus(FeatureStatus::kConnected);
  EXPECT_EQ(1u, GetAppsSetupRequestCount());

  // Simulate a disconnection, expect status update.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  SetFeatureStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnectionDisconnected,
            GetAppsAccessSetupOperationStatus());
  EXPECT_FALSE(IsSetupOperationInProgress());
}

TEST_F(AppsAccessManagerImplTest, OnConnectionStatusChanged) {
  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Simulate disabling the feature and connection is disconnected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  SetFeatureStatus(FeatureStatus::kDisabled);

  EXPECT_EQ(1u, GetAttemptNearbyConnectionCount());
  EXPECT_EQ(0u, GetAppsAccessStateRequestCount());

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  EXPECT_EQ(1u, GetAppsAccessStateRequestCount());

  // Simulate flipping the access state to granted.
  FakeGetAppsAccessStateResponse(
      eche_app::proto::Result::RESULT_NO_ERROR,
      eche_app::proto::AppsAccessState::ACCESS_GRANTED);

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  EXPECT_EQ(1u, GetAttemptNearbyConnectionCount());
}

TEST_F(AppsAccessManagerImplTest,
       SimulateDisabledWithDifferentConnectionStatus) {
  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Simulate disabling the feature and connection is disconnected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  SetFeatureStatus(FeatureStatus::kDisabled);

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnecting,
            GetAppsAccessSetupOperationStatus());
  EXPECT_EQ(2u, GetAttemptNearbyConnectionCount());
  EXPECT_EQ(0u, GetAppsSetupRequestCount());

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnecting,
            GetAppsAccessSetupOperationStatus());
  EXPECT_EQ(0u, GetAppsSetupRequestCount());

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  EXPECT_EQ(AppsAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetAppsAccessSetupOperationStatus());
  EXPECT_EQ(1u, GetAppsSetupRequestCount());

  // Simulate getting a response back from the phone.
  FakeSendAppsSetupResponse(eche_app::proto::Result::RESULT_NO_ERROR,
                            eche_app::proto::AppsAccessState::ACCESS_GRANTED);
  VerifyAppsAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kCompletedSuccessfully,
            GetAppsAccessSetupOperationStatus());
}

TEST_F(AppsAccessManagerImplTest, SimulateConnectedToDependentFeature) {
  // Set initial state to disconnected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  SetFeatureStatus(FeatureStatus::kDisconnected);
  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate connected state.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  SetFeatureStatus(FeatureStatus::kConnected);
  EXPECT_EQ(1u, GetAppsSetupRequestCount());

  // Simulate disabling the feature, expect status update.
  SetFeatureStatus(FeatureStatus::kDependentFeature);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnectionDisconnected,
            GetAppsAccessSetupOperationStatus());
}

TEST_F(AppsAccessManagerImplTest, SimulateConnectedToDependentFeaturePending) {
  // Set initial state to disconnected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  SetFeatureStatus(FeatureStatus::kDisconnected);
  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate connected state.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  SetFeatureStatus(FeatureStatus::kConnected);
  EXPECT_EQ(1u, GetAppsSetupRequestCount());

  // Simulate disabling the feature, expect status update.
  SetFeatureStatus(FeatureStatus::kDependentFeaturePending);
  EXPECT_EQ(AppsAccessSetupOperation::Status::kConnectionDisconnected,
            GetAppsAccessSetupOperationStatus());
}

TEST_F(AppsAccessManagerImplTest, FlipAccessNotGrantedToGranted) {
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);
  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Simulate flipping the access state to granted.
  FakeGetAppsAccessStateResponse(
      eche_app::proto::Result::RESULT_NO_ERROR,
      eche_app::proto::AppsAccessState::ACCESS_GRANTED);

  VerifyAppsAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(1u, GetNumObserverCalls());

  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kEche,
      /*expected_enabled=*/true, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(AppsAccessManagerImplTest, FlipAccessGrantedToNotGranted) {
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);
  SetFeatureState(Feature::kEche, FeatureState::kDisabledByUser);
  Initialize(AccessStatus::kAccessGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAccessGranted);

  // Simulate flipping the access state to not granted.
  FakeGetAppsAccessStateResponse(
      eche_app::proto::Result::RESULT_NO_ERROR,
      eche_app::proto::AppsAccessState::ACCESS_NOT_GRANTED);

  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(AppsAccessManagerImplTest, AccessNotChanged) {
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);
  SetFeatureState(Feature::kEche, FeatureState::kEnabledByUser);
  Initialize(AccessStatus::kAccessGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAccessGranted);

  // Simulate flipping the access state granted.
  FakeGetAppsAccessStateResponse(
      eche_app::proto::Result::RESULT_NO_ERROR,
      eche_app::proto::AppsAccessState::ACCESS_GRANTED);

  VerifyAppsAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(0u, GetNumObserverCalls());

  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kEche,
      /*expected_enabled=*/true, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(AppsAccessManagerImplTest,
       ShouleNotEnableEcheFeatureWhenPhoneHubIsDisabled) {
  // Explicitly disable Phone Hub, all sub feature should be disabled
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);
  SetFeatureState(Feature::kEche, FeatureState::kDisabledByUser);
  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // No action after access is granted
  // Simulate flipping the access state granted.
  FakeGetAppsAccessStateResponse(
      eche_app::proto::Result::RESULT_NO_ERROR,
      eche_app::proto::AppsAccessState::ACCESS_GRANTED);

  EXPECT_EQ(
      0u,
      fake_multidevice_setup_client()->NumPendingSetFeatureEnabledStateCalls());
}

TEST_F(AppsAccessManagerImplTest, InitiallyEnableApps) {
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);
  Initialize(AccessStatus::kAvailableButNotGranted);

  // Simulate flipping the access state granted.
  FakeGetAppsAccessStateResponse(
      eche_app::proto::Result::RESULT_NO_ERROR,
      eche_app::proto::AppsAccessState::ACCESS_GRANTED);

  // If the kEche feature has not been explicitly set yet, enable it
  // when Phone Hub is enabled and access has been granted.
  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kEche,
      /*expected_enabled=*/true, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(AppsAccessManagerImplTest,
       SimulateAccessNotGrantedShouleDisableEcheFeature) {
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);
  SetFeatureState(Feature::kEche, FeatureState::kEnabledByUser);
  Initialize(AccessStatus::kAccessGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAccessGranted);

  // Test that there is a call to disable kEche when apps access has been
  // revoked. Simulate flipping the access state to not granted.
  FakeGetAppsAccessStateResponse(
      eche_app::proto::Result::RESULT_NO_ERROR,
      eche_app::proto::AppsAccessState::ACCESS_NOT_GRANTED);

  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kEche,
      /*expected_enabled=*/false, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(AppsAccessManagerImplTest,
       InitiallyEnableEcheFeature_OnlyEnableFromDefaultState) {
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);
  Initialize(AccessStatus::kAccessGranted);

  // If the Eche feature has not been explicitly set yet, enable it
  // when Phone Hub is enabled and access has been granted.
  FakeGetAppsAccessStateResponse(
      eche_app::proto::Result::RESULT_NO_ERROR,
      eche_app::proto::AppsAccessState::ACCESS_GRANTED);

  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kEche,
      /*expected_enabled=*/true, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
}

TEST_F(AppsAccessManagerImplTest,
       ShouldNotEnableEcheFeatureIfFeatureIsNotDefaultState) {
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);

  // Simulate the Eche feature has been changed by user.
  SetFeatureEnabledState(ash::multidevice_setup::kEcheEnabledPrefName, false);
  Initialize(AccessStatus::kAccessGranted);

  // We take no action after access is granted because the Eche feature
  // state was already explicitly set; we respect the user's choice.
  FakeGetAppsAccessStateResponse(
      eche_app::proto::Result::RESULT_NO_ERROR,
      eche_app::proto::AppsAccessState::ACCESS_GRANTED);

  EXPECT_EQ(
      0u,
      fake_multidevice_setup_client()->NumPendingSetFeatureEnabledStateCalls());
}

TEST_F(AppsAccessManagerImplTest,
       ShouleNotEnableEcheFeatureWhenAppsPolicyDisabled) {
  // Explicitly disable Phone Hub, all sub feature should be disabled
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);
  SetFeatureState(Feature::kEche, FeatureState::kDisabledByUser);
  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Simulate flipping the policy state is disabled.
  FakeSendAppsPolicyStateChange(
      eche_app::proto::AppStreamingPolicy::APP_POLICY_DISABLED);
  // No action after access is granted.
  // Simulate flipping the access state granted.
  FakeGetAppsAccessStateResponse(
      eche_app::proto::Result::RESULT_NO_ERROR,
      eche_app::proto::AppsAccessState::ACCESS_GRANTED);

  EXPECT_EQ(
      0u,
      fake_multidevice_setup_client()->NumPendingSetFeatureEnabledStateCalls());
}

TEST_F(AppsAccessManagerImplTest,
       SimulateAppsPolicyDisabledShouldDisableEcheFeature) {
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);
  SetFeatureState(Feature::kEche, FeatureState::kEnabledByUser);
  Initialize(AccessStatus::kAccessGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAccessGranted);

  // Test that there is a call to disable kEche when apps polcy state has been
  // changed. Simulate flipping the policy state is disabled.
  FakeSendAppsPolicyStateChange(
      eche_app::proto::AppStreamingPolicy::APP_POLICY_DISABLED);

  // No action is taken until get apps access state response is received.
  EXPECT_EQ(0u, GetNumObserverCalls());

  FakeGetAppsAccessStateResponse(
      eche_app::proto::Result::RESULT_NO_ERROR,
      eche_app::proto::AppsAccessState::ACCESS_GRANTED);

  fake_multidevice_setup_client()->InvokePendingSetFeatureEnabledStateCallback(
      /*expected_feature=*/Feature::kEche,
      /*expected_enabled=*/false, /*expected_auth_token=*/std::nullopt,
      /*success=*/true);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(AppsAccessManagerImplTest, LogSetupCancelWhenAppsAccessCanceled) {
  base::HistogramTester histograms;

  // Set initial state to connecting.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);
  SetFeatureStatus(FeatureStatus::kConnecting);

  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled and connecting status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate changing states from connecting to connected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  SetFeatureStatus(FeatureStatus::kConnected);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetAppsAccessSetupOperationStatus());
  EXPECT_TRUE(IsSetupOperationInProgress());

  // Notify the apps access setup operation is canceled.
  NotifyAppsAccessCanceled();

  // Verify the metric logs the expected event.
  histograms.ExpectBucketCount(kEcheOnboardingHistogramName,
                               OnboardingUserActionMetric::kUserActionCanceled,
                               1);
}

TEST_F(AppsAccessManagerImplTest,
       LogFailConnectionWhenCanceledAndDisconnected) {
  base::HistogramTester histograms;

  // Set initial state to connecting.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);
  SetFeatureStatus(FeatureStatus::kConnecting);

  Initialize(AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled and connecting status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate changing states from connecting to connected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  SetFeatureStatus(FeatureStatus::kConnected);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetAppsSetupRequestCount());
  EXPECT_EQ(AppsAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetAppsAccessSetupOperationStatus());
  EXPECT_TRUE(IsSetupOperationInProgress());

  // Simulate changing states from connected to disconnected.
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  SetFeatureStatus(FeatureStatus::kDisconnected);

  // Notify the apps access setup operation is canceled.
  NotifyAppsAccessCanceled();

  // Verify the metric logs the expected event.
  histograms.ExpectBucketCount(kEcheOnboardingHistogramName,
                               OnboardingUserActionMetric::kFailedConnection,
                               1);
}

}  // namespace eche_app
}  // namespace ash
