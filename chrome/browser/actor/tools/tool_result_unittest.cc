// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_result.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

struct TestTabResult : public ToolResultData {
  int tab_id = 0;
  std::string title;

  base::DictValue ToDict() const override {
    return base::DictValue().Set("tab_id", tab_id).Set("title", title);
  }

  std::unique_ptr<ToolResultData> Clone() const override {
    return std::make_unique<TestTabResult>(*this);
  }
};

TEST(ToolResultTest, EmptySuccess) {
  ToolResult result = ToolResult::Success();
  EXPECT_TRUE(result.Ok());
  EXPECT_FALSE(result.error().has_value());
  ASSERT_TRUE(result.action_result());
  EXPECT_EQ(result.action_result()->code, mojom::ActionResultCode::kOk);
  EXPECT_EQ(result.data(), nullptr);
}

TEST(ToolResultTest, SuccessWithActionResult) {
  ToolResult result = ToolResult::Success(MakeOkResultWithMessage(false, "ok"));
  EXPECT_TRUE(result.Ok());
  EXPECT_FALSE(result.error().has_value());
  ASSERT_TRUE(result.action_result());
  EXPECT_EQ(result.action_result()->code, mojom::ActionResultCode::kOk);
  EXPECT_FALSE(result.action_result()->requires_page_stabilization);
  EXPECT_EQ(result.action_result()->message, "ok");
  EXPECT_EQ(result.data(), nullptr);
}

TEST(ToolResultTest, TypedSuccess) {
  TestTabResult payload;
  payload.tab_id = 42;
  payload.title = "Example Tab";

  ToolResult result = ToolResult::Success(std::move(payload));
  EXPECT_TRUE(result.Ok());
  EXPECT_FALSE(result.error().has_value());
  ASSERT_TRUE(result.action_result());
  EXPECT_EQ(result.action_result()->code, mojom::ActionResultCode::kOk);
  ASSERT_NE(result.data(), nullptr);

  const TestTabResult* typed = result.data()->As<TestTabResult>();
  ASSERT_NE(typed, nullptr);
  EXPECT_EQ(typed->tab_id, 42);
  EXPECT_EQ(typed->title, "Example Tab");

  EXPECT_THAT(result.data()->ToDict(), base::test::IsJson(R"({
                "tab_id": 42,
                "title": "Example Tab"
              })"));
}

TEST(ToolResultTest, ClonePreservesTypedDataAndError) {
  TestTabResult payload;
  payload.tab_id = 42;
  payload.title = "Example Tab";

  ToolResult result = ToolResult::Success(std::move(payload));
  result.set_action_result(MakeOkResultWithMessage(false, "ok"));

  ToolResult cloned = result.Clone();
  EXPECT_TRUE(cloned.Ok());
  ASSERT_TRUE(cloned.action_result());
  EXPECT_EQ(cloned.action_result()->message, "ok");
  ASSERT_NE(cloned.data(), nullptr);
  const TestTabResult* typed = cloned.data()->As<TestTabResult>();
  ASSERT_NE(typed, nullptr);
  EXPECT_NE(typed, result.data()->As<TestTabResult>());
  EXPECT_EQ(typed->tab_id, 42);
  EXPECT_EQ(typed->title, "Example Tab");

  ToolResult error_result = ToolResult::Error(
      ToolError(mojom::ActionResultCode::kTabWentAway, "No matching tab"));
  ToolResult cloned_error = error_result.Clone();
  EXPECT_FALSE(cloned_error.Ok());
  ASSERT_TRUE(cloned_error.error().has_value());
  EXPECT_EQ(cloned_error.error()->code, mojom::ActionResultCode::kTabWentAway);
  EXPECT_EQ(cloned_error.error()->message, "No matching tab");
  ASSERT_TRUE(cloned_error.action_result());
  EXPECT_EQ(cloned_error.action_result()->code,
            mojom::ActionResultCode::kTabWentAway);
  EXPECT_EQ(cloned_error.action_result()->message, "No matching tab");
}

TEST(ToolResultTest, ErrorWithMessage) {
  ToolResult result = ToolResult::Error(
      ToolError(mojom::ActionResultCode::kTabWentAway, "No matching tab"));
  EXPECT_FALSE(result.Ok());
  EXPECT_EQ(result.data(), nullptr);

  ASSERT_TRUE(result.error().has_value());
  EXPECT_EQ(result.error()->code, mojom::ActionResultCode::kTabWentAway);
  EXPECT_EQ(result.error()->message, "No matching tab");
  ASSERT_TRUE(result.action_result());
  EXPECT_EQ(result.action_result()->code,
            mojom::ActionResultCode::kTabWentAway);
  EXPECT_EQ(result.action_result()->message, "No matching tab");
}

TEST(ToolResultTest, ErrorWithoutMessage) {
  ToolResult result =
      ToolResult::Error(ToolError(mojom::ActionResultCode::kUrlBlocked));
  EXPECT_FALSE(result.Ok());
  EXPECT_EQ(result.data(), nullptr);
  ASSERT_TRUE(result.error().has_value());
  EXPECT_EQ(result.error()->code, mojom::ActionResultCode::kUrlBlocked);
  EXPECT_EQ(result.error()->message, std::nullopt);
  ASSERT_TRUE(result.action_result());
  EXPECT_EQ(result.action_result()->code, mojom::ActionResultCode::kUrlBlocked);
  EXPECT_EQ(result.action_result()->message, "");
}

}  // namespace
}  // namespace actor
