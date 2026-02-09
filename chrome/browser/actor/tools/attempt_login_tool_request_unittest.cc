// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_login_tool_request.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/shared_types.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

namespace actor {

namespace {

class AttemptLoginToolRequestTest : public testing::Test {
 public:
  AttemptLoginToolRequestTest() = default;
  ~AttemptLoginToolRequestTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_{
      password_manager::features::kActorLoginFederatedLoginSupport};
};

TEST_F(AttemptLoginToolRequestTest, ReadFromProto) {
  optimization_guide::proto::Actions actions;
  optimization_guide::proto::AttemptLoginAction* attempt_login_proto =
      actions.add_actions()->mutable_attempt_login();

  attempt_login_proto->set_tab_id(123);

  optimization_guide::proto::AttemptLoginAction_LoginTarget* target1 =
      attempt_login_proto->add_login_targets();
  optimization_guide::proto::AttemptLoginAction_LoginTarget* target2 =
      attempt_login_proto->add_login_targets();

  target1->set_login_type(
      optimization_guide::proto::
          AttemptLoginAction_LoginTarget_LoginType_PASSWORD_FORM_SUBMIT);
  target1->mutable_target()->set_content_node_id(42);
  target1->mutable_target()
      ->mutable_document_identifier()
      ->set_serialized_token("my_cool_token");

  target2->set_login_type(
      optimization_guide::proto::
          AttemptLoginAction_LoginTarget_LoginType_FEDERATED_GOOGLE_SIGNIN);
  optimization_guide::proto::Coordinate* coord =
      target2->mutable_target()->mutable_coordinate();
  coord->set_x(100);
  coord->set_y(200);

  BuildToolRequestResult requests = BuildToolRequest(actions);
  ASSERT_TRUE(requests.has_value());
  ASSERT_EQ(1u, requests.value().size());

  ToolRequest& created_request = *requests.value().front();
  ASSERT_EQ(AttemptLoginToolRequest::kName, created_request.Name());

  AttemptLoginToolRequest& attempt_login_request =
      static_cast<AttemptLoginToolRequest&>(created_request);

  EXPECT_EQ(attempt_login_request.GetTabHandle().raw_value(), 123);

  std::optional<PageTarget> password_button =
      attempt_login_request.GetPasswordButtonForTesting();
  ASSERT_TRUE(password_button.has_value());
  ASSERT_TRUE(std::holds_alternative<DomNode>(*password_button));
  EXPECT_EQ(std::get<DomNode>(*password_button),
            (DomNode{.node_id = 42, .document_identifier = "my_cool_token"}));

  std::optional<PageTarget> sign_in_with_google_button =
      attempt_login_request.GetSignInWithGoogleButtonForTesting();
  ASSERT_TRUE(sign_in_with_google_button.has_value());
  ASSERT_TRUE(std::holds_alternative<gfx::Point>(*sign_in_with_google_button));
  EXPECT_EQ(std::get<gfx::Point>(*sign_in_with_google_button),
            gfx::Point(100, 200));
}

}  // namespace
}  // namespace actor
