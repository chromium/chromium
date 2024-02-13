// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_NETBIOS_CLIENT_INTERFACE_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_NETBIOS_CLIENT_INTERFACE_H_

#include <vector>

#include "base/functional/callback.h"

namespace net {
class IPAddress;
class IPEndPoint;
}  // namespace net

namespace ash::smb_client {

using NetBiosResponseCallback = base::RepeatingCallback<
    void(const std::vector<uint8_t>&, uint16_t, const net::IPEndPoint&)>;

class NetBiosClientInterface {
 public:
  NetBiosClientInterface(const NetBiosClientInterface&) = delete;
  NetBiosClientInterface& operator=(const NetBiosClientInterface&) = delete;

  virtual ~NetBiosClientInterface() = default;

  // Starts the Name Query Request process. Any response packets that match
  // |transaction_id| are passed to |callback|.
  virtual void ExecuteNameRequest(const net::IPAddress& broadcast_address,
                                  uint16_t transaction_id,
                                  NetBiosResponseCallback callback) = 0;

 protected:
  NetBiosClientInterface() = default;
};

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_NETBIOS_CLIENT_INTERFACE_H_
