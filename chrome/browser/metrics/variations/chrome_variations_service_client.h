// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_CHROME_VARIATIONS_SERVICE_CLIENT_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_CHROME_VARIATIONS_SERVICE_CLIENT_H_

#include <string>

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

  ChromeVariationsServiceClient(const ChromeVariationsServiceClient&) = delete;
  ChromeVariationsServiceClient& operator=(
      const ChromeVariationsServiceClient&) = delete;

  ~ChromeVariationsServiceClient() override;

  // variations::VariationsServiceClient:
  base::Version GetVersionForSimulation() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  bool OverridesRestrictParameter(std::string* parameter) override;
  base::FilePath GetVariationsSeedFileDir() override;
  std::unique_ptr<variations::SeedResponse>
  TakeSeedFromNativeVariationsSeedStore() override;
  bool IsEnterprise() override;
  void RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      PrefService* local_state) override;

 private:
  // variations::VariationsServiceClient:
  version_info::Channel GetChannel() override;
};

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_CHROME_VARIATIONS_SERVICE_CLIENT_H_
