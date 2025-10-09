// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/media_control_tool.h"

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/media_control_tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace actor {

namespace {

class ActorMediaControlToolBrowserTest
    : public ActorToolsGeneralPageStabilityTest {
 public:
  ActorMediaControlToolBrowserTest() = default;
  ~ActorMediaControlToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ActorMediaControlToolBrowserTest,
    testing::ValuesIn(kActorGeneralPageStabilityModeValues),
    ActorToolsGeneralPageStabilityTest::DescribeParam);

// A placeholder test to ensure the test fixture is set up correctly.
IN_PROC_BROWSER_TEST_P(ActorMediaControlToolBrowserTest, PlaceholderTest) {
  const GURL url = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
}

}  // namespace
}  // namespace actor
