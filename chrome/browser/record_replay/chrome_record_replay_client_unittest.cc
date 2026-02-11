// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/chrome_record_replay_client.h"

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "chrome/common/record_replay/record_replay_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using ::testing::Return;

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

  record_replay::RecordReplayClient& client() { return *client_; }

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

}  // namespace
