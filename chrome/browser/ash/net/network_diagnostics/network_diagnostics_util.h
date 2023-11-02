// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_UTIL_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_UTIL_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

class Profile;

namespace ash {
namespace network_diagnostics {

namespace util {

// DNS queries taking longer than 400 ms are potentially problematic.
constexpr int kDnsPotentialProblemLatencyMs = 400;

// DNS queries taking longer than 500 ms are problematic.
constexpr int kDnsProblemLatencyMs = 500;

// Generate 204 path.
extern const char kGenerate204Path[];

// STUN packet header size.
constexpr int kStunHeaderSize = 20;

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

// Similar to GetRandomHosts, but with a |scheme| prepended to the hosts.
std::vector<std::string> GetRandomHostsWithScheme(int num_hosts,
                                                  int prefix_length,
                                                  std::string scheme);

// Similar to GetRandomHostsWithFixedHosts, but with a |scheme| prepended to the
// hosts.
std::vector<std::string> GetRandomAndFixedHostsWithScheme(int num_random_hosts,
                                                          int prefix_length,
                                                          std::string scheme);

// Similar to GetRandomAndFixedHostsWithSchemeAndPort, but with |port|, followed
// by "/", appended to the hosts. E.g. A host will look like:
// "https://www.google.com:443/".
std::vector<std::string> GetRandomAndFixedHostsWithSchemeAndPort(
    int num_random_hosts,
    int prefix_length,
    std::string scheme,
    int port_number);

// Similar to GetRandomHostsWithScheme, but with the 204 path appended to hosts.
std::vector<std::string> GetRandomHostsWithSchemeAndGenerate204Path(
    int num_hosts,
    int prefix_length,
    std::string scheme);

// Similar to GetRandomAndFixedHostsWithSchemeAndPort, but with |port_number|
// and 204 path appended to the hosts. E.g. A host will look like:
// "https://www.google.com:443/generate_204/".
std::vector<GURL> GetRandomHostsWithSchemeAndPortAndGenerate204Path(
    int num_hosts,
    int prefix_length,
    std::string scheme,
    int port_number);

// Returns the profile associated with this account.
Profile* GetUserProfile();

// Returns a STUN packet with a header defined in RFC 5389.
const std::array<uint8_t, kStunHeaderSize>& GetStunHeader();

// Returns the traffic annotation tag for STUN traffic.
net::NetworkTrafficAnnotationTag GetStunNetworkAnnotationTag();

// Returns the ports used to speak to Google's STUN server over UDP.
std::vector<int> GetUdpPortsForGoogleStunServer();

// Returns the ports used to speak to a custom STUN server over UDP.
std::vector<int> GetUdpPortsForCustomStunServer();

// Returns the ports used to speak to Google's STUN server over TCP.
std::vector<int> GetTcpPortsForGoogleStunServer();

// Returns the ports used to speak to a custom STUN server over TCP.
std::vector<int> GetTcpPortsForCustomStunServer();

// Returns the list of urls related to Google media.
std::vector<GURL> GetDefaultMediaUrls();

}  // namespace util

}  // namespace network_diagnostics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_UTIL_H_
