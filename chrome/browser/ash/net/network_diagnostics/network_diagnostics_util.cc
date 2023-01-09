// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_util.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/storage_partition.h"

namespace ash {
namespace network_diagnostics {

namespace util {

const char kGenerate204Path[] = "/generate_204";

namespace {

// Returns |num_prefixes| prefixes of size |length|, where no two entries are
// equal.
std::vector<std::string> GetRandomPrefixes(size_t num_prefixes, int length) {
  std::vector<std::string> random_prefixes;
  while (random_prefixes.size() != num_prefixes) {
    std::string prefix = GetRandomString(length);
    // Check that the prefix doesn't already exist.
    if (!base::Contains(random_prefixes, prefix)) {
      random_prefixes.push_back(prefix);
    }
  }
  return random_prefixes;
}

}  // namespace

const char* GetGstaticHostSuffix() {
  static const char* gstatic_host_suffix = "-ccd-testing-v4.metric.gstatic.com";
  return gstatic_host_suffix;
}

const std::vector<std::string>& GetFixedHosts() {
  static base::NoDestructor<std::vector<std::string>> fixed_hostnames(
      {"www.google.com", "mail.google.com", "drive.google.com",
       "accounts.google.com", "plus.google.com", "groups.google.com"});
  return *fixed_hostnames;
}

std::string GetRandomString(int length) {
  std::string prefix;
  for (int i = 0; i < length; i++) {
    prefix += ('a' + base::RandInt(0, 25));
  }
  return prefix;
}

std::vector<std::string> GetRandomHosts(int num_hosts, int prefix_length) {
  std::vector<std::string> random_hosts;
  std::vector<std::string> random_prefixes =
      GetRandomPrefixes(num_hosts, prefix_length);
  DCHECK(random_prefixes.size() == 1U * num_hosts);
  for (int i = 0; i < num_hosts; i++) {
    random_hosts.push_back(random_prefixes[i] + GetGstaticHostSuffix());
  }
  return random_hosts;
}

std::vector<std::string> GetRandomHostsWithFixedHosts(int num_random_hosts,
                                                      int prefix_length) {
  std::vector<std::string> hosts = GetFixedHosts();
  std::vector<std::string> random_hosts =
      GetRandomHosts(num_random_hosts, prefix_length);
  hosts.insert(hosts.end(), random_hosts.begin(), random_hosts.end());

  return hosts;
}

std::vector<std::string> GetRandomHostsWithScheme(int num_hosts,
                                                  int prefix_length,
                                                  std::string scheme) {
  std::vector<std::string> hosts = GetRandomHosts(num_hosts, prefix_length);
  for (auto& host : hosts) {
    host = scheme + host;
  }
  return hosts;
}

std::vector<std::string> GetRandomAndFixedHostsWithScheme(int num_random_hosts,
                                                          int prefix_length,
                                                          std::string scheme) {
  std::vector<std::string> hosts =
      GetRandomHostsWithFixedHosts(num_random_hosts, prefix_length);
  for (auto& host : hosts) {
    host = scheme + host;
  }
  return hosts;
}

std::vector<std::string> GetRandomAndFixedHostsWithSchemeAndPort(
    int num_random_hosts,
    int prefix_length,
    std::string scheme,
    int port_number) {
  std::vector<std::string> hosts =
      GetRandomAndFixedHostsWithScheme(num_random_hosts, prefix_length, scheme);
  for (auto& host : hosts) {
    host = host + ":" + base::NumberToString(port_number) + "/";
  }
  return hosts;
}

std::vector<std::string> GetRandomHostsWithSchemeAndGenerate204Path(
    int num_hosts,
    int prefix_length,
    std::string scheme) {
  std::vector<std::string> hosts = GetRandomHosts(num_hosts, prefix_length);
  for (auto& host : hosts) {
    host = scheme + host + kGenerate204Path;
  }
  return hosts;
}

std::vector<GURL> GetRandomHostsWithSchemeAndPortAndGenerate204Path(
    int num_hosts,
    int prefix_length,
    std::string scheme,
    int port_number) {
  const auto& hosts = GetRandomHosts(num_hosts, prefix_length);
  std::vector<GURL> urls;
  for (auto& host : hosts) {
    auto url = GURL(scheme + host + ":" + base::NumberToString(port_number) +
                    kGenerate204Path);
    DCHECK(url.is_valid());
    urls.push_back(url);
  }
  return urls;
}

Profile* GetUserProfile() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile);

  return profile;
}

const std::array<uint8_t, kStunHeaderSize>& GetStunHeader() {
  static std::array<uint8_t, kStunHeaderSize> stun_header = {
      0x00, 0x01, 0x00, 0x00, 0x21, 0x12, 0xa4, 0x42, 0x79, 0x64,
      0x66, 0x36, 0x66, 0x53, 0x42, 0x73, 0x76, 0x77, 0x76, 0x75};

  return stun_header;
}

net::NetworkTrafficAnnotationTag GetStunNetworkAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation("network_diagnostics_stun",
                                             R"(
      semantics {
        sender: "NetworkDiagnosticsRoutines"
        description:
            "Routines send network traffic to hosts in order to "
            "validate the internet connection on a device."
        trigger:
            "A routine attempts a socket connection or makes an http/s "
            "request."
        data:
          "For UDP connections, data is sent along with the origin "
          "(scheme-host-port). The primary purpose of the UDP prober is to "
          "send a STUN packet header to a STUN server. For TCP connections, "
          "only the origin is sent. No user identifier is sent along with the "
          "data."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        policy_exception_justification:
            "Not implemented. Does not contain user identifier."
      }
  )");
}

std::vector<int> GetUdpPortsForGoogleStunServer() {
  return {19302, 19303, 19304, 19305, 19306, 19307, 19308, 19309};
}

std::vector<int> GetUdpPortsForCustomStunServer() {
  return {3478};
}

std::vector<int> GetTcpPortsForGoogleStunServer() {
  return {19305, 19306, 19307, 19308};
}

std::vector<int> GetTcpPortsForCustomStunServer() {
  return {3478};
}

std::vector<GURL> GetDefaultMediaUrls() {
  const char* const kHostnames[] = {
      "https://apis.google.com",           "https://talkgadget.google.com",
      "https://clients6.google.com",       "https://hangouts.google.com",
      "https://client-channel.google.com", "https://googleapis.com",
      "https://accounts.google.com",       "https://clients4.google.com"};
  std::vector<GURL> hostnames;
  for (auto* const& hostname : kHostnames) {
    hostnames.push_back(GURL(hostname));
  }
  return hostnames;
}

}  // namespace util

}  // namespace network_diagnostics
}  // namespace ash
