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
                     RandomSessionId session_id,
                     const std::string& authentication_token);

  // An alternate constructor that accepts a shared_secret for testing purposes
  // or for resuming a connection after a critical update.
  IncomingConnection(NearbyConnection* nearby_connection,
                     RandomSessionId session_id,
                     const std::string& authentication_token,
                     std::array<uint8_t, 32> shared_secret);

  IncomingConnection(IncomingConnection&) = delete;
  IncomingConnection& operator=(IncomingConnection&) = delete;
  ~IncomingConnection() override;

  // Derive a 4-digit decimal pin code from the authentication token. This is
  // meant to match the Android implementation found here:
  // http://google3/java/com/google/android/gmscore/integ/modules/smartdevice/src/com/google/android/gms/smartdevice/d2d/nearby/advertisement/VerificationUtils.java;l=37;rcl=511361463
  static std::string DerivePin(const std::string& authentication_token);

  // Returns a deep link URL as a vector of bytes that will form the QR code
  // used to authenticate the connection.
  std::vector<uint8_t> GetQrCodeData() const;

  // Return the 4-digit pin code to be displayed for the user to match against
  // the source device in order to authenticate the connection. Derived from the
  // Nearby Connection's authentication token.
  std::string GetConnectionVerificationPin() const { return pin_; }

 private:
  // A 4-digit decimal pin code derived from the connection's authentication
  // token for the alternative pin authentication flow.
  std::string pin_;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_INCOMING_CONNECTION_H_
