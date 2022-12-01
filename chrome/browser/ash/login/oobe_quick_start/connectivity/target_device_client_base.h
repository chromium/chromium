// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CLIENT_BASE_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CLIENT_BASE_H_

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/quick_start_decoder.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"

namespace ash::quick_start {

class TargetDeviceClientBaseTest;

// TargetDeviceClientBase is the parent class for all clients that will prepare
// requests and parse responses for all round trips between the Chromebook
// and Android phone during Quick Start. There will always be only one client
// running at any time. Multiple clients will interfere with each other.
class TargetDeviceClientBase {
 public:
  TargetDeviceClientBase(const TargetDeviceClientBase&) = delete;
  TargetDeviceClientBase& operator=(const TargetDeviceClientBase&) = delete;
  virtual ~TargetDeviceClientBase();

 protected:
  TargetDeviceClientBase(NearbyConnection* nearby_connection,
                         QuickStartDecoder* quick_start_decoder);
  void SendPayload(const base::Value::Dict& message_payload);

  // OnDataRead() will be called when the remote end responds to the message
  // sent with SendPayload().
  virtual void OnDataRead(absl::optional<std::vector<uint8_t>> data) = 0;

  NearbyConnection* nearby_connection_;
  QuickStartDecoder* quick_start_decoder_;

 private:
  // This will allow TargetDeviceClientBaseTest to call the protected methods
  // for unit tests.
  friend class TargetDeviceClientBaseTest;
  base::WeakPtrFactory<TargetDeviceClientBase> weak_ptr_factory_{this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CLIENT_BASE_H_
