// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/termination_target_policy.h"

#include <memory>

#include "base/byte_count.h"
#include "chrome/browser/performance_manager/mechanisms/termination_target_setter.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "components/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

namespace {

class LenientMockTerminationTargetSetter : public TerminationTargetSetter {
 public:
  LenientMockTerminationTargetSetter() = default;
  ~LenientMockTerminationTargetSetter() override = default;
  LenientMockTerminationTargetSetter(
      const LenientMockTerminationTargetSetter& other) = delete;
  LenientMockTerminationTargetSetter& operator=(
      const LenientMockTerminationTargetSetter&) = delete;

  MOCK_METHOD1(SetTerminationTarget, void(const ProcessNode*));

 private:
};
using MockTerminationTargetSetter =
    ::testing::StrictMock<LenientMockTerminationTargetSetter>;

class TerminationTargetPolicyTest : public GraphTestHarness {
 public:
  TerminationTargetPolicyTest() = default;
  ~TerminationTargetPolicyTest() override = default;

  void SetUp() override {
    GraphTestHarness::SetUp();

    auto termination_target_setter =
        std::make_unique<MockTerminationTargetSetter>();
    termination_target_setter_ = termination_target_setter.get();
    graph()->PassToGraph(std::make_unique<TerminationTargetPolicy>(
        std::move(termination_target_setter)));

    auto eligibility_policy =
        std::make_unique<policies::DiscardEligibilityPolicy>();
    eligibility_policy->SetNoDiscardPatternsForProfile("", {});

    graph()->PassToGraph(std::move(eligibility_policy));
  }

  void TearDown() override {
    termination_target_setter_ = nullptr;
    policy_.reset();

    GraphTestHarness::TearDown();
  }

  raw_ptr<MockTerminationTargetSetter> termination_target_setter_ = nullptr;

 private:
  std::unique_ptr<TerminationTargetPolicy> policy_;
};

}  // namespace

TEST_F(TerminationTargetPolicyTest, Basic) {
  AdvanceClock(base::Milliseconds(1));

  // Old, small, non-discardable
  auto process1 = CreateRendererProcessNode();
  process1->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  auto page1 = CreateNode<PageNodeImpl>();
  auto frame1 = CreateFrameNodeAutoId(process1.get(), page1.get());
  testing::MakePageNodeDiscardable(page1.get(), task_env());
  process1->set_private_footprint(base::MiB(2));
  page1->SetIsAudible(true);

  // Old, small, discardable, same process as `page1`
  auto page2 = CreateNode<PageNodeImpl>();
  auto frame2 = CreateFrameNodeAutoId(process1.get(), page2.get());
  testing::MakePageNodeDiscardable(page2.get(), task_env());
  AdvanceClock(base::Milliseconds(1));

  // Large, discardable
  auto process3 = CreateRendererProcessNode();
  process3->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  auto page3 = CreateNode<PageNodeImpl>();
  auto frame3 = CreateFrameNodeAutoId(process3.get(), page3.get());
  testing::MakePageNodeDiscardable(page3.get(), task_env());
  process3->set_private_footprint(base::MiB(600));
  AdvanceClock(base::Milliseconds(1));

  // Recent, small, discardable
  auto process4 = CreateRendererProcessNode();
  process4->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  auto page4 = CreateNode<PageNodeImpl>();
  auto frame4 = CreateFrameNodeAutoId(process4.get(), page4.get());
  testing::MakePageNodeDiscardable(page4.get(), task_env());
  process4->set_private_footprint(base::MiB(80));
  AdvanceClock(base::Milliseconds(1));

  // Not a renderer
  auto process5 = CreateNode<ProcessNodeImpl>(BrowserProcessNodeTag{});
  process5->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  AdvanceClock(base::Milliseconds(1));

  // Renderer without a valid process handle
  auto process6 = CreateRendererProcessNode();
  AdvanceClock(base::Milliseconds(1));

  // Verify that termination targets are set correctly.
  EXPECT_CALL(*termination_target_setter_,
              SetTerminationTarget(process3.get()));
  GetSystemNode()->OnProcessMemoryMetricsAvailable();
  ::testing::Mock::VerifyAndClearExpectations(termination_target_setter_);

  EXPECT_CALL(*termination_target_setter_,
              SetTerminationTarget(process4.get()));
  frame3.reset();
  page3.reset();
  process3.reset();
  ::testing::Mock::VerifyAndClearExpectations(termination_target_setter_);

  EXPECT_CALL(*termination_target_setter_,
              SetTerminationTarget(process1.get()));
  frame1.reset();
  page1.reset();
  ::testing::Mock::VerifyAndClearExpectations(termination_target_setter_);

  EXPECT_CALL(*termination_target_setter_,
              SetTerminationTarget(process4.get()));
  page2->SetIsVisible(true);
  page2->SetIsVisible(false);
  ::testing::Mock::VerifyAndClearExpectations(termination_target_setter_);

  frame2.reset();
  EXPECT_CALL(*termination_target_setter_,
              SetTerminationTarget(process1.get()));
  page2.reset();
  ::testing::Mock::VerifyAndClearExpectations(termination_target_setter_);

  EXPECT_CALL(*termination_target_setter_,
              SetTerminationTarget(process4.get()));
  process1.reset();
  ::testing::Mock::VerifyAndClearExpectations(termination_target_setter_);

  EXPECT_CALL(*termination_target_setter_, SetTerminationTarget(nullptr));
  frame4.reset();
  page4.reset();
  process4.reset();
  ::testing::Mock::VerifyAndClearExpectations(termination_target_setter_);
}

}  // namespace performance_manager
