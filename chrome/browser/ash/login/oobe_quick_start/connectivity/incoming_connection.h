// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_INCOMING_CONNECTION_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_INCOMING_CONNECTION_H_

#include <array>

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/connection.h"

namespace ash::quick_start {

// Represents a new incoming connection that has not yet been accepted by the
// remote source device.
class IncomingConnection : public Connection {
 public:
  IncomingConnection();
  IncomingConnection(IncomingConnection&) = delete;
  IncomingConnection& operator=(IncomingConnection&) = delete;
  ~IncomingConnection() override;

 private:
  // A secret represented in a 32-byte array that gets generated and sent to the
  // source device so it can be used later to authenticate the connection.
  std::array<uint8_t, 32> shared_secret_;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_INCOMING_CONNECTION_H_
