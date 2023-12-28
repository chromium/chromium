// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/model/projector_session_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "ash/test/ash_test_base.h"
#include "base/dcheck_is_on.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

namespace {

constexpr char kProjectorCreationFlowHistogramName[] =
    "Ash.Projector.CreationFlow.ClamshellMode";

}  // namespace

class ProjectorSessionImplTest : public AshTestBase {
 public:
  ProjectorSessionImplTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ProjectorSessionImplTest(const ProjectorSessionImplTest&) = delete;
  ProjectorSessionImplTest& operator=(const ProjectorSessionImplTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kProjectorManagedUser},
        /*disabled_features=*/{});
    AshTestBase::SetUp();
    session_ = static_cast<ProjectorSessionImpl*>(ProjectorSession::Get());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<ProjectorSessionImpl, DanglingUntriaged> session_;
};

TEST_F(ProjectorSessionImplTest, Start) {
  base::HistogramTester histogram_tester;
  base::Time start_time;
  EXPECT_TRUE(base::Time::FromString("2 Jan 2021 20:02:10", &start_time));
  base::TimeDelta forward_by = start_time - base::Time::Now();
  task_environment()->AdvanceClock(forward_by);
  session_->Start(base::SafeBaseName::Create("projector_data").value());
  histogram_tester.ExpectUniqueSample(kProjectorCreationFlowHistogramName,
                                      ProjectorCreationFlow::kSessionStarted,
                                      /*count=*/1);
  EXPECT_TRUE(session_->is_active());
  EXPECT_EQ("projector_data", session_->storage_dir().path().MaybeAsASCII());
  EXPECT_EQ("Screencast 2021-01-02 20.02.10", session_->screencast_name());

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
  session_->Start(base::SafeBaseName::Create("projector_data").value());
  EXPECT_DEATH_IF_SUPPORTED(
      session_->Start(base::SafeBaseName::Create("projector_data").value()),
      "");
}
#endif

}  // namespace ash
