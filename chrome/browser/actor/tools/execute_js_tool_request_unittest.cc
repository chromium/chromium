// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/execute_js_tool_request.h"

#include <memory>
#include <vector>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

TEST(ExecuteJsToolRequestTest, BuildToolRequest_ValidProto_ReturnsRequest) {
  base::test::ScopedFeatureList feature_list(kActorExecuteJsTool);

  optimization_guide::proto::Actions actions;
  optimization_guide::proto::ExecuteJsAction* execute_js =
      actions.add_actions()->mutable_execute_js();
  execute_js->set_tab_id(123);
  execute_js->set_script_code("2 + 2");
  execute_js->set_timeout_ms(1000);
  execute_js->mutable_document_identifier()->set_serialized_token(
      base::UnguessableToken::Create().ToString());

  ASSERT_OK_AND_ASSIGN(std::vector<std::unique_ptr<ToolRequest>> requests,
                       BuildToolRequest(actions));
  ASSERT_EQ(1u, requests.size());
  EXPECT_EQ(requests[0]->Name(), ExecuteJsToolRequest::kName);
}

TEST(ExecuteJsToolRequestTest, BuildToolRequest_FeatureDisabled_ReturnsError) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kActorExecuteJsTool);

  optimization_guide::proto::Actions actions;
  optimization_guide::proto::ExecuteJsAction* execute_js =
      actions.add_actions()->mutable_execute_js();
  execute_js->set_tab_id(123);
  execute_js->set_script_code("2 + 2");

  auto result = BuildToolRequest(actions);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().first, 0u);
  EXPECT_EQ(result.error().second, mojom::ActionResultCode::kToolUnknown);
}

TEST(ExecuteJsToolRequestTest, BuildToolRequest_NoTab_ReturnsError) {
  base::test::ScopedFeatureList feature_list(kActorExecuteJsTool);

  optimization_guide::proto::Actions actions;
  optimization_guide::proto::ExecuteJsAction* execute_js =
      actions.add_actions()->mutable_execute_js();
  execute_js->set_script_code("2 + 2");

  auto result = BuildToolRequest(actions);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().second, mojom::ActionResultCode::kTabWentAway);
}

TEST(ExecuteJsToolRequestTest, BuildToolRequest_EmptyScript_ReturnsError) {
  base::test::ScopedFeatureList feature_list(kActorExecuteJsTool);

  optimization_guide::proto::Actions actions;
  optimization_guide::proto::ExecuteJsAction* execute_js =
      actions.add_actions()->mutable_execute_js();
  execute_js->set_tab_id(123);
  execute_js->mutable_document_identifier()->set_serialized_token(
      base::UnguessableToken::Create().ToString());

  auto result = BuildToolRequest(actions);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().second, mojom::ActionResultCode::kArgumentsInvalid);
}

TEST(ExecuteJsToolRequestTest,
     BuildToolRequest_MissingDocumentIdentifier_ReturnsError) {
  base::test::ScopedFeatureList feature_list(kActorExecuteJsTool);

  optimization_guide::proto::Actions actions;
  optimization_guide::proto::ExecuteJsAction* execute_js =
      actions.add_actions()->mutable_execute_js();
  execute_js->set_tab_id(123);
  execute_js->set_script_code("2 + 2");

  auto result = BuildToolRequest(actions);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().second, mojom::ActionResultCode::kArgumentsInvalid);
}

TEST(ExecuteJsToolRequestTest,
     BuildToolRequest_InvalidDocumentIdentifier_ReturnsError) {
  base::test::ScopedFeatureList feature_list(kActorExecuteJsTool);

  optimization_guide::proto::Actions actions;
  optimization_guide::proto::ExecuteJsAction* execute_js =
      actions.add_actions()->mutable_execute_js();
  execute_js->set_tab_id(123);
  execute_js->set_script_code("2 + 2");
  execute_js->mutable_document_identifier()->set_serialized_token(
      "not a token");

  auto result = BuildToolRequest(actions);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().second, mojom::ActionResultCode::kArgumentsInvalid);
}

}  // namespace
}  // namespace actor
