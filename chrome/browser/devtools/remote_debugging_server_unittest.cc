// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/remote_debugging_server.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/devtools/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

class MockRemoteDebuggingServer : public RemoteDebuggingServer {
 public:
  MOCK_METHOD(void,
              StartHttpServer,
              (std::unique_ptr<content::DevToolsSocketFactory>,
               const base::FilePath&,
               const base::FilePath&,
               content::DevToolsAgentHost::RemoteDebuggingServerMode),
              (override));
  MOCK_METHOD(void, StopHttpServer, (), (override));
  MOCK_METHOD(void, StartPipeHandler, (), (override));
  MOCK_METHOD(void, StopPipeHandler, (), (override));
};

class RemoteDebuggingServerTest : public testing::Test {
 public:
  RemoteDebuggingServerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    feature_list_.InitAndEnableFeature(
        features::kDevToolsAcceptDebuggingConnections);
  }

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    base::CommandLine::ForCurrentProcess()->InitFromArgv({});
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(RemoteDebuggingServerTest, StartsAndStopsWithPref) {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  auto server = std::make_unique<NiceMock<MockRemoteDebuggingServer>>();

  EXPECT_CALL(*server, StartHttpServer).Times(0);
  server->StartHttpServerInApprovalMode(local_state);
  testing::Mock::VerifyAndClearExpectations(server.get());

  base::RunLoop start_run_loop;
  EXPECT_CALL(*server, StartHttpServer)
      .WillOnce(testing::WithoutArgs(
          testing::Invoke(&start_run_loop, &base::RunLoop::Quit)));
  local_state->SetUserPref(prefs::kDevToolsRemoteDebuggingEnabled,
                           std::make_unique<base::Value>(true));
  start_run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(server.get());

  base::RunLoop stop_run_loop;
  EXPECT_CALL(*server, StopHttpServer)
      .WillOnce(testing::Invoke(&stop_run_loop, &base::RunLoop::Quit));
  local_state->SetUserPref(prefs::kDevToolsRemoteDebuggingEnabled,
                           std::make_unique<base::Value>(false));
  stop_run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(server.get());
}

TEST_F(RemoteDebuggingServerTest, DoesNotStartWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kDevToolsAcceptDebuggingConnections);
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  auto server = RemoteDebuggingServer::GetInstance(local_state);
  EXPECT_EQ(server.error(),
            RemoteDebuggingServer::NotStartedReason::kNotRequested);
}

TEST_F(RemoteDebuggingServerTest, DoesNotStartWhenDisallowedByPolicy) {
  NiceMock<policy::MockConfigurationPolicyProvider> policy_provider;
  policy_provider.SetDefaultReturns(
      /*is_initialization_complete_return=*/true,
      /*is_first_policy_load_complete_return=*/true);
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&policy_provider);
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  local_state->SetManagedPref(prefs::kDevToolsRemoteDebuggingAllowed,
                              std::make_unique<base::Value>(false));
  auto server = RemoteDebuggingServer::GetInstance(local_state);
  EXPECT_EQ(server.error(),
            RemoteDebuggingServer::NotStartedReason::kDisabledByPolicy);
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(nullptr);
}

TEST_F(RemoteDebuggingServerTest, GetPortFromUserDataDir) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath active_port_file =
      temp_dir.GetPath().Append(content::kDevToolsActivePortFileName);

  // Test with a valid port file.
  ASSERT_TRUE(base::WriteFile(active_port_file, "12345"));
  EXPECT_EQ(12345,
            RemoteDebuggingServer::GetPortFromUserDataDir(temp_dir.GetPath()));

  // Test with an empty port file.
  ASSERT_TRUE(base::WriteFile(active_port_file, ""));
  EXPECT_EQ(RemoteDebuggingServer::kDefaultDevToolsPort,
            RemoteDebuggingServer::GetPortFromUserDataDir(temp_dir.GetPath()));

  // Test with a malformed port file.
  ASSERT_TRUE(base::WriteFile(active_port_file, "hello"));
  EXPECT_EQ(RemoteDebuggingServer::kDefaultDevToolsPort,
            RemoteDebuggingServer::GetPortFromUserDataDir(temp_dir.GetPath()));

  // Test with a port file that has extra content.
  ASSERT_TRUE(base::WriteFile(active_port_file, "12345\nfoo"));
  EXPECT_EQ(12345,
            RemoteDebuggingServer::GetPortFromUserDataDir(temp_dir.GetPath()));

  // Test with a negative port.
  ASSERT_TRUE(base::WriteFile(active_port_file, "-1"));
  EXPECT_EQ(RemoteDebuggingServer::kDefaultDevToolsPort,
            RemoteDebuggingServer::GetPortFromUserDataDir(temp_dir.GetPath()));

  // Test with a valid port at the upper boundary.
  ASSERT_TRUE(base::WriteFile(active_port_file, "65535"));
  EXPECT_EQ(65535,
            RemoteDebuggingServer::GetPortFromUserDataDir(temp_dir.GetPath()));

  // Test with an out of bounds port.
  ASSERT_TRUE(base::WriteFile(active_port_file, "65536"));
  EXPECT_EQ(RemoteDebuggingServer::kDefaultDevToolsPort,
            RemoteDebuggingServer::GetPortFromUserDataDir(temp_dir.GetPath()));

  // Test with no port file.
  ASSERT_TRUE(base::DeleteFile(active_port_file));
  EXPECT_EQ(RemoteDebuggingServer::kDefaultDevToolsPort,
            RemoteDebuggingServer::GetPortFromUserDataDir(temp_dir.GetPath()));
}
