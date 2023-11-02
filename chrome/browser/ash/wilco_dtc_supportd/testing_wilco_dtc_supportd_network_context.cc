// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wilco_dtc_supportd/testing_wilco_dtc_supportd_network_context.h"

#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace ash {

TestingWilcoDtcSupportdNetworkContext::TestingWilcoDtcSupportdNetworkContext() =
    default;

TestingWilcoDtcSupportdNetworkContext::
    ~TestingWilcoDtcSupportdNetworkContext() = default;

network::mojom::URLLoaderFactory*
TestingWilcoDtcSupportdNetworkContext::GetURLLoaderFactory() {
  return &test_url_loader_factory_;
}

network::TestURLLoaderFactory*
TestingWilcoDtcSupportdNetworkContext::test_url_loader_factory() {
  return &test_url_loader_factory_;
}

}  // namespace ash
