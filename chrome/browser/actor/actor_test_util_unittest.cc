// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"

#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

using ::optimization_guide::proto::Action;
using ::optimization_guide::proto::ActionsResult;
using ::optimization_guide::proto::ClickAction;
using ::testing::HasSubstr;

// {"actionResult": 11, "indexOfFailedAction": 0}
const char ActionsResultProtoBase64String[] = "CAsQAA==";
// {"click": {"target": {"contentNodeId": 123},"clickType": "LEFT"}}
const char ActionProtoBase64String[] = "CgYKAgh7EAE=";
const char NonBase64String[] = "NotBase64!!";

TEST(ActorTestUtilTest, ParseBase64Proto_ParsesValidProto) {
  {
    auto result =
        ParseBase64Proto<ActionsResult>(ActionsResultProtoBase64String);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->action_result(), 11);
    EXPECT_EQ(result->index_of_failed_action(), 0);
  }

  {
    auto result = ParseBase64Proto<Action>(ActionProtoBase64String);
    ASSERT_TRUE(result.has_value()) << result.error();
    ASSERT_TRUE(result->has_click());
    EXPECT_EQ(result->click().target().content_node_id(), 123);
    EXPECT_EQ(result->click().click_type(), ClickAction::LEFT);
  }
}

TEST(ActorTestUtilTest, ParseBase64Proto_ErrorsOnNonBase64String) {
  auto result = ParseBase64Proto<ActionsResult>(NonBase64String);
  EXPECT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), HasSubstr("Failed to Base64-decode"));
}

}  // namespace
}  // namespace actor
