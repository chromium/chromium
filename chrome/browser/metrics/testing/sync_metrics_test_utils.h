// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TESTING_SYNC_METRICS_TEST_UTILS_H_
#define CHROME_BROWSER_METRICS_TESTING_SYNC_METRICS_TEST_UTILS_H_

#include "base/memory/weak_ptr.h"
#include "components/sync/test/fake_server.h"

// Helpers to support sync in metrics browser tests.

class Profile;
class SyncServiceImplHarness;

namespace fake_server {
class FakeServer;
}

namespace metrics {
namespace test {

// Initializes and enables the test Sync service of the |profile|.
std::unique_ptr<SyncServiceImplHarness> InitializeProfileForSync(
    Profile* profile,
    base::WeakPtr<fake_server::FakeServer> fake_server);

}  // namespace test
}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_TESTING_SYNC_METRICS_TEST_UTILS_H_
