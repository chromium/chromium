// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_CONNECTION_DETAILS_H_
#define ASH_SERVICES_SECURE_CHANNEL_CONNECTION_DETAILS_H_

#include <ostream>
#include <string>

#include "ash/services/secure_channel/public/cpp/shared/connection_medium.h"

namespace chromeos {

namespace secure_channel {

// Fields describing a connection. At any given time, at most one connection
// with a given set of ConnectionDetails should exist. Note that for all host
// devices (i.e., Android phones), a single device ID applies to every logged-in
// user on the device.
class ConnectionDetails {
 public:
  ConnectionDetails(const std::string& device_id,
                    ConnectionMedium connection_medium);
  ~ConnectionDetails();

  ConnectionDetails(const ConnectionDetails&) noexcept = default;
  ConnectionDetails& operator=(const ConnectionDetails&) noexcept = default;

  ConnectionDetails(ConnectionDetails&&) noexcept = default;
  ConnectionDetails& operator=(ConnectionDetails&&) noexcept = default;

  const std::string& device_id() const { return device_id_; }
  ConnectionMedium connection_medium() const { return connection_medium_; }

  bool operator==(const ConnectionDetails& other) const;
  bool operator!=(const ConnectionDetails& other) const;
  bool operator<(const ConnectionDetails& other) const;

 private:
  friend struct ConnectionDetailsHash;

  std::string device_id_;
  ConnectionMedium connection_medium_;
};

std::ostream& operator<<(std::ostream& stream,
                         const ConnectionDetails& details);

}  // namespace secure_channel

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash::secure_channel {
using ::chromeos::secure_channel::ConnectionDetails;
}

#endif  // ASH_SERVICES_SECURE_CHANNEL_CONNECTION_DETAILS_H_
