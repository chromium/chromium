// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/remote_debugging_server.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
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
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    base::CommandLine::ForCurrentProcess()->InitFromArgv({});
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  base::test::ScopedFeatureList feature_list_;
  NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

TEST_F(RemoteDebuggingServerTest, StartsAndStopsWithPref) {
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  auto server = std::make_unique<NiceMock<MockRemoteDebuggingServer>>();

  EXPECT_CALL(*server, StartHttpServer).Times(0);
  server->StartHttpServerInApprovalMode(local_state);
  testing::Mock::VerifyAndClearExpectations(server.get());

  EXPECT_CALL(*server, StartHttpServer).Times(1);
  local_state->SetUserPref(prefs::kDevToolsRemoteDebuggingEnabled,
                           std::make_unique<base::Value>(true));
  testing::Mock::VerifyAndClearExpectations(server.get());

  EXPECT_CALL(*server, StopHttpServer).Times(1);
  local_state->SetUserPref(prefs::kDevToolsRemoteDebuggingEnabled,
                           std::make_unique<base::Value>(false));
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
  TestingPrefServiceSimple* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  local_state->SetManagedPref(prefs::kDevToolsRemoteDebuggingAllowed,
                              std::make_unique<base::Value>(false));
  auto server = RemoteDebuggingServer::GetInstance(local_state);
  EXPECT_EQ(server.error(),
            RemoteDebuggingServer::NotStartedReason::kDisabledByPolicy);
}
