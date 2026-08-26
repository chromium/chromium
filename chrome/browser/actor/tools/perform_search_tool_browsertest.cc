// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/perform_search_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/test/browser_test.h"

namespace actor {

namespace {

class ActorPerformSearchToolBrowserTest : public ActorToolsTest {
 public:
  ActorPerformSearchToolBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicActor);
  }
  ~ActorPerformSearchToolBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorPerformSearchToolBrowserTest, EmptyQueryFails) {
  std::unique_ptr<ToolRequest> action =
      MakePerformSearchRequest(*active_tab(), "");
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kArgumentsInvalid);
}

}  // namespace

}  // namespace actor
