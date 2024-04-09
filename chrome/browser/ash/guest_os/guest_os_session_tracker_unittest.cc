// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"

#include <optional>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace guest_os {

class GuestOsSessionTrackerTest : public testing::Test,
                                  protected guest_os::FakeVmServicesHelper {
 public:
  GuestOsSessionTrackerTest() {
    vm_started_signal_.set_owner_id(OwnerId());
    vm_started_signal_.set_name("vm_name");

    vm_shutdown_signal_.set_name("vm_name");
    vm_shutdown_signal_.set_owner_id(OwnerId());

    container_started_signal_.set_vm_name("vm_name");
    container_started_signal_.set_owner_id(OwnerId());
    container_started_signal_.set_container_name("penguin");
    container_started_signal_.set_container_token(token_);

    container_shutdown_signal_.set_container_name("penguin");
    container_shutdown_signal_.set_vm_name("vm_name");
    container_shutdown_signal_.set_owner_id(OwnerId());
  }

  std::string OwnerId() {
    return ash::ProfileHelper::GetUserIdHashFromProfile(&profile_);
  }

  void CheckContainerNotExists() {
    auto info = tracker_.GetInfo(guest_id_);
    EXPECT_EQ(info, std::nullopt);

    auto id = tracker_.GetGuestIdForToken(token_);
    EXPECT_EQ(id, std::nullopt);
  }

  void CheckContainerExists() {
    auto info = tracker_.GetInfo(guest_id_);
    EXPECT_NE(info, std::nullopt);

    auto id = tracker_.GetGuestIdForToken(token_);
    EXPECT_EQ(id, guest_id_);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::RunLoop run_loop_;
  GuestOsSessionTracker tracker_{OwnerId()};
  const GuestId guest_id_{VmType::UNKNOWN, "vm_name", "penguin"};
  const std::string token_ = "test_container_token";

  vm_tools::concierge::VmStartedSignal vm_started_signal_;
  vm_tools::concierge::VmStoppedSignal vm_shutdown_signal_;
  vm_tools::cicerone::ContainerStartedSignal container_started_signal_;
  vm_tools::cicerone::ContainerShutdownSignal container_shutdown_signal_;
};

TEST_F(GuestOsSessionTrackerTest, ContainerAddedOnStartup) {
  vm_tools::concierge::VmStartedSignal signal;
  signal.set_owner_id(OwnerId());
  signal.set_name("vm_name");
  signal.mutable_vm_info()->set_cid(32);
  FakeConciergeClient()->NotifyVmStarted(signal);
  vm_tools::cicerone::ContainerStartedSignal cicerone_signal;
  cicerone_signal.set_container_name("penguin");
  cicerone_signal.set_owner_id(OwnerId());
  cicerone_signal.set_vm_name("vm_name");
  cicerone_signal.set_container_username("username");
  cicerone_signal.set_container_homedir("/home");
  cicerone_signal.set_ipv4_address("1.2.3.4");
  cicerone_signal.set_container_token(token_);
  FakeCiceroneClient()->NotifyContainerStarted(cicerone_signal);

  auto info = tracker_.GetInfo(guest_id_);

  EXPECT_EQ("vm_name", info->guest_id.vm_name);
  EXPECT_EQ("penguin", info->guest_id.container_name);
  EXPECT_EQ(cicerone_signal.container_username(), info->username);
  EXPECT_EQ(base::FilePath(cicerone_signal.container_homedir()), info->homedir);
  EXPECT_EQ(signal.vm_info().cid(), info->cid);
  EXPECT_EQ(cicerone_signal.ipv4_address(), info->ipv4_address);

  auto id = tracker_.GetGuestIdForToken(token_);
  EXPECT_EQ(id, guest_id_);
}

TEST_F(GuestOsSessionTrackerTest, ContainerRemovedOnContainerShutdown) {
  FakeConciergeClient()->NotifyVmStarted(vm_started_signal_);
  FakeCiceroneClient()->NotifyContainerStarted(container_started_signal_);
  CheckContainerExists();

  FakeCiceroneClient()->NotifyContainerShutdownSignal(
      container_shutdown_signal_);
  CheckContainerNotExists();
}

TEST_F(GuestOsSessionTrackerTest, ContainerRemovedOnVmShutdown) {
  FakeConciergeClient()->NotifyVmStarted(vm_started_signal_);
  FakeCiceroneClient()->NotifyContainerStarted(container_started_signal_);

  CheckContainerExists();

  FakeConciergeClient()->NotifyVmStopped(vm_shutdown_signal_);

  CheckContainerNotExists();
}

TEST_F(GuestOsSessionTrackerTest, AlreadyRunningVMsTracked) {
  vm_tools::concierge::ListVmsResponse response;
  vm_tools::concierge::ExtendedVmInfo vm_info;
  vm_info.set_owner_id(OwnerId());
  vm_info.set_name("vm_name");
  *response.add_vms() = vm_info;
  response.set_success(true);
  FakeConciergeClient()->set_list_vms_response(response);

  GuestOsSessionTracker tracker{OwnerId()};
  run_loop_.RunUntilIdle();

  FakeCiceroneClient()->NotifyContainerStarted(container_started_signal_);

  auto info = tracker.GetInfo(guest_id_);
  ASSERT_NE(info, std::nullopt);
  auto id = tracker.GetGuestIdForToken(token_);
  ASSERT_EQ(id, guest_id_);
}

TEST_F(GuestOsSessionTrackerTest, AlreadyRunningContainersTracked) {
  vm_tools::concierge::ListVmsResponse list_vms_response;
  vm_tools::concierge::ExtendedVmInfo vm_info;
  vm_info.set_owner_id(OwnerId());
  vm_info.set_name("vm_name");
  *list_vms_response.add_vms() = vm_info;
  list_vms_response.set_success(true);
  FakeConciergeClient()->set_list_vms_response(list_vms_response);

  vm_tools::cicerone::ListRunningContainersResponse list_containers_response;
  auto* container = list_containers_response.add_containers();
  container->set_vm_name("vm_name");
  container->set_container_name("penguin");
  container->set_container_token(token_);
  FakeCiceroneClient()->set_list_containers_response(list_containers_response);

  vm_tools::cicerone::GetGarconSessionInfoResponse garcon_response;
  garcon_response.set_container_homedir("/homedir");
  garcon_response.set_container_username("username");
  garcon_response.set_sftp_vsock_port(24);
  garcon_response.set_status(
      vm_tools::cicerone::GetGarconSessionInfoResponse::SUCCEEDED);
  FakeCiceroneClient()->set_get_garcon_session_info_response(garcon_response);

  GuestOsSessionTracker tracker{OwnerId()};
  run_loop_.RunUntilIdle();

  auto info = tracker.GetInfo(guest_id_);
  ASSERT_NE(std::nullopt, info);
  auto id = tracker.GetGuestIdForToken(token_);
  ASSERT_EQ(id, guest_id_);
  ASSERT_EQ(base::FilePath(garcon_response.container_homedir()), info->homedir);
  ASSERT_EQ(garcon_response.container_username(), info->username);
  ASSERT_EQ(garcon_response.sftp_vsock_port(), info->sftp_vsock_port);
}

TEST_F(GuestOsSessionTrackerTest, RunOnceContainerStartedAlreadyRunning) {
  FakeConciergeClient()->NotifyVmStarted(vm_started_signal_);
  FakeCiceroneClient()->NotifyContainerStarted(container_started_signal_);
  bool called = false;
  auto _ = tracker_.RunOnceContainerStarted(
      guest_id_,
      base::BindLambdaForTesting([&called](GuestInfo info) { called = true; }));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(GuestOsSessionTrackerTest, RunOnceContainerStartedDelayedStart) {
  FakeConciergeClient()->NotifyVmStarted(vm_started_signal_);
  bool called = false;
  auto _ = tracker_.RunOnceContainerStarted(
      guest_id_,
      base::BindLambdaForTesting([&called](GuestInfo info) { called = true; }));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(called);
  FakeCiceroneClient()->NotifyContainerStarted(container_started_signal_);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(GuestOsSessionTrackerTest, RunOnceContainerStartedCancel) {
  FakeConciergeClient()->NotifyVmStarted(vm_started_signal_);
  bool called = false;
  static_cast<void>(tracker_.RunOnceContainerStarted(
      guest_id_, base::BindLambdaForTesting(
                     [&called](GuestInfo info) { called = true; })));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(called);
  FakeCiceroneClient()->NotifyContainerStarted(container_started_signal_);
  task_environment_.RunUntilIdle();

  // We dropped the subscription, so it should've been cancelled straight away.
  EXPECT_FALSE(called);
}

TEST_F(GuestOsSessionTrackerTest, RunOnContainerShutdown) {
  FakeConciergeClient()->NotifyVmStarted(vm_started_signal_);
  FakeCiceroneClient()->NotifyContainerStarted(container_started_signal_);
  bool called = false;
  auto _ = tracker_.RunOnShutdown(
      guest_id_, base::BindLambdaForTesting([&called]() { called = true; }));
  FakeCiceroneClient()->NotifyContainerShutdownSignal(
      container_shutdown_signal_);
  EXPECT_TRUE(called);
}

TEST_F(GuestOsSessionTrackerTest, RunOnVmShutdown) {
  FakeConciergeClient()->NotifyVmStarted(vm_started_signal_);
  FakeCiceroneClient()->NotifyContainerStarted(container_started_signal_);
  bool called = false;
  auto _ = tracker_.RunOnShutdown(
      guest_id_, base::BindLambdaForTesting([&called]() { called = true; }));
  FakeConciergeClient()->NotifyVmStopped(vm_shutdown_signal_);
  EXPECT_TRUE(called);
}

TEST_F(GuestOsSessionTrackerTest, GetVmInfo) {
  ASSERT_EQ(std::nullopt, tracker_.GetVmInfo(vm_started_signal_.name()));

  FakeConciergeClient()->NotifyVmStarted(vm_started_signal_);
  ASSERT_NE(std::nullopt, tracker_.GetVmInfo(vm_started_signal_.name()));

  FakeCiceroneClient()->NotifyContainerStarted(container_started_signal_);
  ASSERT_NE(std::nullopt, tracker_.GetVmInfo(vm_started_signal_.name()));

  FakeCiceroneClient()->NotifyContainerShutdownSignal(
      container_shutdown_signal_);
  ASSERT_NE(std::nullopt, tracker_.GetVmInfo(vm_started_signal_.name()));

  FakeConciergeClient()->NotifyVmStopped(vm_shutdown_signal_);
  ASSERT_EQ(std::nullopt, tracker_.GetVmInfo(vm_started_signal_.name()));
}

TEST_F(GuestOsSessionTrackerTest, GetGuestIdForToken) {
  ASSERT_EQ(std::nullopt, tracker_.GetGuestIdForToken(token_));

  FakeConciergeClient()->NotifyVmStarted(vm_started_signal_);
  ASSERT_EQ(std::nullopt, tracker_.GetGuestIdForToken(token_));

  FakeCiceroneClient()->NotifyContainerStarted(container_started_signal_);
  ASSERT_EQ(guest_id_, tracker_.GetGuestIdForToken(token_));

  FakeCiceroneClient()->NotifyContainerShutdownSignal(
      container_shutdown_signal_);
  ASSERT_EQ(std::nullopt, tracker_.GetGuestIdForToken(token_));
}

TEST_F(GuestOsSessionTrackerTest, IsVmStopping) {
  vm_tools::concierge::VmStoppingSignal signal;
  signal.set_name("vm_name");
  FakeConciergeClient()->NotifyVmStopping(signal);
  ASSERT_TRUE(tracker_.IsVmStopping(signal.name()));

  FakeConciergeClient()->NotifyVmStopped(vm_shutdown_signal_);
  ASSERT_FALSE(tracker_.IsVmStopping(signal.name()));
}

}  // namespace guest_os
