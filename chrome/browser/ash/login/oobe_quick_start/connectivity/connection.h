// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_CONNECTION_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_CONNECTION_H_

#include "base/values.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"

namespace ash::quick_start {

// Represents a connection to the remote source device and is an abstraction of
// a Nearby Connection.
class Connection {
 public:
  explicit Connection(NearbyConnection* nearby_connection);
  virtual ~Connection() = default;

 protected:
  // Reusable method to serialize a payload into JSON bytes and send via Nearby
  // Connections.
  void SendPayload(const base::Value::Dict& message_payload);

  NearbyConnection* nearby_connection_;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_CONNECTION_H_
