// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mcp_server/mcp_server.h"

#include "base/test/task_environment.h"
#include "chrome/browser/mcp_server/mcp_server_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mcp_server {

class MCPServerTest : public testing::Test {
 public:
  MCPServerTest() = default;
  ~MCPServerTest() override = default;

  void SetUp() override {
    server_ = MCPServer::GetInstance();

    // Register MCP Server preferences
    pref_service_.registry()->RegisterBooleanPref(mcp_server::kMcpServerEnabled,
                                                   false);
    pref_service_.registry()->RegisterIntegerPref(mcp_server::kMcpServerPort, 9224);

    // Set the pref service on the server
    server_->SetPrefService(&pref_service_);

    // Ensure server is stopped before each test
    if (server_->IsRunning()) {
      server_->Stop();
    }
  }

  void TearDown() override {
    // Clean up: stop server after each test
    if (server_->IsRunning()) {
      server_->Stop();
    }
  }

 protected:
  MCPServer* server_;
  TestingPrefServiceSimple pref_service_;
  base::test::TaskEnvironment task_environment_;
};

// Test: MCPServer singleton returns same instance
TEST_F(MCPServerTest, GetInstanceReturnsSingleton) {
  MCPServer* instance1 = MCPServer::GetInstance();
  MCPServer* instance2 = MCPServer::GetInstance();
  EXPECT_EQ(instance1, instance2);
  EXPECT_NE(instance1, nullptr);
}

// Test: Initial state - server should not be running
TEST_F(MCPServerTest, InitialStateNotRunning) {
  EXPECT_FALSE(server_->IsRunning());
  EXPECT_EQ(server_->GetPort(), 0);
}

// Test: Start server successfully
TEST_F(MCPServerTest, StartServerSuccessfully) {
  bool result = server_->Start(9224);
  EXPECT_TRUE(result);
  EXPECT_TRUE(server_->IsRunning());
  EXPECT_EQ(server_->GetPort(), 9224);
}

// Test: Start server on custom port
TEST_F(MCPServerTest, StartServerOnCustomPort) {
  bool result = server_->Start(8888);
  EXPECT_TRUE(result);
  EXPECT_TRUE(server_->IsRunning());
  EXPECT_EQ(server_->GetPort(), 8888);
}

// Test: Cannot start server twice
TEST_F(MCPServerTest, CannotStartServerTwice) {
  bool first_start = server_->Start(9224);
  EXPECT_TRUE(first_start);
  EXPECT_TRUE(server_->IsRunning());

  // Attempt to start again should fail
  bool second_start = server_->Start(9225);
  EXPECT_FALSE(second_start);
  EXPECT_TRUE(server_->IsRunning());
  EXPECT_EQ(server_->GetPort(), 9224);  // Port should remain unchanged
}

// Test: Stop server successfully
TEST_F(MCPServerTest, StopServerSuccessfully) {
  server_->Start(9224);
  EXPECT_TRUE(server_->IsRunning());

  server_->Stop();
  EXPECT_FALSE(server_->IsRunning());
  EXPECT_EQ(server_->GetPort(), 0);
}

// Test: Stop server when not running (should be safe no-op)
TEST_F(MCPServerTest, StopServerWhenNotRunning) {
  EXPECT_FALSE(server_->IsRunning());
  server_->Stop();  // Should not crash
  EXPECT_FALSE(server_->IsRunning());
}

// Test: Start, stop, and restart server
TEST_F(MCPServerTest, RestartServer) {
  // First start
  server_->Start(9224);
  EXPECT_TRUE(server_->IsRunning());
  EXPECT_EQ(server_->GetPort(), 9224);

  // Stop
  server_->Stop();
  EXPECT_FALSE(server_->IsRunning());
  EXPECT_EQ(server_->GetPort(), 0);

  // Restart on different port
  server_->Start(8080);
  EXPECT_TRUE(server_->IsRunning());
  EXPECT_EQ(server_->GetPort(), 8080);
}

// Test: Default port is 9224
TEST_F(MCPServerTest, DefaultPortIs9224) {
  bool result = server_->Start();
  EXPECT_TRUE(result);
  EXPECT_EQ(server_->GetPort(), 9224);
}

// Test: Port validation - reject ports below 1024
TEST_F(MCPServerTest, RejectLowPortNumber) {
  bool result = server_->Start(80);
  EXPECT_FALSE(result);
  EXPECT_FALSE(server_->IsRunning());
}

// Test: Port validation - reject ports above 65535
TEST_F(MCPServerTest, RejectHighPortNumber) {
  bool result = server_->Start(70000);
  EXPECT_FALSE(result);
  EXPECT_FALSE(server_->IsRunning());
}

// Preference Tests

// Test: Initial preference state
TEST_F(MCPServerTest, PrefsInitialState) {
  EXPECT_FALSE(server_->IsEnabledInPrefs());
  EXPECT_EQ(pref_service_.GetInteger(mcp_server::kMcpServerPort), 9224);
}

// Test: Set enabled in preferences
TEST_F(MCPServerTest, SetEnabledInPrefs) {
  server_->SetEnabledInPrefs(true);
  EXPECT_TRUE(server_->IsEnabledInPrefs());
  EXPECT_TRUE(pref_service_.GetBoolean(mcp_server::kMcpServerEnabled));
}

// Test: Set disabled in preferences
TEST_F(MCPServerTest, SetDisabledInPrefs) {
  server_->SetEnabledInPrefs(true);
  EXPECT_TRUE(server_->IsEnabledInPrefs());

  server_->SetEnabledInPrefs(false);
  EXPECT_FALSE(server_->IsEnabledInPrefs());
  EXPECT_FALSE(pref_service_.GetBoolean(mcp_server::kMcpServerEnabled));
}

// Test: Starting server saves state to preferences
TEST_F(MCPServerTest, StartServerSavesStateToPrefs) {
  server_->Start(8080);
  EXPECT_TRUE(pref_service_.GetBoolean(mcp_server::kMcpServerEnabled));
  EXPECT_EQ(pref_service_.GetInteger(mcp_server::kMcpServerPort), 8080);
}

// Test: Stopping server updates preferences
TEST_F(MCPServerTest, StopServerUpdatesPrefs) {
  server_->Start(8080);
  EXPECT_TRUE(pref_service_.GetBoolean(mcp_server::kMcpServerEnabled));

  server_->Stop();
  EXPECT_FALSE(pref_service_.GetBoolean(mcp_server::kMcpServerEnabled));
}

// Test: Start with port 0 uses port from preferences
TEST_F(MCPServerTest, StartWithPortZeroUsesPrefs) {
  // Set custom port in preferences
  pref_service_.SetInteger(mcp_server::kMcpServerPort, 7777);

  // Start with port 0 should use preference
  bool result = server_->Start(0);
  EXPECT_TRUE(result);
  EXPECT_EQ(server_->GetPort(), 7777);
}

// Test: Start with explicit port overrides preferences
TEST_F(MCPServerTest, StartWithExplicitPortOverridesPrefs) {
  // Set custom port in preferences
  pref_service_.SetInteger(mcp_server::kMcpServerPort, 7777);

  // Start with explicit port should override preference
  bool result = server_->Start(8888);
  EXPECT_TRUE(result);
  EXPECT_EQ(server_->GetPort(), 8888);

  // Preference should be updated to match
  EXPECT_EQ(pref_service_.GetInteger(mcp_server::kMcpServerPort), 8888);
}

// Test: Save state manually
TEST_F(MCPServerTest, ManualSaveStateToPrefs) {
  server_->Start(9999);
  EXPECT_EQ(pref_service_.GetInteger(mcp_server::kMcpServerPort), 9999);
  EXPECT_TRUE(pref_service_.GetBoolean(mcp_server::kMcpServerEnabled));

  // Manual save should persist current state
  server_->SaveStateToPrefs();
  EXPECT_TRUE(pref_service_.GetBoolean(mcp_server::kMcpServerEnabled));
  EXPECT_EQ(pref_service_.GetInteger(mcp_server::kMcpServerPort), 9999);
}

// Test: Preferences persist across start/stop cycles
TEST_F(MCPServerTest, PreferencesPersistAcrossCycles) {
  // First cycle
  server_->Start(5555);
  EXPECT_EQ(pref_service_.GetInteger(mcp_server::kMcpServerPort), 5555);

  server_->Stop();
  EXPECT_FALSE(pref_service_.GetBoolean(mcp_server::kMcpServerEnabled));

  // Port preference should still be saved
  EXPECT_EQ(pref_service_.GetInteger(mcp_server::kMcpServerPort), 5555);

  // Second cycle with port 0 should restore previous port
  server_->Start(0);
  EXPECT_EQ(server_->GetPort(), 5555);
}

}  // namespace mcp_server
