// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_FAKE_NETBIOS_CLIENT_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_FAKE_NETBIOS_CLIENT_H_

#include <map>
#include <vector>

#include "chrome/browser/ash/smb_client/discovery/netbios_client_interface.h"

namespace net {
class IPAddress;
class IPEndPoint;
}  // namespace net

namespace ash::smb_client {

// FakeNetBiosClient is used for testing the NetBiosHostLocator.
// FakeNetBiosClient is constructed with a map of IPs -> Packets to simulate
// responses received from the Name Request. When ExecuteNameRequest is called,
// the NetBiosResponseCallback will be run with each entry in the |fake_data_|
// map. The |broadcast_address| and |transaction_id| parameters on
// ExecuteNameRequest are ignored.
class FakeNetBiosClient : public NetBiosClientInterface {
 public:
  FakeNetBiosClient();
  explicit FakeNetBiosClient(
      std::map<net::IPEndPoint, std::vector<uint8_t>> fake_data);

  FakeNetBiosClient(const FakeNetBiosClient&) = delete;
  FakeNetBiosClient& operator=(const FakeNetBiosClient&) = delete;

  ~FakeNetBiosClient() override;

  // NetBiosClientInterface override.
  void ExecuteNameRequest(const net::IPAddress& broadcast_address,
                          uint16_t transaction_id,
                          NetBiosResponseCallback callback) override;

 private:
  std::map<net::IPEndPoint, std::vector<uint8_t>> fake_data_;
};

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_FAKE_NETBIOS_CLIENT_H_
