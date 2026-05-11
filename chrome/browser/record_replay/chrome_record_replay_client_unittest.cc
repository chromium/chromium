// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/chrome_record_replay_client.h"

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/record_replay/core/browser/task_discovery_service.h"
#include "components/record_replay/core/common/record_replay_features.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace record_replay {
namespace {

using testing::_;
using testing::Return;
using testing::ReturnRef;

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

}  // namespace
}  // namespace record_replay
