// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/load_and_extract_content_tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {

class ActorLoadAndExtractContentToolBrowserTest : public ActorToolsTest {
 public:
  ActorLoadAndExtractContentToolBrowserTest() = default;
  ~ActorLoadAndExtractContentToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      actor::kGlicActorLoadAndExtractContentTool};
};

// Placeholder test to ensure the test fixture is set up correctly.
// TODO(b/478282478): Once the tool is implemented, add real tests, e.g.
// checking that the tabs were created and then closed, and that their content
// was extracted successfully.
IN_PROC_BROWSER_TEST_F(ActorLoadAndExtractContentToolBrowserTest, Placeholder) {
  const GURL url1 = embedded_test_server()->GetURL("/actor/blank.html?1");
  const GURL url2 = embedded_test_server()->GetURL("/actor/blank.html?2");

  std::unique_ptr<ToolRequest> request =
      std::make_unique<LoadAndExtractContentToolRequest>(
          std::vector<GURL>{url1, url2});

  EXPECT_NE(request, nullptr);
}

}  // namespace actor
