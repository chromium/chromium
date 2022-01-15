// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/model/projector_session_impl.h"

#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "ash/test/ash_test_base.h"
#include "base/dcheck_is_on.h"
#include "base/test/metrics/histogram_tester.h"

namespace ash {

namespace {

constexpr char kProjectorCreationFlowHistogramName[] =
    "Ash.Projector.CreationFlow.ClamshellMode";

}  // namespace

class ProjectorSessionImplTest : public AshTestBase {
 public:
  ProjectorSessionImplTest() = default;

  ProjectorSessionImplTest(const ProjectorSessionImplTest&) = delete;
  ProjectorSessionImplTest& operator=(const ProjectorSessionImplTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    session_ = std::make_unique<ProjectorSessionImpl>();
  }

 protected:
  std::unique_ptr<ProjectorSessionImpl> session_;
};

TEST_F(ProjectorSessionImplTest, Start) {
  base::HistogramTester histogram_tester;

  session_->Start("projector_data");
  histogram_tester.ExpectUniqueSample(kProjectorCreationFlowHistogramName,
                                      ProjectorCreationFlow::kSessionStarted,
                                      /*count=*/1);
  EXPECT_TRUE(session_->is_active());
  EXPECT_EQ("projector_data", session_->storage_dir());

  session_->Stop();
  EXPECT_FALSE(session_->is_active());
  histogram_tester.ExpectBucketCount(kProjectorCreationFlowHistogramName,
                                     ProjectorCreationFlow::kSessionStopped,
                                     /*count=*/1);
  histogram_tester.ExpectTotalCount(kProjectorCreationFlowHistogramName,
                                    /*count=*/2);
}

#if DCHECK_IS_ON()
TEST_F(ProjectorSessionImplTest, OnlyOneProjectorSessionAllowed) {
  session_->Start("projector_data");
  EXPECT_DEATH_IF_SUPPORTED(session_->Start("projector_data"), "");
}
#endif

}  // namespace ash
