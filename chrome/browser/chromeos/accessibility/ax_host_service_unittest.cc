// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/ax_host_service.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/chromeos/accessibility/ax_remote_host_delegate.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_tree_id_registry.h"
#include "ui/accessibility/mojom/ax_host.mojom.h"

namespace {

class TestAXRemoteHost : ax::mojom::AXRemoteHost {
 public:
  TestAXRemoteHost() : binding_(this) {}
  ~TestAXRemoteHost() override = default;

  ax::mojom::AXRemoteHostPtr CreateInterfacePtr() {
    ax::mojom::AXRemoteHostPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  // Simulates the real AXRemoteHost.
  void RegisterRemoteHostCallback(const ui::AXTreeID& tree_id, bool enabled) {
    tree_id_ = tree_id;
    OnAutomationEnabled(enabled);
  }

  // ax::mojom::AXRemoteHost:
  void OnAutomationEnabled(bool enabled) override {
    ++automation_enabled_count_;
    last_automation_enabled_ = enabled;
  }
  void PerformAction(const ui::AXActionData& action) override {
    ++perform_action_count_;
    last_action_ = action;
  }

  mojo::Binding<ax::mojom::AXRemoteHost> binding_;
  ui::AXTreeID tree_id_;
  int automation_enabled_count_ = 0;
  bool last_automation_enabled_ = false;
  int perform_action_count_ = 0;
  ui::AXActionData last_action_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAXRemoteHost);
};

class AXHostServiceTest : public testing::Test {
 public:
  AXHostServiceTest() = default;
  ~AXHostServiceTest() override = default;

  void RegisterRemoteHost(AXHostService* service, TestAXRemoteHost* remote) {
    service->RegisterRemoteHost(
        remote->CreateInterfacePtr(),
        base::BindOnce(&TestAXRemoteHost::RegisterRemoteHostCallback,
                       base::Unretained(remote)));
    service->FlushForTesting();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_enviroment_;

  DISALLOW_COPY_AND_ASSIGN(AXHostServiceTest);
};

TEST_F(AXHostServiceTest, AddClientThenEnable) {
  AXHostService service;
  TestAXRemoteHost remote;
  RegisterRemoteHost(&service, &remote);

  // Remote received initial state.
  EXPECT_EQ(1, remote.automation_enabled_count_);
  EXPECT_FALSE(remote.last_automation_enabled_);

  // AXHostService assigned a tree id.
  ui::AXTreeID tree_id = remote.tree_id_;
  EXPECT_NE(ui::AXTreeIDUnknown(), tree_id);

  AXHostService::SetAutomationEnabled(true);
  service.FlushForTesting();

  // Remote received updated state.
  EXPECT_EQ(2, remote.automation_enabled_count_);
  EXPECT_TRUE(remote.last_automation_enabled_);
}

TEST_F(AXHostServiceTest, EnableThenAddClient) {
  AXHostService service;
  AXHostService::SetAutomationEnabled(true);

  TestAXRemoteHost remote;
  RegisterRemoteHost(&service, &remote);

  // Remote received initial state.
  EXPECT_EQ(1, remote.automation_enabled_count_);
  EXPECT_TRUE(remote.last_automation_enabled_);

  // AXHostService assigned a tree id.
  EXPECT_NE(ui::AXTreeIDUnknown(), remote.tree_id_);
}

TEST_F(AXHostServiceTest, PerformAction) {
  AXHostService service;
  AXHostService::SetAutomationEnabled(true);

  TestAXRemoteHost remote;
  RegisterRemoteHost(&service, &remote);

  // AXHostDelegate was created.
  ui::AXTreeID tree_id = remote.tree_id_;
  ui::AXHostDelegate* delegate =
      ui::AXTreeIDRegistry::GetInstance()->GetHostDelegate(tree_id);
  ASSERT_TRUE(delegate);

  // Trigger an action.
  ui::AXActionData action;
  action.action = ax::mojom::Action::kScrollUp;
  delegate->PerformAction(action);
  service.FlushForTesting();

  // Remote interface received the action.
  EXPECT_EQ(1, remote.perform_action_count_);
  EXPECT_EQ(ax::mojom::Action::kScrollUp, remote.last_action_.action);
}

TEST_F(AXHostServiceTest, MultipleRemoteHosts) {
  AXHostService service;
  AXHostService::SetAutomationEnabled(true);

  // Connect 2 remote hosts.
  TestAXRemoteHost remote1;
  RegisterRemoteHost(&service, &remote1);
  TestAXRemoteHost remote2;
  RegisterRemoteHost(&service, &remote2);

  // Different tree ids were assigned.
  EXPECT_NE(ui::AXTreeIDUnknown(), remote1.tree_id_);
  EXPECT_NE(ui::AXTreeIDUnknown(), remote2.tree_id_);
  EXPECT_NE(remote1.tree_id_, remote2.tree_id_);

  // Trigger an action on the first remote.
  ui::AXActionData action;
  action.action = ax::mojom::Action::kScrollUp;
  ui::AXHostDelegate* delegate =
      ui::AXTreeIDRegistry::GetInstance()->GetHostDelegate(remote1.tree_id_);
  delegate->PerformAction(action);
  service.FlushForTesting();

  // Remote 1 received the action.
  EXPECT_EQ(1, remote1.perform_action_count_);
  EXPECT_EQ(ax::mojom::Action::kScrollUp, remote1.last_action_.action);

  // Remote 2 did not receive the action.
  EXPECT_EQ(0, remote2.perform_action_count_);
}

TEST_F(AXHostServiceTest, RemoteHostDisconnect) {
  AXHostService service;
  AXHostService::SetAutomationEnabled(true);

  // Connect 2 remote hosts.
  TestAXRemoteHost remote1;
  RegisterRemoteHost(&service, &remote1);
  TestAXRemoteHost remote2;
  RegisterRemoteHost(&service, &remote2);

  // Tree IDs exist for both.
  auto* tree_id_registry = ui::AXTreeIDRegistry::GetInstance();
  EXPECT_TRUE(tree_id_registry->GetHostDelegate(remote1.tree_id_));
  EXPECT_TRUE(tree_id_registry->GetHostDelegate(remote2.tree_id_));

  // Simulate remote 1 disconnecting.
  service.OnRemoteHostDisconnected(remote1.tree_id_);

  // Tree ID for remote 1 is gone.
  EXPECT_FALSE(tree_id_registry->GetHostDelegate(remote1.tree_id_));
  EXPECT_TRUE(tree_id_registry->GetHostDelegate(remote2.tree_id_));
}

}  // namespace
