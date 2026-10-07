// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/change_password_tool.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
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
  tabs::MockTabInterface& mock_tab() { return mock_tab_; }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockToolDelegate delegate_;
  tabs::MockTabInterface mock_tab_;
};

TEST_F(ChangePasswordToolTest, Validate_Succeeds) {
  ChangePasswordTool tool(TaskId(1), delegate(), mock_tab());

  base::test::TestFuture<mojom::ActionResultPtr> future;
  tool.Validate(future.GetCallback());
  ExpectOkResult(future);
}

TEST_F(ChangePasswordToolTest, Invoke_Succeeds) {
  ChangePasswordTool tool(TaskId(1), delegate(), mock_tab());

  base::test::TestFuture<mojom::ActionResultPtr> future;
  tool.Invoke(future.GetCallback());
  ExpectOkResult(future);
}

TEST_F(ChangePasswordToolTest, Properties) {
  ChangePasswordTool tool(TaskId(1), delegate(), mock_tab());

  EXPECT_EQ(tool.DebugString(), "ChangePasswordTool");
  EXPECT_EQ(tool.JournalEvent(), "ChangePassword");
  EXPECT_EQ(tool.GetTargetTab(), mock_tab().GetHandle());
}

}  // namespace
}  // namespace actor
