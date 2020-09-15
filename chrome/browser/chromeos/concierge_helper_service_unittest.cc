// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/concierge_helper_service.h"

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "content/public/test/browser_task_environment.h"
#include "dbus/bus.h"
#include "dbus/object_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class TestDebugDaemonClient : public FakeDebugDaemonClient {
 public:
  TestDebugDaemonClient() = default;
  ~TestDebugDaemonClient() override = default;

  void StartConcierge(ConciergeCallback callback) override {
    ++start_concierge_called_;
    FakeDebugDaemonClient::StartConcierge(std::move(callback));
  }

  size_t start_concierge_called() const { return start_concierge_called_; }

 private:
  size_t start_concierge_called_{0};

  DISALLOW_COPY_AND_ASSIGN(TestDebugDaemonClient);
};

class TestConciergeClient : public FakeConciergeClient {
 public:
  TestConciergeClient() = default;
  ~TestConciergeClient() override = default;

  void SetVmCpuRestriction(
      const vm_tools::concierge::SetVmCpuRestrictionRequest& request,
      DBusMethodCallback<vm_tools::concierge::SetVmCpuRestrictionResponse>
          callback) override {
    requests_.push_back(request);
    FakeConciergeClient::SetVmCpuRestriction(request, std::move(callback));
  }

  // Runs callback when service becomes available. If service is already
  // available, runs callback immediately.
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    if (!service_is_available_) {
      callbacks_.push_back(std::move(callback));
    } else {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), service_is_available_));
    }
  }

  // Sets whether service is available or not. If service_is_available is
  // true, all callbacks waiting for service to be available will be run.
  void SetServiceIsAvailable(bool service_is_available) {
    service_is_available_ = service_is_available;
    if (service_is_available_ && !callbacks_.empty()) {
      for (auto& callback : callbacks_)
        WaitForServiceToBeAvailable(std::move(callback));
    }
  }
  const std::vector<vm_tools::concierge::SetVmCpuRestrictionRequest>& requests()
      const {
    return requests_;
  }

 private:
  std::vector<vm_tools::concierge::SetVmCpuRestrictionRequest> requests_;
  std::vector<dbus::ObjectProxy::WaitForServiceToBeAvailableCallback>
      callbacks_;
  bool service_is_available_ = true;

  DISALLOW_COPY_AND_ASSIGN(TestConciergeClient);
};

class ConciergeHelperServiceTest : public testing::Test {
 public:
  ConciergeHelperServiceTest() = default;
  ~ConciergeHelperServiceTest() override = default;

  // testing::Test:
  void SetUp() override {
    auto setter = DBusThreadManager::GetSetterForTesting();
    setter->SetDebugDaemonClient(std::make_unique<TestDebugDaemonClient>());
    setter->SetConciergeClient(std::make_unique<TestConciergeClient>());
    service_ = ConciergeHelperService::GetForBrowserContext(&profile_);
  }

  void TearDown() override { DBusThreadManager::Shutdown(); }

 protected:
  ConciergeHelperService* service() { return service_; }

  TestConciergeClient* fake_concierge_client() {
    return static_cast<TestConciergeClient*>(
        DBusThreadManager::Get()->GetConciergeClient());
  }

  TestDebugDaemonClient* fake_debug_client() {
    return static_cast<TestDebugDaemonClient*>(
        DBusThreadManager::Get()->GetDebugDaemonClient());
  }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  ConciergeHelperService* service_;

  DISALLOW_COPY_AND_ASSIGN(ConciergeHelperServiceTest);
};

// Tests that ConciergeHelperService can be constructed and destructed. Also,
// check that ConciergeHelperService starts Concierge on construction.
TEST_F(ConciergeHelperServiceTest, TestConstructDestruct) {
  EXPECT_EQ(1U, fake_debug_client()->start_concierge_called());
}

// Tests that ConciergeHelperService makes cpu restriction requests correctly.
TEST_F(ConciergeHelperServiceTest, TestSetVmCpuRestriction) {
  vm_tools::concierge::SetVmCpuRestrictionResponse response;
  response.set_success(true);
  fake_concierge_client()->set_set_vm_cpu_restriction_response(response);

  service()->SetArcVmCpuRestriction(true);
  service()->SetArcVmCpuRestriction(false);
  service()->SetTerminaVmCpuRestriction(true);
  service()->SetTerminaVmCpuRestriction(false);
  service()->SetPluginVmCpuRestriction(true);
  service()->SetPluginVmCpuRestriction(false);

  task_environment()->RunUntilIdle();

  std::vector<vm_tools::concierge::SetVmCpuRestrictionRequest> requests(
      fake_concierge_client()->requests());
  EXPECT_EQ(6U, requests.size());

  EXPECT_EQ(vm_tools::concierge::CPU_CGROUP_ARCVM, requests[0].cpu_cgroup());
  EXPECT_EQ(vm_tools::concierge::CPU_RESTRICTION_BACKGROUND,
            requests[0].cpu_restriction_state());
  EXPECT_EQ(vm_tools::concierge::CPU_CGROUP_ARCVM, requests[1].cpu_cgroup());
  EXPECT_EQ(vm_tools::concierge::CPU_RESTRICTION_FOREGROUND,
            requests[1].cpu_restriction_state());

  EXPECT_EQ(vm_tools::concierge::CPU_CGROUP_TERMINA, requests[2].cpu_cgroup());
  EXPECT_EQ(vm_tools::concierge::CPU_RESTRICTION_BACKGROUND,
            requests[2].cpu_restriction_state());
  EXPECT_EQ(vm_tools::concierge::CPU_CGROUP_TERMINA, requests[3].cpu_cgroup());
  EXPECT_EQ(vm_tools::concierge::CPU_RESTRICTION_FOREGROUND,
            requests[3].cpu_restriction_state());

  EXPECT_EQ(vm_tools::concierge::CPU_CGROUP_PLUGINVM, requests[4].cpu_cgroup());
  EXPECT_EQ(vm_tools::concierge::CPU_RESTRICTION_BACKGROUND,
            requests[4].cpu_restriction_state());
  EXPECT_EQ(vm_tools::concierge::CPU_CGROUP_PLUGINVM, requests[5].cpu_cgroup());
  EXPECT_EQ(vm_tools::concierge::CPU_RESTRICTION_FOREGROUND,
            requests[5].cpu_restriction_state());
}

// Tests that ConciergeHelperService requests cpu restriction immediately when
// dbus service is available, but waits until service is ready when it is not
// available.
TEST_F(ConciergeHelperServiceTest, TestWaitForServiceAvailable) {
  // Service is available, so request is made immediately.
  vm_tools::concierge::SetVmCpuRestrictionResponse response;
  response.set_success(true);
  fake_concierge_client()->set_set_vm_cpu_restriction_response(response);
  fake_concierge_client()->SetServiceIsAvailable(true);
  service()->SetArcVmCpuRestriction(true);

  task_environment()->RunUntilIdle();
  EXPECT_EQ(1U, fake_concierge_client()->requests().size());

  // Service is unavailable, so request is queued until it is.
  fake_concierge_client()->SetServiceIsAvailable(false);
  service()->SetArcVmCpuRestriction(true);

  task_environment()->RunUntilIdle();
  EXPECT_EQ(1U, fake_concierge_client()->requests().size());

  fake_concierge_client()->SetServiceIsAvailable(true);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(2U, fake_concierge_client()->requests().size());

  EXPECT_EQ(vm_tools::concierge::CPU_CGROUP_ARCVM,
            fake_concierge_client()->requests()[0].cpu_cgroup());
  EXPECT_EQ(vm_tools::concierge::CPU_CGROUP_ARCVM,
            fake_concierge_client()->requests()[1].cpu_cgroup());
  EXPECT_EQ(vm_tools::concierge::CPU_RESTRICTION_BACKGROUND,
            fake_concierge_client()->requests()[0].cpu_restriction_state());
  EXPECT_EQ(vm_tools::concierge::CPU_RESTRICTION_BACKGROUND,
            fake_concierge_client()->requests()[1].cpu_restriction_state());
}

}  // namespace chromeos
