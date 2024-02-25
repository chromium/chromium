// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/discovery/fake_netbios_client.h"

#include "net/base/ip_endpoint.h"

namespace ash::smb_client {

FakeNetBiosClient::FakeNetBiosClient() = default;

FakeNetBiosClient::FakeNetBiosClient(
    std::map<net::IPEndPoint, std::vector<uint8_t>> fake_data)
    : fake_data_(std::move(fake_data)) {}

FakeNetBiosClient::~FakeNetBiosClient() = default;

void FakeNetBiosClient::ExecuteNameRequest(
    const net::IPAddress& broadcast_address,
    uint16_t transaction_id,
    NetBiosResponseCallback callback) {
  DCHECK(callback);

  for (const auto& kv : fake_data_) {
    const net::IPEndPoint& ip_address = kv.first;
    const std::vector<uint8_t>& packet = kv.second;
    callback.Run(packet, transaction_id, ip_address);
  }
}

}  // namespace ash::smb_client
