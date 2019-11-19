// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_CHROME_VARIATIONS_SERVICE_CLIENT_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_CHROME_VARIATIONS_SERVICE_CLIENT_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "components/variations/service/variations_service_client.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

// ChromeVariationsServiceClient provides an implementation of
// VariationsServiceClient that depends on chrome/.
class ChromeVariationsServiceClient
    : public variations::VariationsServiceClient {
 public:
  ChromeVariationsServiceClient();
  ~ChromeVariationsServiceClient() override;

  // variations::VariationsServiceClient:
  base::Callback<base::Version(void)> GetVersionForSimulationCallback()
      override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  bool OverridesRestrictParameter(std::string* parameter) override;
  bool IsEnterprise() override;

 private:
  // variations::VariationsServiceClient:
  version_info::Channel GetChannel() override;

  DISALLOW_COPY_AND_ASSIGN(ChromeVariationsServiceClient);
};

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_CHROME_VARIATIONS_SERVICE_CLIENT_H_
