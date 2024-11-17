// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_context.h"

#include <memory>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_launch_options.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service_fake.h"
#include "chrome/browser/ash/borealis/borealis_shutdown_monitor.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/borealis/testing/apps.h"
#include "chrome/browser/ash/borealis/testing/windows.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_stability_monitor.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/fake_chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/fake_seneschal_client.h"
#include "components/exo/shell_surface_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {

class BorealisContextTest : public testing::Test,
                            protected guest_os::FakeVmServicesHelper {
 public:
  BorealisContextTest() {
    profile_ = std::make_unique<TestingProfile>();
    borealis_shutdown_monitor_ =
        std::make_unique<BorealisShutdownMonitor>(profile_.get());
    borealis_window_manager_ =
        std::make_unique<BorealisWindowManager>(profile_.get());

    features_ = std::make_unique<BorealisFeatures>(profile_.get());
    service_fake_ = BorealisServiceFake::UseFakeForTesting(profile_.get());
    service_fake_->SetFeaturesForTesting(features_.get());
    service_fake_->SetShutdownMonitorForTesting(
        borealis_shutdown_monitor_.get());
    service_fake_->SetWindowManagerForTesting(borealis_window_manager_.get());

    borealis_context_ =
        BorealisContext::CreateBorealisContextForTesting(profile_.get());

    // When GuestOsStabilityMonitor is initialized, it waits for the DBus
    // services to become available before monitoring them. In tests this
    // happens instantly, but the notification still comes via a callback on the
    // task queue, so run all queued tasks here.
    FlushTaskQueue();

    histogram_tester_.ExpectTotalCount(kBorealisStabilityHistogram, 0);
  }

  ~BorealisContextTest() override {
    borealis_context_.reset();  // must destroy before DBusThreadManager
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

 protected:
  content::BrowserTaskEnvironment task_env_;
  std::unique_ptr<borealis::BorealisContext> borealis_context_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<BorealisFeatures> features_;
  raw_ptr<BorealisServiceFake> service_fake_;
  std::unique_ptr<BorealisShutdownMonitor> borealis_shutdown_monitor_;
  std::unique_ptr<BorealisWindowManager> borealis_window_manager_;
  base::HistogramTester histogram_tester_;
  ash::TestNewWindowDelegate new_window_delegate_;
};

TEST_F(BorealisContextTest, ConciergeFailure) {
  auto* concierge_client = ash::FakeConciergeClient::Get();

  concierge_client->NotifyConciergeStopped();
  histogram_tester_.ExpectUniqueSample(
      kBorealisStabilityHistogram, guest_os::FailureClasses::ConciergeStopped,
      1);

  concierge_client->NotifyConciergeStarted();
  histogram_tester_.ExpectUniqueSample(
      kBorealisStabilityHistogram, guest_os::FailureClasses::ConciergeStopped,
      1);
}

TEST_F(BorealisContextTest, CiceroneFailure) {
  auto* cicerone_client = ash::FakeCiceroneClient::Get();

  cicerone_client->NotifyCiceroneStopped();
  histogram_tester_.ExpectUniqueSample(
      kBorealisStabilityHistogram, guest_os::FailureClasses::CiceroneStopped,
      1);

  cicerone_client->NotifyCiceroneStarted();
  histogram_tester_.ExpectUniqueSample(
      kBorealisStabilityHistogram, guest_os::FailureClasses::CiceroneStopped,
      1);
}

TEST_F(BorealisContextTest, SeneschalFailure) {
  auto* seneschal_client = ash::FakeSeneschalClient::Get();

  seneschal_client->NotifySeneschalStopped();
  histogram_tester_.ExpectUniqueSample(
      kBorealisStabilityHistogram, guest_os::FailureClasses::SeneschalStopped,
      1);

  seneschal_client->NotifySeneschalStarted();
  histogram_tester_.ExpectUniqueSample(
      kBorealisStabilityHistogram, guest_os::FailureClasses::SeneschalStopped,
      1);
}

TEST_F(BorealisContextTest, ChunneldFailure) {
  auto* chunneld_client =
      static_cast<ash::FakeChunneldClient*>(ash::ChunneldClient::Get());

  chunneld_client->NotifyChunneldStopped();
  histogram_tester_.ExpectUniqueSample(
      kBorealisStabilityHistogram, guest_os::FailureClasses::ChunneldStopped,
      1);

  chunneld_client->NotifyChunneldStarted();
  histogram_tester_.ExpectUniqueSample(
      kBorealisStabilityHistogram, guest_os::FailureClasses::ChunneldStopped,
      1);
}

}  // namespace borealis
