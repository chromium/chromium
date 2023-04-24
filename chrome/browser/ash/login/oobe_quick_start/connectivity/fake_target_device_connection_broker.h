// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FAKE_TARGET_DEVICE_CONNECTION_BROKER_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FAKE_TARGET_DEVICE_CONNECTION_BROKER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_factory.h"

class FakeNearbyConnection;

namespace ash::quick_start {

class FakeQuickStartDecoder;
class RandomSessionId;

class FakeTargetDeviceConnectionBroker : public TargetDeviceConnectionBroker {
 public:
  class Factory : public TargetDeviceConnectionBrokerFactory {
   public:
    Factory();
    Factory(Factory&) = delete;
    Factory& operator=(Factory&) = delete;
    ~Factory() override;

    // Returns all FakeTargetDeviceConnectionBroker instances created by
    // CreateInstance().
    const std::vector<FakeTargetDeviceConnectionBroker*>& instances() {
      return instances_;
    }

    void set_initial_feature_support_status(
        FeatureSupportStatus initial_feature_support_status) {
      initial_feature_support_status_ = initial_feature_support_status;
    }

   private:
    FeatureSupportStatus initial_feature_support_status_ =
        FeatureSupportStatus::kSupported;

    // TargetDeviceConnectionBrokerFactory:
    std::unique_ptr<TargetDeviceConnectionBroker> CreateInstance(
        base::WeakPtr<NearbyConnectionsManager> nearby_connections_manager,
        RandomSessionId session_id) override;

    std::vector<FakeTargetDeviceConnectionBroker*> instances_;
  };

  FakeTargetDeviceConnectionBroker();
  FakeTargetDeviceConnectionBroker(FakeTargetDeviceConnectionBroker&) = delete;
  FakeTargetDeviceConnectionBroker& operator=(
      FakeTargetDeviceConnectionBroker&) = delete;
  ~FakeTargetDeviceConnectionBroker() override;

  // TargetDeviceConnectionBroker:
  FeatureSupportStatus GetFeatureSupportStatus() const override;
  void StartAdvertising(ConnectionLifecycleListener* listener,
                        bool use_pin_authentication,
                        ResultCallback on_start_advertising_callback) override;
  void StopAdvertising(base::OnceClosure on_stop_advertising_callback) override;
  base::Value::Dict GetPrepareForUpdateInfo() override;

  void InitiateConnection(const std::string& source_device_id);
  void AuthenticateConnection(const std::string& source_device_id);
  void RejectConnection();
  void CloseConnection(ConnectionClosedReason reason);

  void set_feature_support_status(FeatureSupportStatus feature_support_status) {
    feature_support_status_ = feature_support_status;
    MaybeNotifyFeatureStatus();
  }

  void set_use_pin_authentication(bool use_pin_authentication) {
    use_pin_authentication_ = use_pin_authentication;
  }

  size_t num_start_advertising_calls() const {
    return num_start_advertising_calls_;
  }

  size_t num_stop_advertising_calls() const {
    return num_stop_advertising_calls_;
  }

  ConnectionLifecycleListener* connection_lifecycle_listener() const {
    return connection_lifecycle_listener_;
  }

  ResultCallback on_start_advertising_callback() {
    return std::move(on_start_advertising_callback_);
  }

  base::OnceClosure on_stop_advertising_callback() {
    return std::move(on_stop_advertising_callback_);
  }

 private:
  size_t num_start_advertising_calls_ = 0;
  size_t num_stop_advertising_calls_ = 0;
  FeatureSupportStatus feature_support_status_ =
      FeatureSupportStatus::kSupported;
  ResultCallback on_start_advertising_callback_;
  base::OnceClosure on_stop_advertising_callback_;
  std::unique_ptr<FakeNearbyConnection> fake_nearby_connection_;
  std::unique_ptr<FakeQuickStartDecoder> fake_quick_start_decoder_;

  base::WeakPtrFactory<FakeTargetDeviceConnectionBroker> weak_ptr_factory_{
      this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FAKE_TARGET_DEVICE_CONNECTION_BROKER_H_
