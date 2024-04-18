// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FAKE_TARGET_DEVICE_CONNECTION_BROKER_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FAKE_TARGET_DEVICE_CONNECTION_BROKER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/connection.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_connection.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/session_context.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_factory.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"

class FakeNearbyConnection;

namespace ash::quick_start {

class AdvertisingId;
class QuickStartConnectivityService;

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
    const std::vector<
        raw_ptr<FakeTargetDeviceConnectionBroker, VectorExperimental>>&
    instances() {
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
        SessionContext* session_context,
        QuickStartConnectivityService* quick_start_connectivity_service)
        override;

    std::vector<raw_ptr<FakeTargetDeviceConnectionBroker, VectorExperimental>>
        instances_;
  };

  explicit FakeTargetDeviceConnectionBroker(
      SessionContext* session_context,
      QuickStartConnectivityService* quick_start_connectivity_service);
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

  void InitiateConnection(const std::string& source_device_id);
  void AuthenticateConnection(
      const std::string& source_device_id,
      QuickStartMetrics::AuthenticationMethod auth_method);
  void RejectConnection();
  void CloseConnection(ConnectionClosedReason reason);

  void set_feature_support_status(FeatureSupportStatus feature_support_status) {
    feature_support_status_ = feature_support_status;
    MaybeNotifyFeatureStatus();
  }

  std::string GetAdvertisingIdDisplayCode() override;

  void set_use_pin_authentication(bool use_pin_authentication) {
    use_pin_authentication_ = use_pin_authentication;
  }

  std::string GetPinForTests();

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

  std::optional<bool> start_advertising_use_pin_authentication() {
    return start_advertising_use_pin_authentication_;
  }

  SessionContext::SessionId session_id() {
    return session_context_->session_id();
  }

  FakeConnection* GetFakeConnection();

 private:
  size_t num_start_advertising_calls_ = 0;
  size_t num_stop_advertising_calls_ = 0;
  FeatureSupportStatus feature_support_status_ =
      FeatureSupportStatus::kSupported;
  ResultCallback on_start_advertising_callback_;
  base::OnceClosure on_stop_advertising_callback_;
  raw_ptr<SessionContext> session_context_;
  raw_ptr<QuickStartConnectivityService> quick_start_connectivity_service_;
  std::unique_ptr<FakeNearbyConnection> fake_nearby_connection_;
  std::unique_ptr<FakeConnection> connection_;

  AdvertisingId advertising_id_;
  std::optional<bool> start_advertising_use_pin_authentication_;

  base::WeakPtrFactory<FakeTargetDeviceConnectionBroker> weak_ptr_factory_{
      this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FAKE_TARGET_DEVICE_CONNECTION_BROKER_H_
