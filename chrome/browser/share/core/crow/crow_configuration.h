// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_CORE_CROW_CROW_CONFIGURATION_H_
#define CHROME_BROWSER_SHARE_CORE_CROW_CROW_CONFIGURATION_H_

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

  // Returns the Publication ID for 'host', or the empty string.
  std::string GetPublicationID(const std::string& host) const;

 protected:
  CrowConfiguration();

 private:
  // The latest targets we've committed. Starts out null.
  // Protected by lock_.
  base::flat_map<std::string, std::string> domains_;

  mutable base::Lock lock_;

  FRIEND_TEST_ALL_PREFIXES(CrowConfigurationTest, Update);

  friend struct CrowConfigurationSingletonTrait;
};

}  // namespace crow

#endif  // CHROME_BROWSER_SHARE_CORE_CROW_CROW_CONFIGURATION_H_
