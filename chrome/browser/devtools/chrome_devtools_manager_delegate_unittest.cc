// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/startup/automation_infobar_delegate.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENTERPRISE_CDP_ATTRIBUTION)
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/enterprise/connectors/reporting/cdp_actor_session_tracker.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "third_party/inspector_protocol/crdtp/json.h"
#include "third_party/inspector_protocol/crdtp/span.h"
#endif

namespace {

class ChromeDevToolsManagerDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitAndDisableFeature(infobars::kCentralizedInfoBarFramework);
    profile_ = std::make_unique<TestingProfile>();
    tab_strip_ =
        std::make_unique<TabStripModel>(&tab_strip_delegate_, profile_.get());
    tab_strip_delegate_.SetBrowserWindowInterface(&window_);
    ON_CALL(window_, GetTabStripModel())
        .WillByDefault(testing::Return(tab_strip_.get()));
    ON_CALL(std::as_const(window_), GetTabStripModel())
        .WillByDefault(testing::Return(tab_strip_.get()));
    ON_CALL(window_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
    ON_CALL(std::as_const(window_), GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
    collection_observer()->OnBrowserCreated(&window_);
    ASSERT_FALSE(ChromeDevToolsManagerDelegate::GetInstance());
    delegate_ = std::make_unique<ChromeDevToolsManagerDelegate>();
  }

  void TearDown() override {
    delegate_.reset();
    tab_strip_->CloseAllTabs();
    collection_observer()->OnBrowserClosed(&window_);
    tab_strip_delegate_.SetBrowserWindowInterface(nullptr);
    tab_strip_.reset();
    profile_.reset();
  }

  infobars::ContentInfoBarManager* AddTab(
      bool with_automation_infobar = false) {
    auto contents = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    infobars::ContentInfoBarManager::CreateForWebContents(contents.get());
    auto* manager =
        infobars::ContentInfoBarManager::FromWebContents(contents.get());
    if (with_automation_infobar) {
      // The automation and remote debugging infobars have the same message,
      // so the manager rejects the latter as a duplicate.
      EXPECT_TRUE(AutomationInfoBarDelegate::Create(manager));
    }
    tab_strip_->AppendWebContents(std::move(contents), /*foreground=*/true);
    return manager;
  }

  void SetConnections(size_t count) {
    static_cast<content::DevToolsManagerDelegate*>(delegate_.get())
        ->SetActiveWebSocketConnections(count);
  }

  void ExpectReconnect(infobars::ContentInfoBarManager* manager) {
    manager->RemoveAllInfoBars(/*animate=*/false);
    SetConnections(1);
    ASSERT_EQ(1u, manager->infobars().size());
    EXPECT_EQ(
        infobars::InfoBarDelegate::DEV_TOOLS_REMOTE_DEBUGGING_INFOBAR_DELEGATE,
        manager->infobars()[0]->delegate()->GetIdentifier());
    SetConnections(0);
    EXPECT_TRUE(manager->infobars().empty());
  }

  TabStripModel* tab_strip() { return tab_strip_.get(); }

  std::unique_ptr<ChromeDevToolsManagerDelegate> delegate_;

 private:
  BrowserCollectionObserver* collection_observer() {
    return GlobalBrowserCollection::GetInstance()->GetPlatformDelegate();
  }

  content::BrowserTaskEnvironment task_environment_;
  ChromeLayoutProvider layout_provider_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  tabs::TabModel::PreventFeatureInitializationForTesting prevent_tab_features_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  TestTabStripModelDelegate tab_strip_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_;
  testing::NiceMock<MockBrowserWindowInterface> window_;
};

TEST_F(ChromeDevToolsManagerDelegateTest, DisconnectAfterInfoBarCreationFails) {
  auto* manager = AddTab(/*with_automation_infobar=*/true);
  SetConnections(1);
  // Let the global infobar delete itself after AddInfoBar() rejects it.
  base::RunLoop().RunUntilIdle();

  SetConnections(0);
  ASSERT_EQ(1u, manager->infobars().size());
  EXPECT_EQ(infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE,
            manager->infobars()[0]->delegate()->GetIdentifier());
  ExpectReconnect(manager);
}

TEST_F(ChromeDevToolsManagerDelegateTest, DisconnectAfterInfoBarFailsInNewTab) {
  auto* first_manager = AddTab();
  SetConnections(1);
  ASSERT_EQ(1u, first_manager->infobars().size());

  auto* second_manager = AddTab(/*with_automation_infobar=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(first_manager->infobars().empty());

  SetConnections(0);
  ASSERT_EQ(1u, second_manager->infobars().size());
  ExpectReconnect(second_manager);
  EXPECT_TRUE(first_manager->infobars().empty());
}

TEST_F(ChromeDevToolsManagerDelegateTest,
       DestroyDelegateAfterInfoBarCreationFails) {
  auto* manager = AddTab(/*with_automation_infobar=*/true);
  SetConnections(1);
  base::RunLoop().RunUntilIdle();

  delegate_.reset();
  ASSERT_EQ(1u, manager->infobars().size());
  EXPECT_EQ(infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE,
            manager->infobars()[0]->delegate()->GetIdentifier());
}

#if BUILDFLAG(ENTERPRISE_CDP_ATTRIBUTION)
class FakeExternalDevToolsClient : public content::DevToolsAgentHostClient {
 public:
  explicit FakeExternalDevToolsClient(
      content::DevToolsAgentHostClient::PeerMetadata metadata)
      : metadata_(std::move(metadata)) {}
  ~FakeExternalDevToolsClient() override = default;

  void DispatchProtocolMessage(content::DevToolsAgentHost*,
                               base::span<const uint8_t>) override {}
  void AgentHostClosed(content::DevToolsAgentHost*) override {}
  bool IsTrusted() override { return false; }
  std::string GetTypeForMetrics() override { return "RemoteDebugger"; }
  PeerMetadata GetPeerMetadata() override { return metadata_; }

 private:
  PeerMetadata metadata_;
};

class FakeDevToolsClientChannel
    : public content::DevToolsAgentHostClientChannel {
 public:
  FakeDevToolsClientChannel(content::DevToolsAgentHost* agent_host,
                            content::DevToolsAgentHostClient* client)
      : agent_host_(agent_host), client_(client) {}
  ~FakeDevToolsClientChannel() override = default;

  void DispatchProtocolMessageToClient(std::vector<uint8_t>) override {}
  content::DevToolsAgentHost* GetAgentHost() override { return agent_host_; }
  content::DevToolsAgentHostClient* GetClient() override { return client_; }

 private:
  raw_ptr<content::DevToolsAgentHost> agent_host_;
  raw_ptr<content::DevToolsAgentHostClient> client_;
};

TEST_F(ChromeDevToolsManagerDelegateTest,
       CdpActorSessionTrackerCreatedAndWiredWhenFeatureEnabled) {
  // Destroy the default `delegate_` created in `SetUp()` first so its singleton
  // `g_instance` is cleared before constructing the replacement instance.
  delegate_.reset();
  policy::SetDMTokenForTesting(
      policy::DMToken::CreateValidToken("fake-dm-token"));
  base::ScopedClosureRunner reset_dm_token(base::BindOnce([] {
    policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
  }));
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      enterprise_connectors::kEventReportingCdpAttribution);

  delegate_ = std::make_unique<ChromeDevToolsManagerDelegate>();
  auto* manager_delegate =
      static_cast<content::DevToolsManagerDelegate*>(delegate_.get());

  AddTab();
  content::WebContents* web_contents = tab_strip()->GetWebContentsAt(0);
  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetOrCreateForTab(web_contents);
  content::DevToolsAgentHostClient::PeerMetadata pipe_meta;
  pipe_meta.transport_type =
      content::DevToolsAgentHostClient::PeerMetadata::TransportType::kPipe;
  FakeExternalDevToolsClient client(pipe_meta);
  FakeDevToolsClientChannel channel(agent_host.get(), &client);

  manager_delegate->ClientAttached(&channel);
  EXPECT_FALSE(
      enterprise_connectors::CdpActorSessionTracker::GetEventInitiatorInfo(
          web_contents)
          .has_value());

  std::vector<uint8_t> cbor_command;
  ASSERT_TRUE(
      crdtp::json::ConvertJSONToCBOR(
          crdtp::SpanFrom(R"({"id":1,"method":"Page.reload"})"), &cbor_command)
          .ok());
  manager_delegate->HandleCommand(&channel, cbor_command, base::DoNothing());
  std::optional<chrome::cros::reporting::proto::EventInitiatorInfo> initiator =
      enterprise_connectors::CdpActorSessionTracker::GetEventInitiatorInfo(
          web_contents);
  ASSERT_TRUE(initiator.has_value());
  ASSERT_TRUE(initiator->has_external_process_info());
  ASSERT_EQ(initiator->external_process_info().external_processes_size(), 1);
  EXPECT_EQ(
      initiator->external_process_info().external_processes(0).transport_type(),
      chrome::cros::reporting::proto::ExternalProcessInfo::TRANSPORT_TYPE_PIPE);

  manager_delegate->ClientDetached(&channel);
}
#endif  // BUILDFLAG(ENTERPRISE_CDP_ATTRIBUTION)

}  // namespace
