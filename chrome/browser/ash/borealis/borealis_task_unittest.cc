// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_task.h"

#include <memory>

#include "base/test/test_future.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

using ::testing::_;
using ::testing::StrNe;

namespace borealis {

namespace {

using CallbackFactory =
    NiceCallbackFactory<void(BorealisStartupResult, std::string)>;

class BorealisTasksTest : public testing::Test,
                          protected guest_os::FakeVmServicesHelper {
 public:
  BorealisTasksTest() = default;
  ~BorealisTasksTest() override = default;

  // Disallow copy and assign.
  BorealisTasksTest(const BorealisTasksTest&) = delete;
  BorealisTasksTest& operator=(const BorealisTasksTest&) = delete;

 protected:
  void SetUp() override {
    CreateProfile();
    context_ = BorealisContext::CreateBorealisContextForTesting(profile_.get());
    context_->set_vm_name("borealis");
  }

  void TearDown() override {
    context_.reset();  // must destroy before DBus shutdown
    profile_.reset();
  }

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<BorealisContext> context_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  void CreateProfile() {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("defaultprofile");
    profile_ = profile_builder.Build();
  }
};

TEST_F(BorealisTasksTest, MountDlcSucceedsAndCallbackRanWithResults) {
  FakeDlcserviceClient()->set_install_error(dlcservice::kErrorNone);

  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(BorealisStartupResult::kSuccess, _));

  MountDlc task;
  task.Run(context_.get(), callback_factory.BindOnce());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisTasksTest, CreateDiskSucceedsAndCallbackRanWithResults) {
  vm_tools::concierge::CreateDiskImageResponse response;
  base::FilePath path = base::FilePath("test/path");
  response.set_status(vm_tools::concierge::DISK_STATUS_CREATED);
  response.set_disk_path(path.AsUTF8Unsafe());
  FakeConciergeClient()->set_create_disk_image_response(std::move(response));
  EXPECT_EQ(context_->disk_path(), base::FilePath());

  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(BorealisStartupResult::kSuccess, _));

  CreateDiskImage task;
  task.Run(context_.get(), callback_factory.BindOnce());
  task_environment_.RunUntilIdle();

  EXPECT_GE(FakeConciergeClient()->create_disk_image_call_count(), 1);
  EXPECT_EQ(context_->disk_path(), path);
}

TEST_F(BorealisTasksTest,
       CreateDiskImageAlreadyExistsAndCallbackRanWithResults) {
  vm_tools::concierge::CreateDiskImageResponse response;
  base::FilePath path = base::FilePath("test/path");
  response.set_status(vm_tools::concierge::DISK_STATUS_EXISTS);
  response.set_disk_path(path.AsUTF8Unsafe());
  FakeConciergeClient()->set_create_disk_image_response(std::move(response));
  EXPECT_EQ(context_->disk_path(), base::FilePath());

  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(BorealisStartupResult::kSuccess, _));

  CreateDiskImage task;
  task.Run(context_.get(), callback_factory.BindOnce());
  task_environment_.RunUntilIdle();

  EXPECT_GE(FakeConciergeClient()->create_disk_image_call_count(), 1);
  EXPECT_EQ(context_->disk_path(), path);
}

TEST_F(BorealisTasksTest, StartBorealisVmSucceedsAndCallbackRanWithResults) {
  vm_tools::concierge::StartVmResponse response;
  response.set_status(vm_tools::concierge::VM_STATUS_STARTING);
  FakeConciergeClient()->set_start_vm_response(std::move(response));

  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(BorealisStartupResult::kSuccess, _));

  StartBorealisVm task;
  task.Run(context_.get(), callback_factory.BindOnce());
  task_environment_.RunUntilIdle();

  EXPECT_GE(FakeConciergeClient()->start_vm_call_count(), 1);
}

TEST_F(BorealisTasksTest,
       StartBorealisVmVmAlreadyRunningAndCallbackRanWithResults) {
  vm_tools::concierge::StartVmResponse response;
  response.set_status(vm_tools::concierge::VM_STATUS_RUNNING);
  FakeConciergeClient()->set_start_vm_response(std::move(response));

  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(BorealisStartupResult::kSuccess, _));

  StartBorealisVm task;
  task.Run(context_.get(), callback_factory.BindOnce());
  task_environment_.RunUntilIdle();

  EXPECT_GE(FakeConciergeClient()->start_vm_call_count(), 1);
}

TEST_F(BorealisTasksTest,
       AwaitBorealisStartupSucceedsAndCallbackRanWithResults) {
  vm_tools::concierge::VmStartedSignal vm_signal;
  vm_signal.set_owner_id(
      ash::ProfileHelper::GetUserIdHashFromProfile(context_->profile()));
  vm_signal.set_name(context_->vm_name());
  vm_tools::cicerone::ContainerStartedSignal signal;
  signal.set_owner_id(
      ash::ProfileHelper::GetUserIdHashFromProfile(context_->profile()));
  signal.set_vm_name(context_->vm_name());
  signal.set_container_name("penguin");

  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(BorealisStartupResult::kSuccess, _));

  AwaitBorealisStartup task;
  task.Run(context_.get(), callback_factory.BindOnce());
  FakeConciergeClient()->NotifyVmStarted(vm_signal);
  FakeCiceroneClient()->NotifyContainerStarted(std::move(signal));

  task_environment_.RunUntilIdle();
}

TEST_F(BorealisTasksTest,
       AwaitBorealisStartupContainerAlreadyStartedAndCallbackRanWithResults) {
  vm_tools::concierge::ListVmsResponse list_vms_response;
  vm_tools::concierge::ExtendedVmInfo vm_info;
  vm_info.set_owner_id(
      ash::ProfileHelper::GetUserIdHashFromProfile(context_->profile()));
  vm_info.set_name(context_->vm_name());
  *list_vms_response.add_vms() = vm_info;
  list_vms_response.set_success(true);
  FakeConciergeClient()->set_list_vms_response(list_vms_response);

  vm_tools::cicerone::ListRunningContainersResponse list_containers_response;
  auto* container = list_containers_response.add_containers();
  container->set_vm_name(context_->vm_name());
  container->set_container_name("penguin");
  FakeCiceroneClient()->set_list_containers_response(list_containers_response);

  vm_tools::cicerone::GetGarconSessionInfoResponse garcon_response;
  garcon_response.set_status(
      vm_tools::cicerone::GetGarconSessionInfoResponse::SUCCEEDED);
  FakeCiceroneClient()->set_get_garcon_session_info_response(garcon_response);

  guest_os::GuestOsSessionTracker* t =
      guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile_.get());
  // The session tracker gets its list of container on construction, so wait.
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(
      t->IsRunning({guest_os::VmType::BOREALIS, "borealis", "penguin"}));

  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(BorealisStartupResult::kSuccess, _));

  AwaitBorealisStartup task;
  task.Run(context_.get(), callback_factory.BindOnce());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisTasksTest,
       AwaitBorealisStartupTimesOutAndCallbackRanWithResults) {
  CallbackFactory callback_factory;
  EXPECT_CALL(
      callback_factory,
      Call(BorealisStartupResult::kAwaitBorealisStartupFailed, StrNe("")));

  AwaitBorealisStartup task;
  task.Run(context_.get(), callback_factory.BindOnce());
  task_environment_.FastForwardBy(base::Seconds(31));
}

class BorealisTasksTestDlcWithRetry
    : public BorealisTasksTest,
      public testing::WithParamInterface<std::string> {};

TEST_P(BorealisTasksTestDlcWithRetry, DlcRetries) {
  FakeDlcserviceClient()->set_install_error(GetParam());
  base::test::TestFuture<BorealisStartupResult, std::string> result;

  MountDlc task;
  task.Run(context_.get(), result.GetCallback());
  task_environment_.RunUntilIdle();

  // We won't have the result yet because we retry.
  EXPECT_FALSE(result.IsReady());
}

INSTANTIATE_TEST_SUITE_P(BorealisTasksTestDlcRetries,
                         BorealisTasksTestDlcWithRetry,
                         testing::Values(dlcservice::kErrorInternal,
                                         dlcservice::kErrorBusy));

class BorealisTasksTestDlcWithError
    : public BorealisTasksTest,
      public testing::WithParamInterface<
          std::pair<std::string, BorealisStartupResult>> {};

TEST_P(BorealisTasksTestDlcWithError, MountDlcFailsAndCallbackRanWithResults) {
  FakeDlcserviceClient()->set_install_error(GetParam().first);
  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory, Call(GetParam().second, StrNe("")));

  MountDlc task;
  task.Run(context_.get(), callback_factory.BindOnce());
  task_environment_.RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(
    BorealisTasksTestDlcErrors,
    BorealisTasksTestDlcWithError,
    testing::Values(std::make_pair(dlcservice::kErrorNeedReboot,
                                   BorealisStartupResult::kDlcNeedRebootError),
                    std::make_pair(dlcservice::kErrorInvalidDlc,
                                   BorealisStartupResult::kDlcUnsupportedError),
                    std::make_pair(dlcservice::kErrorAllocation,
                                   BorealisStartupResult::kDlcNeedSpaceError),
                    std::make_pair(dlcservice::kErrorNoImageFound,
                                   BorealisStartupResult::kDlcNeedUpdateError),
                    std::make_pair("unknown",
                                   BorealisStartupResult::kDlcUnknownError)));

class BorealisTasksTestDiskImage
    : public BorealisTasksTest,
      public testing::WithParamInterface<vm_tools::concierge::DiskImageStatus> {
};

TEST_P(BorealisTasksTestDiskImage, CreateDiskFailsAndCallbackRanWithResults) {
  vm_tools::concierge::CreateDiskImageResponse response;
  response.set_status(GetParam());
  FakeConciergeClient()->set_create_disk_image_response(std::move(response));
  EXPECT_EQ(context_->disk_path(), base::FilePath());

  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory,
              Call(BorealisStartupResult::kDiskImageFailed, StrNe("")));

  CreateDiskImage task;
  task.Run(context_.get(), callback_factory.BindOnce());
  task_environment_.RunUntilIdle();

  EXPECT_GE(FakeConciergeClient()->create_disk_image_call_count(), 1);
  EXPECT_EQ(context_->disk_path(), base::FilePath());
}

INSTANTIATE_TEST_SUITE_P(
    BorealisTasksTestCreateDiskImageErrors,
    BorealisTasksTestDiskImage,
    testing::Values(vm_tools::concierge::DISK_STATUS_UNKNOWN,
                    vm_tools::concierge::DISK_STATUS_FAILED,
                    vm_tools::concierge::DISK_STATUS_DOES_NOT_EXIST,
                    vm_tools::concierge::DISK_STATUS_DESTROYED,
                    vm_tools::concierge::DISK_STATUS_IN_PROGRESS,
                    vm_tools::concierge::DISK_STATUS_RESIZED));

class BorealisTasksTestsStartBorealisVm
    : public BorealisTasksTest,
      public testing::WithParamInterface<vm_tools::concierge::VmStatus> {};

TEST_P(BorealisTasksTestsStartBorealisVm,
       StartBorealisVmErrorsAndCallbackRanWithResults) {
  vm_tools::concierge::StartVmResponse response;
  response.set_status(GetParam());
  FakeConciergeClient()->set_start_vm_response(std::move(response));

  CallbackFactory callback_factory;
  EXPECT_CALL(callback_factory,
              Call(BorealisStartupResult::kStartVmFailed, StrNe("")));

  StartBorealisVm task;
  task.Run(context_.get(), callback_factory.BindOnce());
  task_environment_.RunUntilIdle();

  EXPECT_GE(FakeConciergeClient()->start_vm_call_count(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    BorealisTasksTestStartBorealisVmErrors,
    BorealisTasksTestsStartBorealisVm,
    testing::Values(vm_tools::concierge::VM_STATUS_UNKNOWN,
                    vm_tools::concierge::VM_STATUS_FAILURE));
}  // namespace
}  // namespace borealis
