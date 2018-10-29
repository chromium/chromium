// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"

namespace {

using browser_sync::ProfileSyncService;

class SyncActiveChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncActiveChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied() override {
    return service()->GetTransportState() ==
           syncer::SyncService::TransportState::ACTIVE;
  }

  std::string GetDebugMessage() const override { return "Sync Active"; }
};

// This test verifies some basic functionality of local sync, used for roaming
// profiles (enterprise use-case).
class LocalSyncTest : public InProcessBrowserTest {
 protected:
  LocalSyncTest() {
    EXPECT_TRUE(local_sync_backend_dir_.CreateUniqueTempDir());
  }

  ~LocalSyncTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // By default on Window OS local sync backend uses roaming profile. It can
    // lead to problems if some tests run simultaneously and use the same
    // roaming profile.
    auto file = local_sync_backend_dir_.GetPath().Append(
        FILE_PATH_LITERAL("profile.pb"));
    command_line->AppendSwitchASCII(switches::kLocalSyncBackendDir,
                                    file.MaybeAsASCII());
    command_line->AppendSwitch(switches::kEnableLocalSyncBackend);
    command_line->AppendSwitchASCII(
        switches::kSyncDeferredStartupTimeoutSeconds, "1");
  }

 private:
  base::ScopedTempDir local_sync_backend_dir_;
  DISALLOW_COPY_AND_ASSIGN(LocalSyncTest);
};

// The local sync backend is currently only supported on Windows.
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(LocalSyncTest, ShouldStart) {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(browser()->profile());

  // Wait until the first sync cycle is completed.
  ASSERT_TRUE(SyncActiveChecker(service).Wait());

  EXPECT_TRUE(service->IsLocalSyncEnabled());
}
#endif  // defined(OS_WIN)

}  // namespace
