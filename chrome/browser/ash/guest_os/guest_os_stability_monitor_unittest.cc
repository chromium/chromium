// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_stability_monitor.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/chunneld/fake_chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/fake_seneschal_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace guest_os {

class GuestOsStabilityMonitorTest : public testing::Test {
 public:
  GuestOsStabilityMonitorTest() : task_env_() {
    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();

    // CrostiniManager will create a GuestOsStabilityMonitor for us.
    profile_ = std::make_unique<TestingProfile>();
    crostini_manager_ =
        std::make_unique<crostini::CrostiniManager>(profile_.get());
    crostini::CrostiniTestHelper::EnableCrostini(profile_.get());

    // When CrostiniStabilityMonitor is initialized, it waits for the DBus
    // services to become available before monitoring them. In tests this
    // happens instantly, but the notification still comes via a callback on the
    // task queue, so run all queued tasks here.
    FlushTaskQueue();

    histogram_tester_.ExpectTotalCount(crostini::kCrostiniStabilityHistogram,
                                       0);
  }

  ~GuestOsStabilityMonitorTest() override {
    crostini::CrostiniTestHelper::DisableCrostini(profile_.get());
    crostini_manager_.reset();
    profile_.reset();
    ash::SeneschalClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }

  // Run all tasks queued prior to this method being called, but not any tasks
  // that are scheduled as a result of those tasks running. This is done by
  // placing a quit closure at the current end of the queue and running until we
  // hit it.
  void FlushTaskQueue() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  void SendVmStoppedSignal() {
    auto* concierge_client = ash::FakeConciergeClient::Get();

    vm_tools::concierge::VmStoppedSignal signal;
    signal.set_name("termina");
    signal.set_owner_id(crostini::CryptohomeIdForProfile(profile_.get()));
    concierge_client->NotifyVmStopped(signal);
  }

 protected:
  // CrostiniManager requires a full browser task environment to run.
  content::BrowserTaskEnvironment task_env_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<crostini::CrostiniManager> crostini_manager_;
  base::HistogramTester histogram_tester_;
};

TEST_F(GuestOsStabilityMonitorTest, ConciergeFailure) {
  auto* concierge_client = ash::FakeConciergeClient::Get();

  concierge_client->NotifyConciergeStopped();
  histogram_tester_.ExpectUniqueSample(crostini::kCrostiniStabilityHistogram,
                                       FailureClasses::ConciergeStopped, 1);

  concierge_client->NotifyConciergeStarted();
  histogram_tester_.ExpectUniqueSample(crostini::kCrostiniStabilityHistogram,
                                       FailureClasses::ConciergeStopped, 1);
}

TEST_F(GuestOsStabilityMonitorTest, CiceroneFailure) {
  auto* cicerone_client = ash::FakeCiceroneClient::Get();

  cicerone_client->NotifyCiceroneStopped();
  histogram_tester_.ExpectUniqueSample(crostini::kCrostiniStabilityHistogram,
                                       FailureClasses::CiceroneStopped, 1);

  cicerone_client->NotifyCiceroneStarted();
  histogram_tester_.ExpectUniqueSample(crostini::kCrostiniStabilityHistogram,
                                       FailureClasses::CiceroneStopped, 1);
}

TEST_F(GuestOsStabilityMonitorTest, SeneschalFailure) {
  auto* seneschal_client = ash::FakeSeneschalClient::Get();

  seneschal_client->NotifySeneschalStopped();
  histogram_tester_.ExpectUniqueSample(crostini::kCrostiniStabilityHistogram,
                                       FailureClasses::SeneschalStopped, 1);

  seneschal_client->NotifySeneschalStarted();
  histogram_tester_.ExpectUniqueSample(crostini::kCrostiniStabilityHistogram,
                                       FailureClasses::SeneschalStopped, 1);
}

TEST_F(GuestOsStabilityMonitorTest, ChunneldFailure) {
  auto* chunneld_client =
      static_cast<ash::FakeChunneldClient*>(ash::ChunneldClient::Get());

  chunneld_client->NotifyChunneldStopped();
  histogram_tester_.ExpectUniqueSample(crostini::kCrostiniStabilityHistogram,
                                       FailureClasses::ChunneldStopped, 1);

  chunneld_client->NotifyChunneldStarted();
  histogram_tester_.ExpectUniqueSample(crostini::kCrostiniStabilityHistogram,
                                       FailureClasses::ChunneldStopped, 1);
}

TEST_F(GuestOsStabilityMonitorTest, UnknownVmStopped) {
  SendVmStoppedSignal();
  histogram_tester_.ExpectUniqueSample(crostini::kCrostiniStabilityHistogram,
                                       FailureClasses::VmStopped, 0);
}

TEST_F(GuestOsStabilityMonitorTest, VmStoppedDuringStartup) {
  crostini_manager_->AddRunningVmForTesting("termina");
  crostini_manager_->UpdateVmState("termina", crostini::VmState::STARTING);

  SendVmStoppedSignal();
  histogram_tester_.ExpectUniqueSample(crostini::kCrostiniStabilityHistogram,
                                       FailureClasses::VmStopped, 0);
}

TEST_F(GuestOsStabilityMonitorTest, ExpectedVmStopped) {
  crostini_manager_->AddStoppingVmForTesting("termina");

  SendVmStoppedSignal();
  histogram_tester_.ExpectUniqueSample(crostini::kCrostiniStabilityHistogram,
                                       FailureClasses::VmStopped, 0);
}

TEST_F(GuestOsStabilityMonitorTest, UnexpectedVmStopped) {
  crostini_manager_->AddRunningVmForTesting("termina");

  SendVmStoppedSignal();
  histogram_tester_.ExpectUniqueSample(crostini::kCrostiniStabilityHistogram,
                                       FailureClasses::VmStopped, 1);
}

}  // namespace guest_os
