// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_client_adapter.h"

#include <memory>

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/command_line.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcClientAdapterTest : public testing::Test,
                             public ArcClientAdapter::Observer {
 public:
  ArcClientAdapterTest() = default;
  ArcClientAdapterTest(const ArcClientAdapterTest&) = delete;
  ArcClientAdapterTest& operator=(const ArcClientAdapterTest&) = delete;
  ~ArcClientAdapterTest() override = default;

  // ArcClientAdapter::Observer overrides:
  void ArcInstanceStopped(bool is_system_shutdown) override {}

  void SetUp() override {
    ash::DebugDaemonClient::InitializeFake();
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    ash::UpstartClient::InitializeFake();
  }
  void TearDown() override {
    ash::ConciergeClient::Shutdown();
    ash::DebugDaemonClient::Shutdown();
  }

 private:
  ArcServiceManager arc_service_manager_;
};

TEST_F(ArcClientAdapterTest, ConstructDestruct) {
  auto adapter = ArcClientAdapter::Create();
  base::ScopedObservation<ArcClientAdapter, ArcClientAdapter::Observer>
      adapter_observation(this);
  adapter_observation.Observe(adapter.get());
}

TEST_F(ArcClientAdapterTest, ConstructDestruct_WithARCVM) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv({"", "--enable-arcvm"});
  ASSERT_TRUE(IsArcVmEnabled());

  auto vm_adapter = ArcClientAdapter::Create();
  base::ScopedObservation<ArcClientAdapter, ArcClientAdapter::Observer>
      vm_adapter_observation(this);
  vm_adapter_observation.Observe(vm_adapter.get());
}

}  // namespace
}  // namespace arc
