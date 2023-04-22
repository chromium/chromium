// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/lock_screen/arc_lock_screen_bridge.h"

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_lock_screen_instance.h"
#include "ash/components/arc/test/test_browser_context.h"
#include "base/memory/raw_ptr.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcLockScreenBridgeTest : public testing::Test {
 protected:
  ArcLockScreenBridgeTest() = default;
  ArcLockScreenBridgeTest(const ArcLockScreenBridgeTest&) = delete;
  ArcLockScreenBridgeTest& operator=(const ArcLockScreenBridgeTest&) = delete;
  ~ArcLockScreenBridgeTest() override = default;

  void SetUp() override {
    // Set the state to "logged in" before testing.
    session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);

    bridge_ = ArcLockScreenBridge::GetForBrowserContextForTesting(&context_);
    // This results in ArcLockScreenBridge::OnInstanceReady being called.
    ArcServiceManager::Get()->arc_bridge_service()->lock_screen()->SetInstance(
        &lock_screen_instance_);
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->lock_screen());
  }

  ArcLockScreenBridge* bridge() { return bridge_; }
  session_manager::SessionManager* session_manager() {
    return &session_manager_;
  }
  FakeLockScreenInstance* lock_screen_instance() {
    return &lock_screen_instance_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  session_manager::SessionManager session_manager_;
  ArcServiceManager arc_service_manager_;
  FakeLockScreenInstance lock_screen_instance_;
  TestBrowserContext context_;
  raw_ptr<ArcLockScreenBridge, ExperimentalAsh> bridge_ = nullptr;
};

TEST_F(ArcLockScreenBridgeTest, ConstructDestruct) {
  EXPECT_NE(nullptr, bridge());
}

TEST_F(ArcLockScreenBridgeTest, OnConnectionReady) {
  const absl::optional<bool>& is_locked = lock_screen_instance()->is_locked();
  // The state should have been set already. See SetUp().
  ASSERT_TRUE(is_locked);
  // And the state should be "not locked";
  EXPECT_FALSE(*is_locked);
}

TEST_F(ArcLockScreenBridgeTest, OnSessionStateChanged) {
  const absl::optional<bool>& is_locked = lock_screen_instance()->is_locked();
  // The state should have been set already. See SetUp().
  ASSERT_TRUE(is_locked);
  // Lock the screen and check the instance state.
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_TRUE(*is_locked);
  // Unlock it and do the same.
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_FALSE(*is_locked);
}

}  // namespace
}  // namespace arc
