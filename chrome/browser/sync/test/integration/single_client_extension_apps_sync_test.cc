// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/driver/sync_service_impl.h"
#include "content/public/test/browser_test.h"

using apps_helper::AllProfilesHaveSameApps;
using apps_helper::InstallHostedApp;
using apps_helper::InstallPlatformApp;

class SingleClientExtensionAppsSyncTest : public SyncTest {
 public:
  SingleClientExtensionAppsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientExtensionAppsSyncTest() override = default;

  bool UseVerifier() override {
    // TODO(crbug.com/1137717): rewrite tests to not use verifier profile.
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientExtensionAppsSyncTest, StartWithNoApps) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_F(SingleClientExtensionAppsSyncTest,
                       StartWithSomeLegacyApps) {
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 2;
  for (int i = 0; i < kNumApps; ++i) {
    InstallHostedApp(GetProfile(0), i);
    InstallHostedApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_F(SingleClientExtensionAppsSyncTest,
                       StartWithSomePlatformApps) {
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 2;
  for (int i = 0; i < kNumApps; ++i) {
    InstallPlatformApp(GetProfile(0), i);
    InstallPlatformApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_F(SingleClientExtensionAppsSyncTest,
                       InstallSomeLegacyApps) {
  ASSERT_TRUE(SetupSync());

  const int kNumApps = 2;
  for (int i = 0; i < kNumApps; ++i) {
    InstallHostedApp(GetProfile(0), i);
    InstallHostedApp(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_F(SingleClientExtensionAppsSyncTest,
                       InstallSomePlatformApps) {
  ASSERT_TRUE(SetupSync());

  const int kNumApps = 2;
  for (int i = 0; i < kNumApps; ++i) {
    InstallPlatformApp(GetProfile(0), i);
    InstallPlatformApp(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_F(SingleClientExtensionAppsSyncTest, InstallSomeApps) {
  ASSERT_TRUE(SetupSync());

  // TODO(crbug.com/1124986): Determine if these values
  // can be raised without introducing flakiness.
  const int kNumApps = 1;
  const int kNumPlatformApps = 1;

  int i = 0;

  for (int j = 0; j < kNumApps; ++i, ++j) {
    InstallHostedApp(GetProfile(0), i);
    InstallHostedApp(verifier(), i);
  }

  for (int j = 0; j < kNumPlatformApps; ++i, ++j) {
    InstallPlatformApp(GetProfile(0), i);
    InstallPlatformApp(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}
