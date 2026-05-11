// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_otp_filling_tool_request.h"

#include <memory>
#include <variant>
#include <vector>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/shared_types.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

class AttemptOtpFillingToolRequestTest : public testing::Test {
 public:
  AttemptOtpFillingToolRequestTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kGlicActorAutofillOneTimePassword);
  }
  ~AttemptOtpFillingToolRequestTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AttemptOtpFillingToolRequestTest,
       BuildToolRequest_ValidProto_ReturnsRequestWithCorrectNames) {
  optimization_guide::proto::Actions actions;
  optimization_guide::proto::Action* action = actions.add_actions();
  optimization_guide::proto::AttemptOtpFillingAction* otp_action =
      action->mutable_attempt_otp_filling();
  otp_action->set_tab_id(1);
  optimization_guide::proto::ActionTarget* target =
      otp_action->add_target_fields();
  target->set_content_node_id(1);
  target->mutable_document_identifier()->set_serialized_token("doc_id");

  ASSERT_OK_AND_ASSIGN(std::vector<std::unique_ptr<ToolRequest>> requests,
                       BuildToolRequest(actions));
  ASSERT_EQ(requests.size(), 1u);

  EXPECT_EQ("AttemptOtpFilling", requests.front()->Name());
  EXPECT_EQ("AttemptOtpFilling", requests.front()->JournalEvent());
}

TEST_F(AttemptOtpFillingToolRequestTest,
       BuildToolRequest_ValidProto_CreatesRequestWithCorrectParameters) {
  // Arrange
  optimization_guide::proto::Actions actions;
  optimization_guide::proto::Action* action = actions.add_actions();
  optimization_guide::proto::AttemptOtpFillingAction* otp_action =
      action->mutable_attempt_otp_filling();
  otp_action->set_tab_id(100);
  otp_action->set_for_signin(true);
  optimization_guide::proto::ActionTarget* target1 =
      otp_action->add_target_fields();
  target1->set_content_node_id(123);
  target1->mutable_document_identifier()->set_serialized_token("id_1");
  optimization_guide::proto::ActionTarget* target2 =
      otp_action->add_target_fields();
  target2->set_content_node_id(456);
  target2->mutable_document_identifier()->set_serialized_token("id_2");

  // Act
  ASSERT_OK_AND_ASSIGN(std::vector<std::unique_ptr<ToolRequest>> requests,
                       BuildToolRequest(actions));

  // Assert
  ASSERT_EQ(requests.size(), 1u);
  ToolRequest& created_request = *requests.front();
  ASSERT_EQ("AttemptOtpFilling", created_request.Name());
  AttemptOtpFillingToolRequest& otp_request =
      static_cast<AttemptOtpFillingToolRequest&>(created_request);

  EXPECT_EQ(100, otp_request.GetTabHandle().raw_value());

  EXPECT_TRUE(otp_request.GetForSigninForTesting());

  const std::vector<PageTarget>& trigger_fields =
      otp_request.GetTriggerFieldsForTesting();
  ASSERT_EQ(trigger_fields.size(), 2u);
  ASSERT_TRUE(std::holds_alternative<DomNode>(trigger_fields[0]));
  const DomNode& dom_node1 = std::get<DomNode>(trigger_fields[0]);
  EXPECT_EQ(123, dom_node1.node_id);
  EXPECT_EQ("id_1", dom_node1.document_identifier);
  ASSERT_TRUE(std::holds_alternative<DomNode>(trigger_fields[1]));
  const DomNode& dom_node2 = std::get<DomNode>(trigger_fields[1]);
  EXPECT_EQ(456, dom_node2.node_id);
  EXPECT_EQ("id_2", dom_node2.document_identifier);
}

TEST_F(AttemptOtpFillingToolRequestTest,
       BuildToolRequest_FlagDisabled_ReturnsError) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      features::kGlicActorAutofillOneTimePassword);

  optimization_guide::proto::Actions actions;
  optimization_guide::proto::Action* action = actions.add_actions();
  optimization_guide::proto::AttemptOtpFillingAction* otp_action =
      action->mutable_attempt_otp_filling();
  otp_action->set_tab_id(1);

  optimization_guide::proto::ActionTarget* target =
      otp_action->add_target_fields();
  target->set_content_node_id(123);
  target->mutable_document_identifier()->set_serialized_token("doc_id");

  EXPECT_THAT(BuildToolRequest(actions),
              base::test::ErrorIs(testing::Pair(
                  0u, mojom::ActionResultCode::kArgumentsInvalid)));
}

TEST_F(AttemptOtpFillingToolRequestTest,
       BuildToolRequest_MissingTabId_ReturnsError) {
  optimization_guide::proto::Actions actions;
  optimization_guide::proto::Action* action = actions.add_actions();
  optimization_guide::proto::AttemptOtpFillingAction* otp_action =
      action->mutable_attempt_otp_filling();
  // Do not set tab_id

  optimization_guide::proto::ActionTarget* target =
      otp_action->add_target_fields();
  target->set_content_node_id(123);
  target->mutable_document_identifier()->set_serialized_token("doc_id");

  EXPECT_THAT(BuildToolRequest(actions),
              base::test::ErrorIs(testing::Pair(
                  0u, mojom::ActionResultCode::kArgumentsInvalid)));
}

TEST_F(AttemptOtpFillingToolRequestTest,
       BuildToolRequest_ZeroTriggerFields_ReturnsError) {
  optimization_guide::proto::Actions actions;
  optimization_guide::proto::Action* action = actions.add_actions();
  optimization_guide::proto::AttemptOtpFillingAction* otp_action =
      action->mutable_attempt_otp_filling();
  otp_action->set_tab_id(1);

  EXPECT_THAT(BuildToolRequest(actions),
              base::test::ErrorIs(testing::Pair(
                  0u, mojom::ActionResultCode::kArgumentsInvalid)));
}

TEST_F(AttemptOtpFillingToolRequestTest,
       BuildToolRequest_InvalidTarget_ReturnsError) {
  optimization_guide::proto::Actions actions;
  optimization_guide::proto::Action* action = actions.add_actions();
  optimization_guide::proto::AttemptOtpFillingAction* otp_action =
      action->mutable_attempt_otp_filling();
  otp_action->set_tab_id(1);

  optimization_guide::proto::ActionTarget* target =
      otp_action->add_target_fields();
  target->set_content_node_id(123);
  // Do not set document_identifier

  EXPECT_THAT(BuildToolRequest(actions),
              base::test::ErrorIs(testing::Pair(
                  0u, mojom::ActionResultCode::kArgumentsInvalid)));
}

}  // namespace
}  // namespace actor
