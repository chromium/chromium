// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_mount_provider.h"

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/bruschetta/fake_bruschetta_launcher.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bruschetta {

class BruschettaMountProviderTest : public testing::Test {
 protected:
  BruschettaMountProviderTest() {
    BruschettaMountProvider provider{&profile_, id_};

    guest_os::GuestOsSessionTrackerFactory::GetForProfile(&profile_)
        ->AddGuestForTesting(id_, info_);
    std::unique_ptr<FakeBruschettaLauncher> launcher =
        std::make_unique<FakeBruschettaLauncher>();
    launcher_ = launcher.get();
    BruschettaServiceFactory::GetForProfile(&profile_)->SetLauncherForTesting(
        id_.vm_name, std::move(launcher));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  guest_os::GuestId id_{guest_os::VmType::BRUSCHETTA, "vm_name", ""};
  guest_os::GuestInfo info_{id_, 32, "username", base::FilePath("/home/dir"),
                            "",  123};
  raw_ptr<FakeBruschettaLauncher> launcher_;
  BruschettaMountProvider provider_{&profile_, id_};
};

TEST_F(BruschettaMountProviderTest, TestPrepareLaunchFailure) {
  launcher_->set_ensure_running_result(BruschettaResult::kStartVmFailed);
  bool called = false;
  provider_.Prepare(base::BindLambdaForTesting(
      [&called](bool result, int cid, int port, base::FilePath path) {
        EXPECT_FALSE(result);
        called = true;
      }));
  EXPECT_TRUE(called);
}

TEST_F(BruschettaMountProviderTest, TestPrepare) {
  launcher_->set_ensure_running_result(BruschettaResult::kSuccess);
  bool called = false;
  provider_.Prepare(base::BindLambdaForTesting(
      [this, &called](bool result, int cid, int port, base::FilePath path) {
        EXPECT_TRUE(result);
        EXPECT_EQ(cid, info_.cid);
        EXPECT_EQ(port, 123);
        EXPECT_EQ(path, info_.homedir);
        called = true;
      }));
  EXPECT_TRUE(called);
}

}  // namespace bruschetta
