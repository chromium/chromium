// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_IN_MEMORY_HOST_LOCATOR_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_IN_MEMORY_HOST_LOCATOR_H_

#include "chrome/browser/ash/smb_client/discovery/host_locator.h"

namespace ash::smb_client {

// HostLocator implementation that uses a map as the source for hosts. New hosts
// can be registered through AddHost().
class InMemoryHostLocator : public HostLocator {
 public:
  InMemoryHostLocator();
  explicit InMemoryHostLocator(bool should_run_synchronously);

  InMemoryHostLocator(const InMemoryHostLocator&) = delete;
  InMemoryHostLocator& operator=(const InMemoryHostLocator&) = delete;

  ~InMemoryHostLocator() override;

  // Adds host with |hostname| and |address| to host_map_.
  void AddHost(const Hostname& hostname, const Address& address);

  // Adds |hosts| to host_map_;
  void AddHosts(const HostMap& hosts);

  // Removes host with |hostname| from host_map_.
  void RemoveHost(const Hostname& hostname);

  // HostLocator override.
  void FindHosts(FindHostsCallback callback) override;

  // Runs the callback, |stored_callback_|.
  void RunCallback();

 private:
  HostMap host_map_;
  FindHostsCallback stored_callback_;
  bool should_run_synchronously_ = true;
};

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_IN_MEMORY_HOST_LOCATOR_H_
