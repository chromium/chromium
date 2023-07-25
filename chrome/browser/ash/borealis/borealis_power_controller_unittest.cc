// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_power_controller.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/testing/apps.h"
#include "chrome/browser/ash/borealis/testing/windows.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace borealis {
namespace {

class BorealisPowerControllerTest : public ChromeAshTestBase {
 protected:
  void SetUp() override {
    ChromeAshTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    CreateFakeMainApp(profile_.get());
    ash::CiceroneClient::InitializeFake();
    power_controller_ =
        std::make_unique<BorealisPowerController>(profile_.get());
    remote_provider_ =
        std::make_unique<mojo::Remote<device::mojom::WakeLockProvider>>();
    wake_lock_provider_ = std::make_unique<device::TestWakeLockProvider>();
    wake_lock_provider_->BindReceiver(
        remote_provider_->BindNewPipeAndPassReceiver());
    power_controller_->SetWakeLockProviderForTesting(
        std::move(*remote_provider_));
    owner_id_ = ash::ProfileHelper::GetUserIdHashFromProfile(profile_.get());
  }

  void TearDown() override {
    wake_lock_provider_.reset();
    remote_provider_.reset();
    power_controller_.reset();
    ChromeAshTestBase::TearDown();
  }

  // Returns the number of active wake locks.
  int CountActiveWakeLocks(device::mojom::WakeLockType type) {
    base::RunLoop run_loop;
    int result_count = 0;
    wake_lock_provider_->GetActiveWakeLocksForTests(
        type,
        base::BindOnce(
            [](base::RunLoop* run_loop, int* result_count, int32_t count) {
              *result_count = count;
              run_loop->Quit();
            },
            &run_loop, &result_count));
    run_loop.Run();
    return result_count;
  }

  // Flushes the outstanding messages on the wakelock and returns active wake
  // locks.
  int FlushAndCountWakeLocks(BorealisPowerController& power_controller,
                             device::mojom::WakeLockType type) {
    power_controller.FlushForTesting();
    return CountActiveWakeLocks(type);
  }

  std::string owner_id_;

  std::unique_ptr<mojo::Remote<device::mojom::WakeLockProvider>>
      remote_provider_;
  std::unique_ptr<device::TestWakeLockProvider> wake_lock_provider_;
  std::unique_ptr<BorealisPowerController> power_controller_;

 private:
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(BorealisPowerControllerTest, WakeLockToggledByInhibitUninhibit) {
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      0);
  vm_tools::cicerone::InhibitScreensaverSignal inhibit;
  inhibit.set_vm_name("borealis");
  inhibit.set_owner_id(owner_id_);
  inhibit.set_cookie(1);
  power_controller_->OnInhibitScreensaver(inhibit);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      1);

  vm_tools::cicerone::UninhibitScreensaverSignal uninhibit;
  uninhibit.set_vm_name("borealis");
  uninhibit.set_owner_id(owner_id_);
  uninhibit.set_cookie(1);
  power_controller_->OnUninhibitScreensaver(uninhibit);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      0);
}

TEST_F(BorealisPowerControllerTest, NonBorealisDoesNotWakeLock) {
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      0);
  vm_tools::cicerone::InhibitScreensaverSignal inhibit;
  inhibit.set_vm_name("notborealis");
  inhibit.set_cookie(1);
  power_controller_->OnInhibitScreensaver(inhibit);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      0);

  inhibit.set_vm_name("borealis");
  inhibit.set_owner_id(owner_id_);
  inhibit.set_cookie(1);
  power_controller_->OnInhibitScreensaver(inhibit);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      1);

  vm_tools::cicerone::UninhibitScreensaverSignal uninhibit;
  uninhibit.set_vm_name("notborealis");
  uninhibit.set_cookie(1);
  power_controller_->OnUninhibitScreensaver(uninhibit);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      1);
}

TEST_F(BorealisPowerControllerTest, MultipleInhibits) {
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      0);
  vm_tools::cicerone::InhibitScreensaverSignal inhibit;
  inhibit.set_vm_name("borealis");
  inhibit.set_owner_id(owner_id_);
  inhibit.set_cookie(1);
  power_controller_->OnInhibitScreensaver(inhibit);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      1);

  inhibit.set_cookie(2);
  power_controller_->OnInhibitScreensaver(inhibit);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      1);

  vm_tools::cicerone::UninhibitScreensaverSignal uninhibit;
  uninhibit.set_vm_name("borealis");
  uninhibit.set_owner_id(owner_id_);
  uninhibit.set_cookie(1);
  power_controller_->OnUninhibitScreensaver(uninhibit);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      1);

  uninhibit.set_cookie(2);
  power_controller_->OnUninhibitScreensaver(uninhibit);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      0);
}

TEST_F(BorealisPowerControllerTest, UninhibitWithoutInhibit) {
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      0);
  vm_tools::cicerone::UninhibitScreensaverSignal uninhibit;
  uninhibit.set_vm_name("borealis");
  uninhibit.set_owner_id(owner_id_);
  uninhibit.set_cookie(1);
  power_controller_->OnUninhibitScreensaver(uninhibit);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      0);
}

TEST_F(BorealisPowerControllerTest, WakeLockToggledByDownloadInhibitUninhibit) {
  EXPECT_EQ(FlushAndCountWakeLocks(
                *power_controller_,
                device::mojom::WakeLockType::kPreventAppSuspension),
            0);
  vm_tools::cicerone::InhibitScreensaverSignal inhibit;
  inhibit.set_vm_name("borealis");
  inhibit.set_owner_id(owner_id_);
  inhibit.set_reason("download");
  inhibit.set_cookie(1);
  power_controller_->OnInhibitScreensaver(inhibit);
  EXPECT_EQ(FlushAndCountWakeLocks(
                *power_controller_,
                device::mojom::WakeLockType::kPreventAppSuspension),
            1);

  vm_tools::cicerone::UninhibitScreensaverSignal uninhibit;
  uninhibit.set_vm_name("borealis");
  uninhibit.set_owner_id(owner_id_);
  uninhibit.set_cookie(1);
  power_controller_->OnUninhibitScreensaver(uninhibit);
  EXPECT_EQ(FlushAndCountWakeLocks(
                *power_controller_,
                device::mojom::WakeLockType::kPreventAppSuspension),
            0);
}

TEST_F(BorealisPowerControllerTest, BothInhibitTypesActive) {
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      0);
  EXPECT_EQ(FlushAndCountWakeLocks(
                *power_controller_,
                device::mojom::WakeLockType::kPreventAppSuspension),
            0);

  vm_tools::cicerone::InhibitScreensaverSignal inhibit;
  inhibit.set_vm_name("borealis");
  inhibit.set_owner_id(owner_id_);
  inhibit.set_reason("not download");
  inhibit.set_cookie(1);
  power_controller_->OnInhibitScreensaver(inhibit);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      1);

  inhibit.set_reason("download");
  inhibit.set_cookie(2);
  power_controller_->OnInhibitScreensaver(inhibit);
  EXPECT_EQ(FlushAndCountWakeLocks(
                *power_controller_,
                device::mojom::WakeLockType::kPreventAppSuspension),
            1);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      1);

  vm_tools::cicerone::UninhibitScreensaverSignal uninhibit;
  uninhibit.set_vm_name("borealis");
  uninhibit.set_owner_id(owner_id_);
  uninhibit.set_cookie(1);
  power_controller_->OnUninhibitScreensaver(uninhibit);
  EXPECT_EQ(FlushAndCountWakeLocks(
                *power_controller_,
                device::mojom::WakeLockType::kPreventAppSuspension),
            1);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      0);

  inhibit.set_reason("not download");
  inhibit.set_cookie(3);
  power_controller_->OnInhibitScreensaver(inhibit);
  EXPECT_EQ(FlushAndCountWakeLocks(
                *power_controller_,
                device::mojom::WakeLockType::kPreventAppSuspension),
            1);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      1);

  uninhibit.set_cookie(2);
  power_controller_->OnUninhibitScreensaver(uninhibit);
  EXPECT_EQ(FlushAndCountWakeLocks(
                *power_controller_,
                device::mojom::WakeLockType::kPreventAppSuspension),
            0);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      1);

  uninhibit.set_cookie(3);
  power_controller_->OnUninhibitScreensaver(uninhibit);
  EXPECT_EQ(FlushAndCountWakeLocks(
                *power_controller_,
                device::mojom::WakeLockType::kPreventAppSuspension),
            0);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      0);
}

TEST_F(BorealisPowerControllerTest, WakeLockResetWhenBorealisShutsdown) {
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      0);
  vm_tools::cicerone::InhibitScreensaverSignal inhibit;
  inhibit.set_vm_name("borealis");
  inhibit.set_owner_id(owner_id_);
  inhibit.set_cookie(1);
  power_controller_->OnInhibitScreensaver(inhibit);
  EXPECT_EQ(
      FlushAndCountWakeLocks(*power_controller_,
                             device::mojom::WakeLockType::kPreventDisplaySleep),
      1);

  EXPECT_EQ(FlushAndCountWakeLocks(
                *power_controller_,
                device::mojom::WakeLockType::kPreventAppSuspension),
            0);
  inhibit.set_reason("download");
  inhibit.set_cookie(2);
  power_controller_->OnInhibitScreensaver(inhibit);
  EXPECT_EQ(FlushAndCountWakeLocks(
                *power_controller_,
                device::mojom::WakeLockType::kPreventAppSuspension),
            1);

  power_controller_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      CountActiveWakeLocks(device::mojom::WakeLockType::kPreventDisplaySleep),
      0);
  EXPECT_EQ(
      CountActiveWakeLocks(device::mojom::WakeLockType::kPreventAppSuspension),
      0);
}

}  // namespace
}  // namespace borealis
