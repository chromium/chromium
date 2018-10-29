// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/guid.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using sessions_helper::CheckInitialState;
using sessions_helper::DeleteForeignSession;
using sessions_helper::GetLocalWindows;
using sessions_helper::GetSessionData;
using sessions_helper::NavigateTab;
using sessions_helper::OpenTab;
using sessions_helper::OpenTabAtIndex;
using sessions_helper::ScopedWindowMap;
using sessions_helper::SessionWindowMap;
using sessions_helper::SyncedSessionVector;
using sessions_helper::WindowsMatch;

class TwoClientSessionsSyncTest : public FeatureToggler, public SyncTest {
 public:
  TwoClientSessionsSyncTest()
      : FeatureToggler(switches::kSyncUSSSessions), SyncTest(TWO_CLIENT) {}
  ~TwoClientSessionsSyncTest() override {}

  void WaitForWindowsInForeignSession(int index, ScopedWindowMap windows) {
    std::vector<ScopedWindowMap> expected_windows(1);
    expected_windows[0] = std::move(windows);
    EXPECT_TRUE(ForeignSessionsMatchChecker(index, expected_windows).Wait());
  }

  void WaitForForeignSessionsToSync(int local_index, int non_local_index) {
    ScopedWindowMap client_windows;
    ASSERT_TRUE(GetLocalWindows(local_index, &client_windows));
    WaitForWindowsInForeignSession(non_local_index, std::move(client_windows));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientSessionsSyncTest);
};

static const char* kURL1 = "data:text/html,<html><title>Test</title></html>";
static const char* kURL2 = "data:text/html,<html><title>Test2</title></html>";
static const char* kURL3 = "data:text/html,<html><title>Test3</title></html>";
static const char* kURL4 = "data:text/html,<html><title>Test4</title></html>";
static const char* kURLTemplate =
    "data:text/html,<html><title>Test%s</title></html>";

// TODO(zea): Test each individual session command we care about separately.
// (as well as multi-window). We're currently only checking basic single-window/
// single-tab functionality.

IN_PROC_BROWSER_TEST_P(TwoClientSessionsSyncTest,
                       E2E_ENABLED(SingleClientChanged)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Open tab and access a url on client 0
  ScopedWindowMap client0_windows;
  std::string url =
      base::StringPrintf(kURLTemplate, base::GenerateGUID().c_str());

  ASSERT_TRUE(OpenTab(0, GURL(url)));
  WaitForForeignSessionsToSync(0, 1);
}

IN_PROC_BROWSER_TEST_P(TwoClientSessionsSyncTest, E2E_ENABLED(AllChanged)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Open tabs on all clients and retain window information.
  std::vector<ScopedWindowMap> client_windows(num_clients());
  for (int i = 0; i < num_clients(); ++i) {
    ScopedWindowMap windows;
    std::string url =
        base::StringPrintf(kURLTemplate, base::GenerateGUID().c_str());
    ASSERT_TRUE(OpenTab(i, GURL(url)));
    ASSERT_TRUE(GetLocalWindows(i, &windows));
    client_windows[i] = std::move(windows);
  }

  // Get foreign session data from all clients and check it against all
  // client_windows.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_TRUE(ForeignSessionsMatchChecker(i, client_windows).Wait());
  }
}

IN_PROC_BROWSER_TEST_P(TwoClientSessionsSyncTest,
                       SingleClientEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
}

IN_PROC_BROWSER_TEST_P(TwoClientSessionsSyncTest,
                       SingleClientEnabledEncryptionAndChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ASSERT_TRUE(OpenTab(0, GURL(kURL1)));
  ASSERT_TRUE(EnableEncryption(0));
  WaitForForeignSessionsToSync(0, 1);
}

IN_PROC_BROWSER_TEST_P(TwoClientSessionsSyncTest,
                       BothClientsEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(EnableEncryption(1));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
}

IN_PROC_BROWSER_TEST_P(TwoClientSessionsSyncTest, BothChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ASSERT_TRUE(OpenTab(0, GURL(kURL1)));
  ASSERT_TRUE(OpenTab(1, GURL(kURL2)));

  WaitForForeignSessionsToSync(0, 1);
  WaitForForeignSessionsToSync(1, 0);

  // Check that a navigation in client 0 is reflected on client 1.
  NavigateTab(0, GURL(kURL3));
  WaitForForeignSessionsToSync(0, 1);
}

IN_PROC_BROWSER_TEST_P(TwoClientSessionsSyncTest, DeleteIdleSession) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  // Client 0 opened some tabs then went idle.
  ASSERT_TRUE(OpenTab(0, GURL(kURL1)));
  WaitForForeignSessionsToSync(0, 1);

  // Get foreign session data from client 1.
  SyncedSessionVector sessions1;
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Client 1 now deletes client 0's tabs. This frees the memory of sessions1.
  DeleteForeignSession(1, sessions1[0]->session_tag);
  ASSERT_TRUE(GetClient(1)->HasUnsyncedItems());
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  EXPECT_FALSE(GetSessionData(1, &sessions1));
}

IN_PROC_BROWSER_TEST_P(TwoClientSessionsSyncTest, DeleteActiveSession) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  // Client 0 opened some tabs then went idle.
  ASSERT_TRUE(OpenTab(0, GURL(kURL1)));
  WaitForForeignSessionsToSync(0, 1);

  SyncedSessionVector sessions1;
  ASSERT_TRUE(GetSessionData(1, &sessions1));
  ASSERT_EQ(1U, sessions1.size());

  // Client 1 now deletes client 0's tabs. This frees the memory of sessions1.
  DeleteForeignSession(1, sessions1[0]->session_tag);
  ASSERT_TRUE(GetClient(1)->HasUnsyncedItems());
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_FALSE(GetSessionData(1, &sessions1));

  // Client 0 becomes active again with a new tab.
  ASSERT_TRUE(OpenTab(0, GURL(kURL2)));
  WaitForForeignSessionsToSync(0, 1);
  EXPECT_TRUE(GetSessionData(1, &sessions1));
}

IN_PROC_BROWSER_TEST_P(TwoClientSessionsSyncTest, MultipleWindowsMultipleTabs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  EXPECT_TRUE(OpenTab(0, GURL(kURL1)));
  EXPECT_TRUE(OpenTabAtIndex(0, 1, GURL(kURL2)));

  // Add a second browser for profile 0. This browser ends up in index 2.
  AddBrowser(0);
  EXPECT_TRUE(OpenTab(2, GURL(kURL3)));
  EXPECT_TRUE(OpenTabAtIndex(2, 1, GURL(kURL4)));

  WaitForForeignSessionsToSync(0, 1);
}

INSTANTIATE_TEST_CASE_P(USS,
                        TwoClientSessionsSyncTest,
                        ::testing::Values(false, true));

}  // namespace
