// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_SHARED_CLIENT_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_SHARED_CLIENT_H_

#include "chrome/browser/local_discovery/service_discovery_client.h"
#include "content/public/browser/browser_thread.h"

namespace local_discovery {

class ServiceDiscoverySharedClient
    : public base::RefCountedThreadSafe<
          ServiceDiscoverySharedClient,
          content::BrowserThread::DeleteOnUIThread>,
      public ServiceDiscoveryClient {
 public:
  static scoped_refptr<ServiceDiscoverySharedClient> GetInstance();

  ServiceDiscoverySharedClient(const ServiceDiscoverySharedClient&) = delete;
  ServiceDiscoverySharedClient& operator=(const ServiceDiscoverySharedClient&) =
      delete;

 protected:
  ServiceDiscoverySharedClient();
  ~ServiceDiscoverySharedClient() override;

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<ServiceDiscoverySharedClient>;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_SHARED_CLIENT_H_
