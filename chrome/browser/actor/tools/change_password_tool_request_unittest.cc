// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/change_password_tool_request.h"

#include <memory>
#include <variant>
#include <vector>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/tool_request_variant.h"
#include "chrome/common/actor.mojom.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

class ChangePasswordToolRequestTest : public testing::Test {
 public:
  ChangePasswordToolRequestTest() {
    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kChangePasswordTool);
  }
  ~ChangePasswordToolRequestTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChangePasswordToolRequestTest,
       BuildToolRequest_ValidProto_ReturnsRequestWithCorrectNameAndTabHandle) {
  optimization_guide::proto::Actions actions;
  optimization_guide::proto::ChangePasswordAction* change_password_action =
      actions.add_actions()->mutable_change_password();
  change_password_action->set_tab_id(100);

  ASSERT_OK_AND_ASSIGN(std::vector<std::unique_ptr<ToolRequest>> requests,
                       BuildToolRequest(actions));
  ASSERT_EQ(requests.size(), 1u);

  ToolRequest& created_request = *requests.front();
  EXPECT_EQ("ChangePassword", created_request.Name());
  EXPECT_EQ("ChangePassword", created_request.JournalEvent());

  const ChangePasswordToolRequest& change_password_request =
      static_cast<const ChangePasswordToolRequest&>(created_request);
  EXPECT_EQ(100, change_password_request.GetTabHandle().raw_value());
}

TEST_F(ChangePasswordToolRequestTest,
       BuildToolRequest_MissingTabId_ReturnsError) {
  optimization_guide::proto::Actions actions;
  actions.add_actions()->mutable_change_password();

  EXPECT_THAT(BuildToolRequest(actions),
              base::test::ErrorIs(testing::Pair(
                  0u, mojom::ActionResultCode::kArgumentsInvalid)));
}

}  // namespace
}  // namespace actor
