// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_CORE_CROW_CROW_CONFIGURATION_H_
#define CHROME_BROWSER_SHARE_CORE_CROW_CROW_CONFIGURATION_H_

#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/share/proto/crow_configuration.pb.h"

namespace crow {

struct CrowConfigurationSingletonTrait;

class CrowConfiguration {
 public:
  static CrowConfiguration* GetInstance();  // Singleton

  CrowConfiguration(const CrowConfiguration&) = delete;
  CrowConfiguration& operator=(const CrowConfiguration&) = delete;
  virtual ~CrowConfiguration();

  // Read data from an serialized protobuf and update the internal list.
  void PopulateFromBinaryPb(const std::string& binary_pb);

  // Returns the Publication ID for 'host' if present in the component data
  // allowlist, or the empty string if it is not present.
  // Note that hosts may also be enabled via a Java-side bloom filter,
  // and might be present in the component Denylist (see DenylistContainsHost),
  // though we do not intend to have a host in both lists simultaneously.
  std::string GetPublicationIDFromAllowlist(const std::string& host) const;

  // Returns whether |host| is explicitly in the feature's denylist.
  bool DenylistContainsHost(const std::string& host) const;

 protected:
  CrowConfiguration();

 private:
  // Hostname/Publisher ID mappings. Protected by |lock_|.
  base::flat_map<std::string, std::string> domains_;

  // Hostname/Publisher ID mappings. Protected by |lock_|.
  std::set<std::string> denied_hosts_;

  mutable base::Lock lock_;

  FRIEND_TEST_ALL_PREFIXES(CrowConfigurationTest, UpdateAllowlist);

  friend struct CrowConfigurationSingletonTrait;
};

}  // namespace crow

#endif  // CHROME_BROWSER_SHARE_CORE_CROW_CROW_CONFIGURATION_H_
