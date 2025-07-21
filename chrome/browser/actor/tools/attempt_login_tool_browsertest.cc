// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using base::test::TestFuture;

namespace actor {

namespace {

// TODO(crbug.com/427817201): This tool is currently a no-op. Update this test
// to do something meaningful once the tool is actually implemented.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, AttemptLoginTool) {
  const GURL url = embedded_https_test_server().GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  EXPECT_TRUE(result.Wait());
}

}  // namespace
}  // namespace actor
