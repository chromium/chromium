// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/model/projector_session_impl.h"

#include "base/dcheck_is_on.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class ProjectorSessionImplTest : public testing::Test {
 public:
  ProjectorSessionImplTest() = default;

  ProjectorSessionImplTest(const ProjectorSessionImplTest&) = delete;
  ProjectorSessionImplTest& operator=(const ProjectorSessionImplTest&) = delete;

  // Testing::Test:
  void SetUp() override { session_ = std::make_unique<ProjectorSessionImpl>(); }

 protected:
  std::unique_ptr<ProjectorSessionImpl> session_;
};

TEST_F(ProjectorSessionImplTest, Start) {
  session_->Start();
  EXPECT_TRUE(session_->is_active());
  ASSERT_EQ(SourceType::kUnset, session_->preset_source_type());

  session_->Stop();
  EXPECT_FALSE(session_->is_active());
  ASSERT_EQ(SourceType::kUnset, session_->preset_source_type());
}

TEST_F(ProjectorSessionImplTest, StartWithPresetSourceType) {
  session_->Start(SourceType::kWindow);
  EXPECT_TRUE(session_->is_active());
  ASSERT_EQ(SourceType::kWindow, session_->preset_source_type());

  session_->Stop();
  EXPECT_FALSE(session_->is_active());
  ASSERT_EQ(SourceType::kUnset, session_->preset_source_type());
}

#if DCHECK_IS_ON()
TEST_F(ProjectorSessionImplTest, OnlyOneProjectorSessionAllowed) {
  session_->Start();
  EXPECT_DEATH_IF_SUPPORTED(session_->Start(), "");
}
#endif

}  // namespace ash