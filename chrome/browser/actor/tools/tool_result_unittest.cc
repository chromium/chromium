// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_result.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/test/values_test_util.h"
#include "base/time/time.h"
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
  ASSERT_TRUE(result.deprecated_action_result());
  EXPECT_EQ(result.deprecated_action_result()->code,
            mojom::ActionResultCode::kOk);
  EXPECT_EQ(result.data(), nullptr);
}

TEST(ToolResultTest, SuccessWithActionResult) {
  ToolResult result = ToolResult::Success(MakeOkResultWithMessage(false, "ok"));
  EXPECT_TRUE(result.Ok());
  EXPECT_FALSE(result.error().has_value());
  ASSERT_TRUE(result.deprecated_action_result());
  EXPECT_EQ(result.deprecated_action_result()->code,
            mojom::ActionResultCode::kOk);
  EXPECT_FALSE(result.deprecated_action_result()->requires_page_stabilization);
  EXPECT_EQ(result.deprecated_action_result()->message, "ok");
  EXPECT_EQ(result.data(), nullptr);
}

TEST(ToolResultTest, FromDeprecatedActionResult) {
  ToolResult ok_result = ToolResult::FromDeprecatedActionResult(
      MakeOkResultWithMessage(false, "ok"));
  EXPECT_TRUE(ok_result.Ok());
  EXPECT_FALSE(ok_result.error().has_value());
  ASSERT_TRUE(ok_result.deprecated_action_result());
  EXPECT_EQ(ok_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kOk);
  EXPECT_FALSE(
      ok_result.deprecated_action_result()->requires_page_stabilization);
  EXPECT_EQ(ok_result.deprecated_action_result()->message, "ok");

  ToolResult error_result = ToolResult::FromDeprecatedActionResult(
      MakeResult(mojom::ActionResultCode::kTabWentAway,
                 /*requires_page_stabilization=*/false, "No matching tab"));
  EXPECT_FALSE(error_result.Ok());
  ASSERT_TRUE(error_result.error().has_value());
  EXPECT_EQ(error_result.error()->code, mojom::ActionResultCode::kTabWentAway);
  EXPECT_EQ(error_result.error()->message, "No matching tab");
  ASSERT_TRUE(error_result.deprecated_action_result());
  EXPECT_EQ(error_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kTabWentAway);
  EXPECT_EQ(error_result.deprecated_action_result()->message,
            "No matching tab");
}

TEST(ToolResultTest, TypedSuccess) {
  TestTabResult payload;
  payload.tab_id = 42;
  payload.title = "Example Tab";

  ToolResult result = ToolResult::Success(std::move(payload));
  EXPECT_TRUE(result.Ok());
  EXPECT_FALSE(result.error().has_value());
  ASSERT_TRUE(result.deprecated_action_result());
  EXPECT_EQ(result.deprecated_action_result()->code,
            mojom::ActionResultCode::kOk);
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
  result.set_deprecated_action_result(MakeOkResultWithMessage(false, "ok"));

  ToolResult cloned = result.Clone();
  EXPECT_TRUE(cloned.Ok());
  ASSERT_TRUE(cloned.deprecated_action_result());
  EXPECT_EQ(cloned.deprecated_action_result()->message, "ok");
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
  ASSERT_TRUE(cloned_error.deprecated_action_result());
  EXPECT_EQ(cloned_error.deprecated_action_result()->code,
            mojom::ActionResultCode::kTabWentAway);
  EXPECT_EQ(cloned_error.deprecated_action_result()->message,
            "No matching tab");
}

TEST(ToolResultTest, ErrorWithMessage) {
  ToolResult result = ToolResult::Error(
      ToolError(mojom::ActionResultCode::kTabWentAway, "No matching tab"));
  EXPECT_FALSE(result.Ok());
  EXPECT_EQ(result.data(), nullptr);

  ASSERT_TRUE(result.error().has_value());
  EXPECT_EQ(result.error()->code, mojom::ActionResultCode::kTabWentAway);
  EXPECT_EQ(result.error()->message, "No matching tab");
  ASSERT_TRUE(result.deprecated_action_result());
  EXPECT_EQ(result.deprecated_action_result()->code,
            mojom::ActionResultCode::kTabWentAway);
  EXPECT_EQ(result.deprecated_action_result()->message, "No matching tab");
}

TEST(ToolResultTest, ErrorWithoutMessage) {
  ToolResult result =
      ToolResult::Error(ToolError(mojom::ActionResultCode::kUrlBlocked));
  EXPECT_FALSE(result.Ok());
  EXPECT_EQ(result.data(), nullptr);
  ASSERT_TRUE(result.error().has_value());
  EXPECT_EQ(result.error()->code, mojom::ActionResultCode::kUrlBlocked);
  EXPECT_EQ(result.error()->message, std::nullopt);
  ASSERT_TRUE(result.deprecated_action_result());
  EXPECT_EQ(result.deprecated_action_result()->code,
            mojom::ActionResultCode::kUrlBlocked);
  EXPECT_EQ(result.deprecated_action_result()->message, "");
}

TEST(ToolResultTest, ToolResultWithLatencyInfoAndClone) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const base::TimeTicks end_time = start_time + base::Milliseconds(25);

  TestTabResult payload;
  payload.tab_id = 7;
  payload.title = "Active Tab";

  ToolResultWithLatencyInfo info(start_time, end_time,
                                 ToolResult::Success(std::move(payload)));
  EXPECT_EQ(info.start_time, start_time);
  EXPECT_EQ(info.end_time, end_time);
  EXPECT_TRUE(info.tool_result.Ok());

  ToolResultWithLatencyInfo copied(info);
  EXPECT_EQ(copied.start_time, start_time);
  EXPECT_EQ(copied.end_time, end_time);
  EXPECT_TRUE(copied.tool_result.Ok());
  ASSERT_NE(copied.tool_result.data(), nullptr);
  const TestTabResult* typed = copied.tool_result.data()->As<TestTabResult>();
  ASSERT_NE(typed, nullptr);
  EXPECT_NE(typed, info.tool_result.data()->As<TestTabResult>());
  EXPECT_EQ(typed->tab_id, 7);
  EXPECT_EQ(typed->title, "Active Tab");
}

TEST(ToolResultTest, MakeResultVectorAndExtractErrorResult) {
  std::vector<ToolResultWithLatencyInfo> ok_results =
      MakeResultVector(ToolResult::Success());
  ASSERT_EQ(ok_results.size(), 1u);
  EXPECT_TRUE(ok_results[0].tool_result.Ok());

  mojom::ActionResultCode error_code = mojom::ActionResultCode::kOk;
  std::optional<size_t> failed_index;
  ExtractErrorResult(ok_results, error_code, failed_index);
  EXPECT_EQ(error_code, mojom::ActionResultCode::kOk);
  EXPECT_EQ(failed_index, std::nullopt);

  const base::TimeTicks now = base::TimeTicks::Now();
  ok_results.emplace_back(now, now,
                          MakeResult(mojom::ActionResultCode::kTabWentAway));
  ExtractErrorResult(ok_results, error_code, failed_index);
  EXPECT_EQ(error_code, mojom::ActionResultCode::kTabWentAway);
  EXPECT_EQ(failed_index, 1u);
}

}  // namespace
}  // namespace actor
