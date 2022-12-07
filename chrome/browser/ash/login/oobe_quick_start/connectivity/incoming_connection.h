// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_INCOMING_CONNECTION_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_INCOMING_CONNECTION_H_

#include <array>

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/connection.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"

namespace ash::quick_start {

// Represents a new incoming connection that has not yet been accepted by the
// remote source device.
class IncomingConnection : public Connection {
 public:
  IncomingConnection(NearbyConnection* nearby_connection,
                     RandomSessionId session_id);

  // An alternate constructor that accepts a shared_secret for testing purposes
  // or for resuming a connection after a critical update.
  IncomingConnection(NearbyConnection* nearby_connection,
                     RandomSessionId session_id,
                     std::array<uint8_t, 32> shared_secret);

  IncomingConnection(IncomingConnection&) = delete;
  IncomingConnection& operator=(IncomingConnection&) = delete;
  ~IncomingConnection() override;

  // Returns a deep link URL as a vector of bytes that will form the QR code
  // used to authenticate the connection.
  std::vector<uint8_t> GetQrCodeData() const;

 private:
  RandomSessionId random_session_id_;

  // A secret represented in a 32-byte array that gets generated and sent to the
  // source device so it can be used later to authenticate the connection.
  std::array<uint8_t, 32> shared_secret_;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_INCOMING_CONNECTION_H_
