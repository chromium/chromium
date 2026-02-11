// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/record_replay_driver_factory.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/record_replay/record_replay_client.h"
#include "chrome/browser/record_replay/record_replay_driver.h"
#include "chrome/browser/record_replay/record_replay_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace record_replay {

namespace {

using ::testing::Eq;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::UnorderedElementsAre;

auto HasFrame(const blink::LocalFrameToken& token) {
  return testing::Property(&RecordReplayDriver::GetFrameToken, token);
}

auto HasFrame(const content::RenderFrameHost* rfh) {
  return HasFrame(rfh->GetFrameToken());
}

class MockRecordReplayClient : public RecordReplayClient {
 public:
  explicit MockRecordReplayClient(content::WebContents* web_contents) {
    driver_factory_.Observe(web_contents);
    ON_CALL(*this, GetManager()).WillByDefault(ReturnRef(manager_));
    ON_CALL(*this, GetDriverFactory())
        .WillByDefault(ReturnRef(driver_factory_));
  }
  ~MockRecordReplayClient() override = default;

  MOCK_METHOD(RecordReplayManager&, GetManager, (), (override));
  MOCK_METHOD(RecordReplayDriverFactory&, GetDriverFactory, (), (override));
  MOCK_METHOD(RecordingDataManager*, GetRecordingDataManager, (), (override));
  MOCK_METHOD(GURL, GetPrimaryMainFrameUrl, (), (override));
  MOCK_METHOD(autofill::AutofillClient*, GetAutofillClient, (), (override));
  MOCK_METHOD(void, ReportToUser, (std::string_view message), (override));

 private:
  RecordReplayManager manager_{this};
  RecordReplayDriverFactory driver_factory_{*this};
};

}  // namespace

class RecordReplayDriverFactoryTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    client_.emplace(web_contents());
    NavigateAndCommit(GURL("https://a.test/main.html"));
    web_contents_delegate_.emplace(*web_contents());
  }

  void TearDown() override {
    web_contents_delegate_.reset();
    client_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MockRecordReplayClient& client() { return *client_; }
  RecordReplayDriverFactory& factory() { return client().GetDriverFactory(); }

  content::RenderFrameHost* CreateChildFrame() {
    return content::NavigationSimulator::NavigateAndCommitFromDocument(
        GURL(base::StrCat({"https://a.test/child.html"})),
        content::RenderFrameHostTester::For(main_rfh())->AppendChild("child"));
  }

  content::RenderFrameHost* CreatePrerenderedPage() {
    return content::WebContentsTester::For(web_contents())
        ->AddPrerenderAndCommitNavigation(GURL(kPrerenderUrl));
  }

  void ActivatePrerenderedPage() {
    content::WebContentsTester::For(web_contents())
        ->ActivatePrerenderedPage(GURL(kPrerenderUrl));
  }

 private:
  static constexpr std::string_view kPrerenderUrl =
      "https://a.test/prerender.html";

  content::test::ScopedPrerenderFeatureList prerender_feature_list_;
  std::optional<MockRecordReplayClient> client_;
  std::optional<content::test::ScopedPrerenderWebContentsDelegate>
      web_contents_delegate_;
};

// Tests that a GetOrCreateDriver() creates a driver.
TEST_F(RecordReplayDriverFactoryTest, CreateAndGetDriver) {
  EXPECT_EQ(factory().GetDriver(blink::LocalFrameToken()), nullptr);
  content::RenderFrameHost* child_rfh = CreateChildFrame();
  ASSERT_TRUE(child_rfh);
  RecordReplayDriver* driver = factory().GetOrCreateDriver(child_rfh);
  EXPECT_EQ(factory().GetOrCreateDriver(child_rfh), driver);
  EXPECT_EQ(factory().GetDriver(child_rfh->GetFrameToken()), driver);
}

// Tests that a driver is created in RenderFrameCreated().
TEST_F(RecordReplayDriverFactoryTest, RenderFrameCreated) {
  content::RenderFrameHost* child_rfh = CreateChildFrame();
  ASSERT_TRUE(child_rfh);
  EXPECT_NE(factory().GetDriver(child_rfh->GetFrameToken()), nullptr);
}

TEST_F(RecordReplayDriverFactoryTest, RenderFrameDeleted) {
  content::RenderFrameHost* child_rfh = CreateChildFrame();
  ASSERT_TRUE(child_rfh);
  blink::LocalFrameToken token = child_rfh->GetFrameToken();

  EXPECT_NE(factory().GetDriver(token), nullptr);
  content::RenderFrameHostTester::For(child_rfh)->Detach();
  EXPECT_EQ(factory().GetDriver(token), nullptr);
}

// Tests that GetActiveDrivers() (only) returns the active drivers.
TEST_F(RecordReplayDriverFactoryTest, GetActiveDrivers) {
  EXPECT_THAT(factory().GetActiveDrivers(),
              UnorderedElementsAre(HasFrame(main_rfh())));

  content::RenderFrameHost* child_rfh = CreateChildFrame();
  ASSERT_TRUE(child_rfh);
  EXPECT_THAT(factory().GetActiveDrivers(),
              UnorderedElementsAre(HasFrame(main_rfh()), HasFrame(child_rfh)));

  content::RenderFrameHost* prerender_rfh = CreatePrerenderedPage();
  ASSERT_TRUE(prerender_rfh);
  EXPECT_THAT(factory().GetActiveDrivers(),
              UnorderedElementsAre(HasFrame(main_rfh()), HasFrame(child_rfh)));

  ActivatePrerenderedPage();
  EXPECT_THAT(factory().GetActiveDrivers(),
              UnorderedElementsAre(HasFrame(prerender_rfh)));
}

}  // namespace record_replay
