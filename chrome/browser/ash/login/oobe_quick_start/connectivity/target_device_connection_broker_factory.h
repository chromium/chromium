// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_FACTORY_H_

#include <memory>

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"

namespace ash::quick_start {

class QuickStartConnectivityService;
class SessionContext;

// A factory class for creating instances of TargetDeviceConnectionBroker.
// Calling code should use the static Create() method.
class TargetDeviceConnectionBrokerFactory {
 public:
  static std::unique_ptr<TargetDeviceConnectionBroker> Create(
      SessionContext* session_context,
      QuickStartConnectivityService* quick_start_connectivity_service);

  static void SetFactoryForTesting(
      TargetDeviceConnectionBrokerFactory* test_factory);

  TargetDeviceConnectionBrokerFactory();
  TargetDeviceConnectionBrokerFactory(TargetDeviceConnectionBrokerFactory&) =
      delete;
  TargetDeviceConnectionBrokerFactory& operator=(
      TargetDeviceConnectionBrokerFactory&) = delete;
  virtual ~TargetDeviceConnectionBrokerFactory();

 protected:
  virtual std::unique_ptr<TargetDeviceConnectionBroker> CreateInstance(
      SessionContext* session_context,
      QuickStartConnectivityService* quick_start_connectivity_service) = 0;

 private:
  static TargetDeviceConnectionBrokerFactory* test_factory_;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_FACTORY_H_
