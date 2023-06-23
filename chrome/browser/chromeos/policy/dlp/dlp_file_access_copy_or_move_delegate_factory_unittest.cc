// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_file_access_copy_or_move_delegate_factory.h"

#include "components/file_access/file_access_copy_or_move_delegate_factory.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DlpFileAccessCopyOrMoveDelegateFactoryTest : public testing::Test {
 public:
  content::BrowserTaskEnvironment browser_task_environment_;
  void init() { DlpFileAccessCopyOrMoveDelegateFactory::Initialize(); }
};

TEST_F(DlpFileAccessCopyOrMoveDelegateFactoryTest, TestInit) {
  EXPECT_EQ(nullptr, file_access::FileAccessCopyOrMoveDelegateFactory::Get());
  init();
  browser_task_environment_.RunUntilIdle();
  EXPECT_NE(nullptr, file_access::FileAccessCopyOrMoveDelegateFactory::Get());
}

TEST_F(DlpFileAccessCopyOrMoveDelegateFactoryTest, TestHook) {
  init();
  browser_task_environment_.RunUntilIdle();
  file_access::FileAccessCopyOrMoveDelegateFactory* factory =
      file_access::FileAccessCopyOrMoveDelegateFactory::Get();
  std::unique_ptr<storage::CopyOrMoveHookDelegate> hook = factory->MakeHook();
  EXPECT_NE(nullptr, hook.get());
}

}  // namespace policy
