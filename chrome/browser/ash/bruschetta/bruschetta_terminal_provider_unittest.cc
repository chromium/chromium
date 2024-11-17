// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_terminal_provider.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/extensions/api/terminal/startup_status.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "fake_bruschetta_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bruschetta {

class BruschettaTerminalProviderTest : public testing::Test {
 public:
  BruschettaTerminalProviderTest() {
    std::unique_ptr<FakeBruschettaLauncher> launcher =
        std::make_unique<FakeBruschettaLauncher>();
    launcher_ = launcher.get();
    BruschettaServiceFactory::GetForProfile(&profile_)->SetLauncherForTesting(
        "vm_name", std::move(launcher));
  }
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  raw_ptr<FakeBruschettaLauncher> launcher_;
  base::RunLoop run_loop_;
};

TEST_F(BruschettaTerminalProviderTest, TestEnsureRunningLaunchesVm) {
  BruschettaTerminalProvider provider{
      &profile_, guest_os::GuestId(guest_os::VmType::UNKNOWN, "vm_name", "")};
  auto printer = std::make_unique<extensions::StartupStatusPrinter>(
      base::DoNothing(), false);
  auto status = provider.CreateStartupStatus(std::move(printer));
  bool called = false;
  provider.EnsureRunning(status.get(), base::BindLambdaForTesting(
                                           [this, &called](bool, std::string) {
                                             called = true;
                                             run_loop_.Quit();
                                           }));
  run_loop_.Run();
  ASSERT_TRUE(called);
}
}  // namespace bruschetta
