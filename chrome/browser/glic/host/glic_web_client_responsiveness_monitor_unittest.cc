// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_web_client_responsiveness_monitor.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class MockDelegate : public GlicWebClientResponsivenessMonitor::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void, CheckResponsive, (base::OnceClosure), (override));
};

class TestDevToolsClient : public content::DevToolsAgentHostClient {
 public:
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {}
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {}
};

class GlicWebClientResponsivenessMonitorTest : public testing::Test {
 public:
  GlicWebClientResponsivenessMonitorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    web_contents_ = web_contents_factory_.CreateWebContents(&profile_);
  }

  void TearDown() override {
    monitor_.reset();
    web_contents_ = nullptr;
  }

  void CreateMonitor(GlicWebClientResponsivenessMonitor::Delegate* delegate,
                     content::RenderFrameHost* rfh = nullptr) {
    monitor_ = std::make_unique<GlicWebClientResponsivenessMonitor>(
        delegate, rfh,
        base::BindRepeating(
            &GlicWebClientResponsivenessMonitorTest::OnStateChanged,
            base::Unretained(this)));
  }

  void OnStateChanged(mojom::WebClientState state) {
    state_changes_.push_back(state);
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<GlicWebClientResponsivenessMonitor> monitor_;
  std::vector<mojom::WebClientState> state_changes_;
};

TEST_F(GlicWebClientResponsivenessMonitorTest, FeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      ::features::kGlicClientResponsivenessCheck);
  testing::NiceMock<MockDelegate> mock_delegate;
  EXPECT_CALL(mock_delegate, CheckResponsive).Times(0);

  CreateMonitor(&mock_delegate);
  monitor_->Start();
  FastForwardBy(base::Seconds(30));

  EXPECT_TRUE(monitor_->is_responsive());
  EXPECT_TRUE(state_changes_.empty());
}

TEST_F(GlicWebClientResponsivenessMonitorTest, ResponsiveClient) {
  scoped_feature_list_.InitAndEnableFeature(
      ::features::kGlicClientResponsivenessCheck);
  testing::NiceMock<MockDelegate> mock_delegate;

  EXPECT_CALL(mock_delegate, CheckResponsive)
      .WillRepeatedly(
          [](base::OnceClosure callback) { std::move(callback).Run(); });

  CreateMonitor(&mock_delegate);
  monitor_->Start();

  // Advance time by enough to trigger multiple checks.
  FastForwardBy(base::Seconds(20));

  EXPECT_TRUE(monitor_->is_responsive());
  EXPECT_TRUE(state_changes_.empty());
}

TEST_F(GlicWebClientResponsivenessMonitorTest, UnresponsiveClient) {
  scoped_feature_list_.InitAndEnableFeature(
      ::features::kGlicClientResponsivenessCheck);
  testing::NiceMock<MockDelegate> mock_delegate;

  EXPECT_CALL(mock_delegate, CheckResponsive)
      .WillRepeatedly([](base::OnceClosure callback) {
        // Do not invoke callback.
      });

  CreateMonitor(&mock_delegate);
  monitor_->Start();

  // Fast forward by ping interval (5s) to trigger CheckResponsive.
  FastForwardBy(base::Milliseconds(5000));
  EXPECT_TRUE(monitor_->is_responsive());
  EXPECT_TRUE(state_changes_.empty());

  // Fast forward by timeout (500ms default) to trigger timeout.
  FastForwardBy(base::Milliseconds(500));
  EXPECT_FALSE(monitor_->is_responsive());
  EXPECT_THAT(state_changes_,
              testing::ElementsAre(mojom::WebClientState::kUnresponsive));
}

TEST_F(GlicWebClientResponsivenessMonitorTest,
       UnresponsiveClientTransitionsToError) {
  scoped_feature_list_.InitAndEnableFeature(
      ::features::kGlicClientResponsivenessCheck);
  testing::NiceMock<MockDelegate> mock_delegate;

  EXPECT_CALL(mock_delegate, CheckResponsive)
      .WillRepeatedly([](base::OnceClosure callback) {
        // Do not invoke callback.
      });

  CreateMonitor(&mock_delegate);
  monitor_->Start();

  FastForwardBy(base::Milliseconds(5000));
  FastForwardBy(base::Milliseconds(500));
  EXPECT_FALSE(monitor_->is_responsive());
  EXPECT_EQ(monitor_->current_state(), mojom::WebClientState::kUnresponsive);
  EXPECT_THAT(state_changes_,
              testing::ElementsAre(mojom::WebClientState::kUnresponsive));

  // Fast forward by error timeout (5000ms) to trigger error state.
  FastForwardBy(base::Milliseconds(5000));
  EXPECT_FALSE(monitor_->is_responsive());
  EXPECT_EQ(monitor_->current_state(), mojom::WebClientState::kError);
  EXPECT_THAT(state_changes_,
              testing::ElementsAre(mojom::WebClientState::kUnresponsive,
                                   mojom::WebClientState::kError));
}

TEST_F(GlicWebClientResponsivenessMonitorTest, ClientBecomesResponsiveAgain) {
  scoped_feature_list_.InitAndEnableFeature(
      ::features::kGlicClientResponsivenessCheck);
  testing::NiceMock<MockDelegate> mock_delegate;
  base::OnceClosure saved_callback;

  EXPECT_CALL(mock_delegate, CheckResponsive)
      .WillRepeatedly([&saved_callback](base::OnceClosure callback) {
        saved_callback = std::move(callback);
      });

  CreateMonitor(&mock_delegate);
  monitor_->Start();

  // Trigger ping and timeout.
  FastForwardBy(base::Milliseconds(5000));
  FastForwardBy(base::Milliseconds(500));
  EXPECT_FALSE(monitor_->is_responsive());
  EXPECT_THAT(state_changes_,
              testing::ElementsAre(mojom::WebClientState::kUnresponsive));

  // Client responds after timeout (before error timer expires).
  ASSERT_TRUE(saved_callback);
  std::move(saved_callback).Run();

  EXPECT_TRUE(monitor_->is_responsive());
  EXPECT_THAT(state_changes_,
              testing::ElementsAre(mojom::WebClientState::kUnresponsive,
                                   mojom::WebClientState::kResponsive));
}

TEST_F(GlicWebClientResponsivenessMonitorTest, StopCancelsTimers) {
  scoped_feature_list_.InitAndEnableFeature(
      ::features::kGlicClientResponsivenessCheck);
  testing::NiceMock<MockDelegate> mock_delegate;

  EXPECT_CALL(mock_delegate, CheckResponsive).Times(1);

  CreateMonitor(&mock_delegate);
  monitor_->Start();

  FastForwardBy(base::Milliseconds(5000));

  monitor_->Stop();

  // Fast forward past what would have been the timeout and additional pings.
  FastForwardBy(base::Seconds(30));

  EXPECT_TRUE(monitor_->is_responsive());
  EXPECT_TRUE(state_changes_.empty());
}

TEST_F(GlicWebClientResponsivenessMonitorTest, CustomIntervalAndTimeout) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      ::features::kGlicClientResponsivenessCheck,
      {
          {::features::kGlicClientResponsivenessCheckIntervalMs.name, "1000"},
          {::features::kGlicClientResponsivenessCheckTimeoutMs.name, "2000"},
      });
  testing::NiceMock<MockDelegate> mock_delegate;

  EXPECT_CALL(mock_delegate, CheckResponsive)
      .WillRepeatedly([](base::OnceClosure callback) {
        // Do not invoke callback.
      });

  CreateMonitor(&mock_delegate);
  monitor_->Start();

  // Before interval, no check.
  FastForwardBy(base::Milliseconds(999));
  EXPECT_TRUE(monitor_->is_responsive());

  // At interval (1000ms total), CheckResponsive is called.
  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(monitor_->is_responsive());

  // Before timeout (1000 + 1999 = 2999ms total), still responsive.
  FastForwardBy(base::Milliseconds(1999));
  EXPECT_TRUE(monitor_->is_responsive());

  // At timeout (3000ms total), becomes unresponsive.
  FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(monitor_->is_responsive());
  EXPECT_THAT(state_changes_,
              testing::ElementsAre(mojom::WebClientState::kUnresponsive));
}

TEST_F(GlicWebClientResponsivenessMonitorTest,
       ZeroOrNegativeParametersFallBackToDefaults) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      ::features::kGlicClientResponsivenessCheck,
      {
          {::features::kGlicClientResponsivenessCheckIntervalMs.name, "-10"},
          {::features::kGlicClientResponsivenessCheckTimeoutMs.name, "0"},
      });
  testing::NiceMock<MockDelegate> mock_delegate;
  EXPECT_CALL(mock_delegate, CheckResponsive)
      .WillRepeatedly([](base::OnceClosure callback) {});

  CreateMonitor(&mock_delegate);
  monitor_->Start();

  // With negative/zero parameters, it should fall back to 5000ms interval
  // and 15000ms timeout.
  FastForwardBy(base::Milliseconds(4999));
  EXPECT_TRUE(monitor_->is_responsive());

  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(monitor_->is_responsive());

  FastForwardBy(base::Milliseconds(14999));
  EXPECT_TRUE(monitor_->is_responsive());

  FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(monitor_->is_responsive());
  EXPECT_THAT(state_changes_,
              testing::ElementsAre(mojom::WebClientState::kUnresponsive));
}

TEST_F(GlicWebClientResponsivenessMonitorTest, IgnoreWhenDebuggerAttached) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      ::features::kGlicClientResponsivenessCheck,
      {{::features::kGlicClientResponsivenessCheckIgnoreWhenDebuggerAttached
            .name,
        "true"}});

  testing::NiceMock<MockDelegate> mock_delegate;
  EXPECT_CALL(mock_delegate, CheckResponsive)
      .WillRepeatedly([](base::OnceClosure callback) {
        // Do not invoke callback to simulate unresponsive client.
      });

  CreateMonitor(&mock_delegate, web_contents_->GetPrimaryMainFrame());
  monitor_->Start();

  // Attach DevTools client to the web contents.
  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(web_contents_);
  TestDevToolsClient client;
  EXPECT_TRUE(agent_host->AttachClient(&client));
  EXPECT_TRUE(content::DevToolsAgentHost::IsDebuggerAttached(web_contents_));

  // Trigger ping and timeout. Because debugger is attached, it should be
  // ignored.
  FastForwardBy(base::Milliseconds(5000));
  FastForwardBy(base::Milliseconds(15000));

  EXPECT_TRUE(monitor_->is_responsive());
  EXPECT_TRUE(state_changes_.empty());

  // Detach DevTools client.
  EXPECT_TRUE(agent_host->DetachClient(&client));
  EXPECT_FALSE(content::DevToolsAgentHost::IsDebuggerAttached(web_contents_));

  // Trigger next timeout without debugger attached.
  FastForwardBy(base::Milliseconds(500));

  EXPECT_FALSE(monitor_->is_responsive());
  EXPECT_THAT(state_changes_,
              testing::ElementsAre(mojom::WebClientState::kUnresponsive));
}

TEST_F(GlicWebClientResponsivenessMonitorTest,
       DoNotIgnoreWhenDebuggerAttachedFeatureDisabled) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      ::features::kGlicClientResponsivenessCheck,
      {{::features::kGlicClientResponsivenessCheckIgnoreWhenDebuggerAttached
            .name,
        "false"}});

  testing::NiceMock<MockDelegate> mock_delegate;
  EXPECT_CALL(mock_delegate, CheckResponsive)
      .WillRepeatedly([](base::OnceClosure callback) {});

  CreateMonitor(&mock_delegate, web_contents_->GetPrimaryMainFrame());
  monitor_->Start();

  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(web_contents_);
  TestDevToolsClient client;
  EXPECT_TRUE(agent_host->AttachClient(&client));
  EXPECT_TRUE(content::DevToolsAgentHost::IsDebuggerAttached(web_contents_));

  // Even with debugger attached, because the ignore parameter is false,
  // timeout should mark it as unresponsive.
  FastForwardBy(base::Milliseconds(5000));
  FastForwardBy(base::Milliseconds(500));

  EXPECT_FALSE(monitor_->is_responsive());
  EXPECT_THAT(state_changes_,
              testing::ElementsAre(mojom::WebClientState::kUnresponsive));

  EXPECT_TRUE(agent_host->DetachClient(&client));
}

TEST_F(GlicWebClientResponsivenessMonitorTest, GuestMainFrameDestroyed) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      ::features::kGlicClientResponsivenessCheck,
      {{::features::kGlicClientResponsivenessCheckIgnoreWhenDebuggerAttached
            .name,
        "true"}});

  testing::NiceMock<MockDelegate> mock_delegate;
  EXPECT_CALL(mock_delegate, CheckResponsive)
      .WillRepeatedly([](base::OnceClosure callback) {});

  CreateMonitor(&mock_delegate, web_contents_->GetPrimaryMainFrame());
  monitor_->Start();

  // Clear web_contents_ before DestroyWebContents to prevent DanglingPtr alert.
  content::WebContents* to_destroy = web_contents_;
  web_contents_ = nullptr;
  web_contents_factory_.DestroyWebContents(to_destroy);

  FastForwardBy(base::Milliseconds(5000));
  FastForwardBy(base::Milliseconds(500));

  EXPECT_FALSE(monitor_->is_responsive());
  EXPECT_THAT(state_changes_,
              testing::ElementsAre(mojom::WebClientState::kUnresponsive));
}

TEST_F(GlicWebClientResponsivenessMonitorTest, ResponseAfterStop) {
  scoped_feature_list_.InitAndEnableFeature(
      ::features::kGlicClientResponsivenessCheck);
  testing::NiceMock<MockDelegate> mock_delegate;
  base::OnceClosure saved_callback;

  EXPECT_CALL(mock_delegate, CheckResponsive)
      .WillRepeatedly([&saved_callback](base::OnceClosure callback) {
        saved_callback = std::move(callback);
      });

  CreateMonitor(&mock_delegate);
  monitor_->Start();

  FastForwardBy(base::Milliseconds(5000));
  FastForwardBy(base::Milliseconds(500));
  EXPECT_FALSE(monitor_->is_responsive());
  EXPECT_THAT(state_changes_,
              testing::ElementsAre(mojom::WebClientState::kUnresponsive));

  monitor_->Stop();

  ASSERT_TRUE(saved_callback);
  std::move(saved_callback).Run();

  EXPECT_TRUE(monitor_->is_responsive());
  EXPECT_THAT(state_changes_,
              testing::ElementsAre(mojom::WebClientState::kUnresponsive,
                                   mojom::WebClientState::kResponsive));
}

TEST_F(GlicWebClientResponsivenessMonitorTest, CallbackAfterMonitorDestroyed) {
  scoped_feature_list_.InitAndEnableFeature(
      ::features::kGlicClientResponsivenessCheck);
  testing::NiceMock<MockDelegate> mock_delegate;
  base::OnceClosure saved_callback;

  EXPECT_CALL(mock_delegate, CheckResponsive)
      .WillRepeatedly([&saved_callback](base::OnceClosure callback) {
        saved_callback = std::move(callback);
      });

  CreateMonitor(&mock_delegate);
  monitor_->Start();

  FastForwardBy(base::Milliseconds(5000));
  ASSERT_TRUE(saved_callback);

  monitor_.reset();
  std::move(saved_callback).Run();
  // Should not crash.
}

}  // namespace
}  // namespace glic
