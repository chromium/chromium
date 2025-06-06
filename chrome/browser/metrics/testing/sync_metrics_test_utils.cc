// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/testing/sync_metrics_test_utils.h"

#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"

namespace metrics {
namespace test {

// TODO(https://crbug.com/41451146): Remove this function as it is trivial.
std::unique_ptr<SyncServiceImplHarness> InitializeProfileForSync(
    Profile* profile,
    base::WeakPtr<fake_server::FakeServer> fake_server) {
  DCHECK(profile);

  return SyncServiceImplHarness::Create(
      profile, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
}

}  // namespace test
}  // namespace metrics
