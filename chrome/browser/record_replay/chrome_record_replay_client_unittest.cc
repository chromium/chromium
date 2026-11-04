// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/chrome_record_replay_client.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/record_replay/task_parameters_extractor_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/record_replay/core/browser/task_discovery_service.h"
#include "components/record_replay/core/browser/task_parameters_extractor.h"
#include "components/record_replay/core/common/record_replay_features.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace record_replay {
namespace {

using testing::_;
using testing::Return;
using testing::ReturnRef;

class MockTaskParametersExtractor : public TaskParametersExtractor {
 public:
  MockTaskParametersExtractor() = default;
  ~MockTaskParametersExtractor() override = default;

  MOCK_METHOD((std::map<std::string, std::string>),
              GetParameterValueSelectorsForUrl,
              (const GURL& url),
              (override));
};

class MockTaskDiscoveryService : public TaskDiscoveryService {
 public:
  MOCK_METHOD(void,
              ShouldOfferTask,
              (const GURL& url, base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(std::optional<AutomationMetadata>, GetMetadata, (), (override));
};

class ChromeRecordReplayClientTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tab_.emplace();
    ON_CALL(*tab_, GetContents()).WillByDefault(Return(web_contents()));
    client_.emplace(*tab_);
  }

  void TearDown() override {
    client_.reset();
    tab_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  tabs::MockTabInterface& tab() { return *tab_; }
  ChromeRecordReplayClient& client() { return *client_; }

  void ReinitializeClient(std::unique_ptr<TaskDiscoveryService> service) {
    client_.emplace(*tab_, std::move(service));
  }

  void ResetClient() { client_.reset(); }

 private:
  base::test::ScopedFeatureList feature_list_{
      record_replay::features::kRecordReplayBase};
  std::optional<tabs::MockTabInterface> tab_;
  std::optional<ChromeRecordReplayClient> client_;
};

// Tests that GetPrimaryMainFrameUrl() changes on navigations.
TEST_F(ChromeRecordReplayClientTest, GetPrimaryMainFrameUrl) {
  const GURL url("https://example.com/");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
  EXPECT_EQ(client().GetPrimaryMainFrameUrl(), url);

  const GURL url2("https://google.com/");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url2);
  EXPECT_EQ(client().GetPrimaryMainFrameUrl(), url2);
}

TEST_F(ChromeRecordReplayClientTest, FromTabInterface) {
  tabs::MockTabInterface unattached_tab;
  EXPECT_EQ(ChromeRecordReplayClient::From(&unattached_tab), nullptr);
  EXPECT_EQ(ChromeRecordReplayClient::From(&tab()), &client());
  EXPECT_EQ(ChromeRecordReplayClient::From(nullptr), nullptr);

  ResetClient();
  EXPECT_EQ(ChromeRecordReplayClient::From(&tab()), nullptr);
}

TEST_F(ChromeRecordReplayClientTest, BindRecordReplayDriver) {
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(), &tab());
  base::ScopedClosureRunner cleanup_tab_lookup(base::BindOnce(
      [](content::WebContents* contents) {
        contents->RemoveUserData(tabs::TabLookupFromWebContents::UserDataKey());
      },
      web_contents()));
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://example.com/"));

  mojo::AssociatedRemote<record_replay::mojom::RecordReplayDriver> remote;
  ChromeRecordReplayClient::BindRecordReplayDriver(
      main_rfh(), remote.BindNewEndpointAndPassDedicatedReceiver());
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());

  ResetClient();
  mojo::AssociatedRemote<record_replay::mojom::RecordReplayDriver>
      disconnected_remote;
  ChromeRecordReplayClient::BindRecordReplayDriver(
      main_rfh(),
      disconnected_remote.BindNewEndpointAndPassDedicatedReceiver());
  disconnected_remote.FlushForTesting();
  EXPECT_FALSE(disconnected_remote.is_connected());
}

TEST_F(ChromeRecordReplayClientTest, DidFinishNavigation_OffersTask) {
  auto mock_service = std::make_unique<MockTaskDiscoveryService>();
  EXPECT_CALL(*mock_service, ShouldOfferTask(_, _))
      .WillOnce([](const GURL& url, base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });
  ReinitializeClient(std::move(mock_service));

  MockBrowserWindowInterface bwi;
  EXPECT_CALL(tab(), GetBrowserWindowInterface()).WillRepeatedly(Return(&bwi));

  BrowserWindowFeatures features;
  EXPECT_CALL(bwi, GetFeatures()).WillRepeatedly(ReturnRef(features));

  content::MockNavigationHandle handle(web_contents());
  handle.set_is_in_primary_main_frame(true);
  handle.set_has_committed(true);
  handle.set_url(GURL("https://deephand.github.io/deephand-bahn"));

  client().DidFinishNavigation(&handle);
}

TEST_F(ChromeRecordReplayClientTest, DOMContentLoaded_ExtractsParameters) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  auto* mock_extractor = static_cast<MockTaskParametersExtractor*>(
      record_replay::TaskParametersExtractorFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              profile,
              base::BindRepeating([](content::BrowserContext* context)
                                      -> std::unique_ptr<KeyedService> {
                return std::make_unique<MockTaskParametersExtractor>();
              })));
  ASSERT_TRUE(mock_extractor);

  const GURL url("https://example.com/");

  // Set the expectation for 2 calls (1 from DidFinishNavigation, 1 from
  // DOMContentLoaded)
  EXPECT_CALL(*mock_extractor, GetParameterValueSelectorsForUrl(url))
      .Times(2)
      .WillRepeatedly(Return(std::map<std::string, std::string>{}));

  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
}

}  // namespace
}  // namespace record_replay
