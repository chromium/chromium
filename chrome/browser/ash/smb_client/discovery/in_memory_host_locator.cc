// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/discovery/in_memory_host_locator.h"

#include <map>
#include <utility>

namespace ash::smb_client {

InMemoryHostLocator::InMemoryHostLocator() = default;
InMemoryHostLocator::InMemoryHostLocator(bool should_run_synchronously)
    : should_run_synchronously_(should_run_synchronously) {}
InMemoryHostLocator::~InMemoryHostLocator() = default;

void InMemoryHostLocator::AddHost(const Hostname& hostname,
                                  const Address& address) {
  host_map_[hostname] = address;
}

void InMemoryHostLocator::AddHosts(const HostMap& hosts) {
  for (const auto& host : hosts) {
    AddHost(host.first, host.second);
  }
}

void InMemoryHostLocator::RemoveHost(const Hostname& hostname) {
  host_map_.erase(hostname);
}

void InMemoryHostLocator::FindHosts(FindHostsCallback callback) {
  if (should_run_synchronously_) {
    std::move(callback).Run(true /* success */, host_map_);
  } else {
    stored_callback_ = std::move(callback);
  }
}

void InMemoryHostLocator::RunCallback() {
  DCHECK(!should_run_synchronously_);

  std::move(stored_callback_).Run(true /* success */, host_map_);
}

}  // namespace ash::smb_client
