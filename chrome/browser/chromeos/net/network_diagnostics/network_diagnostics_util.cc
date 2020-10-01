// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics_util.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/storage_partition.h"

namespace chromeos {
namespace network_diagnostics {

namespace util {

namespace {

// Returns |num_prefixes| prefixes of size |length|, where no two entries are
// equal.
std::vector<std::string> GetRandomPrefixes(size_t num_prefixes, int length) {
  std::vector<std::string> random_prefixes;
  while (random_prefixes.size() != num_prefixes) {
    std::string prefix = GetRandomString(length);
    // Check that the prefix doesn't already exist.
    if (std::find(std::begin(random_prefixes), std::end(random_prefixes),
                  prefix) == std::end(random_prefixes)) {
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
  DCHECK(random_prefixes.size() == num_hosts);
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

Profile* GetUserProfile() {
  return ProfileManager::GetPrimaryUserProfile();
}

}  // namespace util

}  // namespace network_diagnostics
}  // namespace chromeos
