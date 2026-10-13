// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/password_manager/change_password/change_password_tool.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_surface_impl.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/core/task_id.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

class ChangePasswordToolTest : public testing::Test {
 public:
  ChangePasswordToolTest() = default;
  ~ChangePasswordToolTest() override = default;

  MockToolDelegate& delegate() { return delegate_; }
  ActorSurface& actor_surface() { return actor_surface_; }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockToolDelegate delegate_;
  tabs::MockTabInterface mock_tab_;
  ActorSurfaceImpl actor_surface_{ActorSurfaceHandle::CreateForTesting(),
                                  mock_tab_.GetHandle()};
};

TEST_F(ChangePasswordToolTest, Validate_Succeeds) {
  ChangePasswordTool tool(TaskId(1), delegate(), actor_surface());

  base::test::TestFuture<mojom::ActionResultPtr> future;
  tool.Validate(future.GetCallback());
  ExpectOkResult(future);
}

TEST_F(ChangePasswordToolTest, Invoke_Succeeds) {
  ChangePasswordTool tool(TaskId(1), delegate(), actor_surface());

  base::test::TestFuture<mojom::ActionResultPtr> future;
  tool.Invoke(future.GetCallback());
  ExpectOkResult(future);
}

TEST_F(ChangePasswordToolTest, Properties) {
  ChangePasswordTool tool(TaskId(1), delegate(), actor_surface());

  EXPECT_EQ(tool.DebugString(), "ChangePasswordTool");
  EXPECT_EQ(tool.JournalEvent(), "ChangePassword");
  EXPECT_EQ(tool.GetTargetActorSurface(), actor_surface().GetHandle());
}

}  // namespace
}  // namespace actor
