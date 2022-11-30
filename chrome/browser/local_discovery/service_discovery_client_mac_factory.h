// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_FACTORY_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_FACTORY_H_

#include "chrome/browser/local_discovery/service_discovery_shared_client.h"

namespace local_discovery {

class ServiceDiscoveryClientMacFactory {
 public:
  static scoped_refptr<ServiceDiscoverySharedClient> CreateInstance();

 private:
  ServiceDiscoveryClientMacFactory() {}
  virtual ~ServiceDiscoveryClientMacFactory() {}
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_FACTORY_H_
