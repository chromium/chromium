// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_UTIL_H_

#include <string>
#include <vector>

class Profile;

namespace chromeos {
namespace network_diagnostics {

namespace util {

// Returns the Gstatic host suffix. Network diagnostic routines attach a random
// prefix to |kGstaticHostSuffix| to get a complete hostname.
const char* GetGstaticHostSuffix();

// Returns the list representing a fixed set of hostnames used by routines.
const std::vector<std::string>& GetFixedHosts();

// Returns a string of length |length|. Contains characters 'a'-'z', inclusive.
std::string GetRandomString(int length);

// Returns |num_hosts| random hosts, each suffixed with |kGstaticHostSuffix| and
// prefixed with a random string of length |prefix_length|.
std::vector<std::string> GetRandomHosts(int num_hosts, int prefix_length);

// Similar to GetRandomHosts(), but the fixed hosts are prepended to the list.
// The total number of hosts in this list is GetFixedHosts().size() +
// num_random_hosts.
std::vector<std::string> GetRandomHostsWithFixedHosts(int num_random_hosts,
                                                      int prefix_length);

// Returns the profile associated with this account.
Profile* GetUserProfile();

}  // namespace util

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_UTIL_H_
