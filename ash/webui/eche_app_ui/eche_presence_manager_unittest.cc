// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_presence_manager.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/fake_eche_connector.h"
#include "ash/webui/eche_app_ui/fake_eche_message_receiver.h"
#include "ash/webui/eche_app_ui/fake_feature_status_provider.h"
#include "ash/webui/eche_app_ui/proto/exo_messages.pb.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/presence_monitor_client_impl.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace eche_app {

static size_t num_start_monitor_calls_ = 0;
static size_t num_stop_monitor_calls_ = 0;

namespace {

class FakePresenceMonitorClient : public secure_channel::PresenceMonitorClient {
 public:
  FakePresenceMonitorClient() = default;
  ~FakePresenceMonitorClient() override = default;

 private:
  // secure_channel::PresenceMonitorClient:
  void SetPresenceMonitorCallbacks(
      secure_channel::PresenceMonitor::ReadyCallback ready_callback,
      secure_channel::PresenceMonitor::DeviceSeenCallback device_seen_callback)
      override {}
  void StartMonitoring(
      const multidevice::RemoteDeviceRef& remote_device_ref,
      const multidevice::RemoteDeviceRef& local_device_ref) override {
    num_start_monitor_calls_++;
  }
  void StopMonitoring() override { num_stop_monitor_calls_++; }
};
}  // namespace

class EchePresenceManagerTest : public testing::Test {
 protected:
  EchePresenceManagerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        test_remote_device_(multidevice::CreateRemoteDeviceRefForTest()),
        test_devices_(multidevice::CreateRemoteDeviceRefListForTest(1)) {}
  EchePresenceManagerTest(const EchePresenceManagerTest&) = delete;
  EchePresenceManagerTest& operator=(const EchePresenceManagerTest&) = delete;
  ~EchePresenceManagerTest() override = default;

  void SetUp() override {
    fake_multidevice_setup_client_.SetHostStatusWithDevice(
        std::make_pair(multidevice_setup::mojom::HostStatus::kHostVerified,
                       test_remote_device_));
    fake_device_sync_client_.set_local_device_metadata(test_devices_[0]);
    fake_device_sync_client_.NotifyReady();
    fake_eche_connector_ = std::make_unique<FakeEcheConnector>();
    fake_eche_message_receiver_ = std::make_unique<FakeEcheMessageReceiver>();
    fake_feature_status_provider_ = std::make_unique<FakeFeatureStatusProvider>(
        FeatureStatus::kDependentFeature);
    fake_presence_monitor_client_ =
        std::make_unique<FakePresenceMonitorClient>();
    eche_presence_manager_ = std::make_unique<EchePresenceManager>(
        fake_feature_status_provider_.get(), &fake_device_sync_client_,
        &fake_multidevice_setup_client_,
        std::move(fake_presence_monitor_client_), fake_eche_connector_.get(),
        fake_eche_message_receiver_.get());
  }

  void TearDown() override {
    eche_presence_manager_.reset();
    fake_eche_connector_.reset();
    fake_eche_message_receiver_.reset();
    fake_feature_status_provider_.reset();
    fake_presence_monitor_client_.reset();
  }

  void SetFeatureStatus(FeatureStatus status) {
    fake_feature_status_provider_->SetStatus(status);
  }

  FeatureStatus GetFeatureStatus() {
    return fake_feature_status_provider_->GetStatus();
  }

  void SetStreamStatus(proto::StatusChangeType type) {
    fake_eche_message_receiver_->FakeStatusChange(type);
  }

  void Reset() {
    num_start_monitor_calls_ = 0;
    num_stop_monitor_calls_ = 0;
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<FakeEcheConnector> fake_eche_connector_;
  std::unique_ptr<FakeEcheMessageReceiver> fake_eche_message_receiver_;
  std::unique_ptr<FakeFeatureStatusProvider> fake_feature_status_provider_;
  const multidevice::RemoteDeviceRef test_remote_device_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
  device_sync::FakeDeviceSyncClient fake_device_sync_client_;
  const multidevice::RemoteDeviceRefList test_devices_;
  std::unique_ptr<FakePresenceMonitorClient> fake_presence_monitor_client_;
  std::unique_ptr<EchePresenceManager> eche_presence_manager_;
};

TEST_F(EchePresenceManagerTest, StopMonitoring_PersistentScan) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kEcheShorterScanningDutyCycle});
  // Test feature status change to kIneligible
  Reset();
  SetFeatureStatus(FeatureStatus::kConnected);
  SetStreamStatus(proto::StatusChangeType::TYPE_STREAM_START);
  SetFeatureStatus(FeatureStatus::kIneligible);
  EXPECT_EQ(1u, num_stop_monitor_calls_);

  // Test feature status change to kDisabled
  Reset();
  SetFeatureStatus(FeatureStatus::kConnected);
  SetStreamStatus(proto::StatusChangeType::TYPE_STREAM_START);
  SetFeatureStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(1u, num_stop_monitor_calls_);

  // Test feature status change to kDependentFeature
  Reset();
  SetFeatureStatus(FeatureStatus::kConnected);
  SetStreamStatus(proto::StatusChangeType::TYPE_STREAM_START);
  SetFeatureStatus(FeatureStatus::kDependentFeature);
  EXPECT_EQ(1u, num_stop_monitor_calls_);

  // Test feature status change to kDependentFeaturePending
  Reset();
  SetFeatureStatus(FeatureStatus::kConnected);
  SetStreamStatus(proto::StatusChangeType::TYPE_STREAM_START);
  SetFeatureStatus(FeatureStatus::kDependentFeaturePending);
  EXPECT_EQ(1u, num_stop_monitor_calls_);

  // Test feature status change to kDisconnected.
  Reset();
  SetFeatureStatus(FeatureStatus::kConnected);
  SetStreamStatus(proto::StatusChangeType::TYPE_STREAM_START);
  SetFeatureStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, num_stop_monitor_calls_);

  // Test feature status change to kConnecting.
  Reset();
  SetFeatureStatus(FeatureStatus::kConnected);
  SetStreamStatus(proto::StatusChangeType::TYPE_STREAM_START);
  SetFeatureStatus(FeatureStatus::kConnecting);
  EXPECT_EQ(1u, num_stop_monitor_calls_);

  // Test stream status change to stop.
  Reset();
  SetFeatureStatus(FeatureStatus::kConnected);
  SetStreamStatus(proto::StatusChangeType::TYPE_STREAM_START);
  SetStreamStatus(proto::StatusChangeType::TYPE_STREAM_STOP);
  EXPECT_EQ(1u, num_stop_monitor_calls_);

  // Test 5 minutes not see device.
  Reset();
  SetFeatureStatus(FeatureStatus::kConnected);
  SetStreamStatus(proto::StatusChangeType::TYPE_STREAM_START);
  task_environment_.FastForwardBy(base::Minutes(5));
  EXPECT_EQ(1u, num_stop_monitor_calls_);
}

TEST_F(EchePresenceManagerTest, StartMonitoring) {
  Reset();
  SetFeatureStatus(FeatureStatus::kConnected);
  SetStreamStatus(proto::StatusChangeType::TYPE_STREAM_START);
  EXPECT_EQ(1u, num_start_monitor_calls_);
}

TEST_F(EchePresenceManagerTest, StartMonitoring_PeriodicalScanning) {
  Reset();
  SetFeatureStatus(FeatureStatus::kConnected);
  SetStreamStatus(proto::StatusChangeType::TYPE_STREAM_START);
  EXPECT_EQ(1u, num_start_monitor_calls_);

  task_environment_.FastForwardBy(base::Seconds(30));
  // After 30 seconds, monitoring stopped.
  EXPECT_EQ(1u, num_stop_monitor_calls_);

  task_environment_.FastForwardBy(base::Seconds(30));
  // After 30 seconds, started monioring again.
  EXPECT_EQ(2u, num_start_monitor_calls_);

  task_environment_.FastForwardBy(base::Seconds(30));
  // After 30 seconds, monitoring stopped.
  EXPECT_EQ(2u, num_stop_monitor_calls_);

  task_environment_.FastForwardBy(base::Seconds(30));
  // After 30 seconds, started monioring again.
  EXPECT_EQ(3u, num_start_monitor_calls_);

  task_environment_.FastForwardBy(base::Seconds(30));
  // After 30 seconds, monitoring stopped.
  EXPECT_EQ(3u, num_stop_monitor_calls_);

  task_environment_.FastForwardBy(base::Seconds(30));
  // After 30 seconds, started monioring again.
  EXPECT_EQ(4u, num_start_monitor_calls_);

  task_environment_.FastForwardBy(base::Seconds(30));
  // After 30 seconds, monitoring stopped.
  EXPECT_EQ(4u, num_stop_monitor_calls_);

  task_environment_.FastForwardBy(base::Seconds(30));
  // After 30 seconds, started monioring again.
  EXPECT_EQ(5u, num_start_monitor_calls_);

  task_environment_.FastForwardBy(base::Seconds(30));
  // After 30 seconds, monitoring stopped.
  EXPECT_EQ(5u, num_stop_monitor_calls_);

  task_environment_.FastForwardBy(base::Seconds(30));
  // After 30 seconds, started monioring again.
  EXPECT_EQ(6u, num_start_monitor_calls_);

  // 5 minutes passed since last proximity check passed.

  task_environment_.FastForwardBy(base::Seconds(30));
  // After 30 seconds, monitoring stopped.
  EXPECT_EQ(6u, num_stop_monitor_calls_);

  task_environment_.FastForwardBy(base::Seconds(30));
  // After 5 minutesof last proximity check, monitoering should not be started
  // again.
  EXPECT_EQ(6u, num_start_monitor_calls_);
}

}  // namespace eche_app
}  // namespace ash
