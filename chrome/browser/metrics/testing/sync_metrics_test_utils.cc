// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/testing/sync_metrics_test_utils.h"

#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "components/sync/test/fake_server_network_resources.h"

namespace metrics {
namespace test {

std::unique_ptr<SyncServiceImplHarness> InitializeProfileForSync(
    Profile* profile,
    base::WeakPtr<fake_server::FakeServer> fake_server) {
  DCHECK(profile);

  SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(profile)
      ->OverrideNetworkForTest(
          fake_server::CreateFakeServerHttpPostProviderFactory(
              fake_server->AsWeakPtr()));

  return SyncServiceImplHarness::Create(
      profile, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
}

}  // namespace test
}  // namespace metrics
