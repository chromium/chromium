// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WILCO_DTC_SUPPORTD_TESTING_WILCO_DTC_SUPPORTD_NETWORK_CONTEXT_H_
#define CHROME_BROWSER_ASH_WILCO_DTC_SUPPORTD_TESTING_WILCO_DTC_SUPPORTD_NETWORK_CONTEXT_H_

#include "base/macros.h"
#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_network_context.h"
#include "services/network/test/test_url_loader_factory.h"

namespace network {
namespace mojom {

class URLLoaderFactory;

}  // namespace mojom
}  // namespace network

namespace chromeos {

class TestingWilcoDtcSupportdNetworkContext
    : public WilcoDtcSupportdNetworkContext {
 public:
  TestingWilcoDtcSupportdNetworkContext();
  ~TestingWilcoDtcSupportdNetworkContext() override;

  // WilcoDtcSupportdNetworkContext overrides:
  network::mojom::URLLoaderFactory* GetURLLoaderFactory() override;

  network::TestURLLoaderFactory* test_url_loader_factory();

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestingWilcoDtcSupportdNetworkContext);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_WILCO_DTC_SUPPORTD_TESTING_WILCO_DTC_SUPPORTD_NETWORK_CONTEXT_H_
