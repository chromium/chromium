// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_HOST_LOCATOR_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_HOST_LOCATOR_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "net/base/ip_address.h"

namespace ash::smb_client {

using Hostname = std::string;
using Address = net::IPAddress;
using HostMap = std::map<Hostname, Address>;

// |success| will be false if an error occurred when finding hosts. |success|
// can be true even if |hosts| is empty.
using FindHostsCallback =
    base::OnceCallback<void(bool success, const HostMap& hosts)>;

// Interface that abstracts the multiple methods of finding SMB hosts in a
// network. (e.g. mDNS, NetBIOS over TCP, LMHosts, DNS)
class HostLocator {
 public:
  HostLocator() = default;

  HostLocator(const HostLocator&) = delete;
  HostLocator& operator=(const HostLocator&) = delete;

  virtual ~HostLocator() = default;

  // Finds hosts in the local network. |callback| will be called once finished
  // finding all the hosts. If no hosts are found, an empty map will be passed
  // in the |callback|.
  virtual void FindHosts(FindHostsCallback callback) = 0;
};

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_HOST_LOCATOR_H_
