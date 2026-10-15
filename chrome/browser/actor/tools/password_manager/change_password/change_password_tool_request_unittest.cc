// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/password_manager/change_password/change_password_tool_request.h"

#include <optional>
#include <variant>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_surface_impl.h"
#include "chrome/browser/actor/tool_request_variant.h"
#include "chrome/browser/actor/tools/password_manager/change_password/change_password_tool.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/core/shared_types.h"
#include "components/actor/core/task_id.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

namespace actor {
namespace {

TEST(ChangePasswordToolRequestTest, Properties) {
  tabs::MockTabInterface mock_tab;
  ActorSurfaceImpl actor_surface(ActorSurfaceHandle::CreateForTesting(),
                                 mock_tab.GetHandle());
  PageTarget current_password_field =
      DomNode{.node_id = 10, .document_identifier = "doc_token_1"};
  PageTarget new_password_field =
      DomNode{.node_id = 20, .document_identifier = "doc_token_1"};
  PageTarget confirm_new_password_field =
      DomNode{.node_id = 30, .document_identifier = "doc_token_1"};
  PageTarget confirm_button = gfx::Point(50, 60);

  ChangePasswordToolRequest request(actor_surface.GetHandle(),
                                    current_password_field, new_password_field,
                                    confirm_new_password_field, confirm_button);

  EXPECT_EQ(request.Name(), ChangePasswordToolRequest::kName);
  EXPECT_EQ(request.Name(), "ChangePassword");
  EXPECT_EQ(request.JournalEvent(), "ChangePassword");
  EXPECT_EQ(request.GetActorSurfaceHandle(), actor_surface.GetHandle());
  EXPECT_EQ(request.GetActorSurfaceHandle().GetTabHandle(),
            mock_tab.GetHandle());
  EXPECT_EQ(request.GetCurrentPasswordFieldForTesting(),
            current_password_field);
  EXPECT_EQ(request.GetNewPasswordFieldForTesting(), new_password_field);
  EXPECT_EQ(request.GetConfirmNewPasswordFieldForTesting(),
            confirm_new_password_field);
  EXPECT_EQ(request.GetConfirmButtonForTesting(), confirm_button);
}

TEST(ChangePasswordToolRequestTest, CreateTool_ValidTab_Succeeds) {
  MockToolDelegate delegate;
  tabs::MockTabInterface mock_tab;
  ActorSurfaceImpl actor_surface(ActorSurfaceHandle::CreateForTesting(),
                                 mock_tab.GetHandle());
  ChangePasswordToolRequest request(
      actor_surface.GetHandle(),
      /*current_password_field=*/std::nullopt,
      PageTarget(DomNode{.node_id = 20, .document_identifier = "doc_token_1"}),
      /*confirm_new_password_field=*/std::nullopt,
      /*confirm_button=*/std::nullopt);

  ToolRequest::CreateToolResult result =
      request.CreateTool(TaskId(1), delegate);
  EXPECT_NE(result.tool, nullptr);
  ASSERT_TRUE(result.result);
  EXPECT_EQ(result.result->code, mojom::ActionResultCode::kOk);
}

TEST(ChangePasswordToolRequestTest, CreateTool_NullTab_ReturnsError) {
  MockToolDelegate delegate;
  // No surface is registered under this handle.
  ChangePasswordToolRequest request(
      ActorSurfaceHandle::CreateForTesting(),
      /*current_password_field=*/std::nullopt,
      PageTarget(DomNode{.node_id = 20, .document_identifier = "doc_token_1"}),
      /*confirm_new_password_field=*/std::nullopt,
      /*confirm_button=*/std::nullopt);

  ToolRequest::CreateToolResult result =
      request.CreateTool(TaskId(1), delegate);
  EXPECT_EQ(result.tool, nullptr);
  ASSERT_TRUE(result.result);
  EXPECT_EQ(result.result->code, mojom::ActionResultCode::kTabWentAway);
}

}  // namespace
}  // namespace actor
