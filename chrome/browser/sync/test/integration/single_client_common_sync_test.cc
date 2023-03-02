// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SingleClientCommonSyncTest : public SyncTest {
 public:
  SingleClientCommonSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientCommonSyncTest() override = default;
  SingleClientCommonSyncTest(const SingleClientCommonSyncTest&) = delete;
  SingleClientCommonSyncTest& operator=(const SingleClientCommonSyncTest&) =
      delete;

  // SyncTest overrides.
  bool UseArcPackage() override {
    // ARC_PACKAGE data type is deferred during browser startup by disabling it
    // in the model type controller. This may result in an additional GetUpdates
    // request if the data type gets ready after the configuration. As a
    // workaround, just disable it to prevent an additional GetUpdates request
    // after browser restart.
    return false;
  }
};

// Android doesn't currently support PRE_ tests, see crbug.com/1117345.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientCommonSyncTest,
                       PRE_ShouldNotIssueGetUpdatesOnBrowserRestart) {
  ASSERT_TRUE(SetupSync());
}

IN_PROC_BROWSER_TEST_F(SingleClientCommonSyncTest,
                       ShouldNotIssueGetUpdatesOnBrowserRestart) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  // Verify that there were no unexpected GetUpdates requests during Sync
  // initialization.
  // TODO(crbug.com/1418329): wait for invalidations to initialize and consider
  // making a Commit request. This would help to verify that there are no
  // unnecessary GetUpdates requests after browser restart.
  sync_pb::ClientToServerMessage message;
  EXPECT_FALSE(GetFakeServer()->GetLastGetUpdatesMessage(&message));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
