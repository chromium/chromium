// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/user_session/arc_user_session_service.h"

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/test/fake_intent_helper_host.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcUserSessionServiceTest : public testing::Test {
 protected:
  ArcUserSessionServiceTest() = default;
  ArcUserSessionServiceTest(const ArcUserSessionServiceTest&) = delete;
  ArcUserSessionServiceTest& operator=(const ArcUserSessionServiceTest&) =
      delete;
  ~ArcUserSessionServiceTest() override = default;

  void SetUp() override {
    intent_helper_host_ = std::make_unique<FakeIntentHelperHost>(
        ArcServiceManager::Get()->arc_bridge_service()->intent_helper());
    ArcUserSessionService::GetForBrowserContextForTesting(&profile_);
    // This results in ArcUserSessionService::OnConnectionReady being called
    // by intent_helper().
    // Note that unlike other bridges, ArcUserSessionServices piggybacks on
    // intent_helper().
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->intent_helper()
        ->SetInstance(&intent_helper_instance_);
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->intent_helper());
  }

  session_manager::SessionManager* session_manager() {
    return &session_manager_;
  }
  const FakeIntentHelperInstance* intent_helper_instance() const {
    return &intent_helper_instance_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  session_manager::SessionManager session_manager_;
  ArcServiceManager arc_service_manager_;
  std::unique_ptr<FakeIntentHelperHost> intent_helper_host_;
  FakeIntentHelperInstance intent_helper_instance_;
  TestingProfile profile_;
};

TEST_F(ArcUserSessionServiceTest, ConstructDestruct) {}

TEST_F(ArcUserSessionServiceTest, OnSessionStateChanged) {
  const auto& broadcasts = intent_helper_instance()->broadcasts();
  EXPECT_TRUE(broadcasts.empty());

  // Set the state to "logged in".
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  // Verify a broadcast is sent.
  EXPECT_EQ(1u, broadcasts.size());

  // Set the state to "locked".
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  // Verify a broadcast is not sent.
  EXPECT_EQ(1u, broadcasts.size());
}

}  // namespace
}  // namespace arc
