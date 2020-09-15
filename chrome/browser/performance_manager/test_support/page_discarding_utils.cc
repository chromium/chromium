// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"

#include "base/time/time.h"
#include "chrome/browser/performance_manager/decorators/page_aggregator.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"

namespace performance_manager {
namespace testing {

namespace {

// Test version of the PageDiscardingHelper.
class TestPageDiscardingHelper : public policies::PageDiscardingHelper {
 public:
  TestPageDiscardingHelper();
  ~TestPageDiscardingHelper() override;
  TestPageDiscardingHelper(const TestPageDiscardingHelper& other) = delete;
  TestPageDiscardingHelper& operator=(const TestPageDiscardingHelper&) = delete;

 protected:
  const PageLiveStateDecorator::Data* GetPageNodeLiveStateData(
      const PageNode* page_node) const override {
    // Returns a fake version of the PageLiveStateDecorator::Data, create it if
    // it doesn't exist. Tests that want to set some fake live state data should
    // call |FakePageLiveStateData::GetOrCreate|.
    return FakePageLiveStateData::GetOrCreate(
        PageNodeImpl::FromNode(page_node));
  }
};

}  // namespace

FakePageLiveStateData::~FakePageLiveStateData() = default;

// PageLiveStateDecorator::Data:
bool FakePageLiveStateData::IsConnectedToUSBDevice() const {
  return is_connected_to_usb_device_;
}
bool FakePageLiveStateData::IsConnectedToBluetoothDevice() const {
  return is_connected_to_bluetooth_device_;
}
bool FakePageLiveStateData::IsCapturingVideo() const {
  return is_capturing_video_;
}
bool FakePageLiveStateData::IsCapturingAudio() const {
  return is_capturing_audio_;
}
bool FakePageLiveStateData::IsBeingMirrored() const {
  return is_being_mirrored_;
}
bool FakePageLiveStateData::IsCapturingWindow() const {
  return is_capturing_window_;
}
bool FakePageLiveStateData::IsCapturingDisplay() const {
  return is_capturing_display_;
}
bool FakePageLiveStateData::IsAutoDiscardable() const {
  return is_auto_discardable_;
}
bool FakePageLiveStateData::WasDiscarded() const {
  return was_discarded_;
}

FakePageLiveStateData::FakePageLiveStateData(const PageNodeImpl* page_node) {}

LenientMockPageDiscarder::LenientMockPageDiscarder() = default;
LenientMockPageDiscarder::~LenientMockPageDiscarder() = default;

void LenientMockPageDiscarder::DiscardPageNode(
    const PageNode* page_node,
    base::OnceCallback<void(bool)> post_discard_cb) {
  std::move(post_discard_cb).Run(DiscardPageNodeImpl(page_node));
}

TestPageDiscardingHelper::TestPageDiscardingHelper() = default;
TestPageDiscardingHelper::~TestPageDiscardingHelper() = default;

GraphTestHarnessWithMockDiscarder::GraphTestHarnessWithMockDiscarder() {
  // Some tests depends on the existence of the PageAggregator.
  graph()->PassToGraph(std::make_unique<PageAggregator>());
}

GraphTestHarnessWithMockDiscarder::~GraphTestHarnessWithMockDiscarder() =
    default;

void GraphTestHarnessWithMockDiscarder::SetUp() {
  GraphTestHarness::SetUp();

  // Make the policy use a mock PageDiscarder.
  auto mock_discarder = std::make_unique<MockPageDiscarder>();
  mock_discarder_ = mock_discarder.get();

  // Create the helper and pass it to the graph.
  auto page_discarding_helper = std::make_unique<TestPageDiscardingHelper>();
  page_discarding_helper->SetMockDiscarderForTesting(std::move(mock_discarder));

  graph()->PassToGraph(std::move(page_discarding_helper));
  DCHECK(policies::PageDiscardingHelper::GetFromGraph(graph()));

  // Create a PageNode and make it discardable.
  process_node_ = CreateNode<performance_manager::ProcessNodeImpl>();
  page_node_ = CreateNode<performance_manager::PageNodeImpl>();
  main_frame_node_ =
      CreateFrameNodeAutoId(process_node_.get(), page_node_.get());
  main_frame_node_->SetIsCurrent(true);
  MakePageNodeDiscardable(page_node(), task_env());
}

void GraphTestHarnessWithMockDiscarder::TearDown() {
  main_frame_node_.reset();
  page_node_.reset();
  process_node_.reset();
  GraphTestHarness::TearDown();
}

void MakePageNodeDiscardable(PageNodeImpl* page_node,
                             content::BrowserTaskEnvironment& task_env) {
  page_node->SetIsVisible(false);
  page_node->SetIsAudible(false);
  const auto kUrl = GURL("https://foo.com");
  page_node->OnMainFrameNavigationCommitted(false, base::TimeTicks::Now(), 42,
                                            kUrl, "text/html");
  (*page_node->main_frame_nodes().begin())->OnNavigationCommitted(kUrl, false);
  task_env.FastForwardBy(base::TimeDelta::FromMinutes(10));
  DCHECK(policies::PageDiscardingHelper::GetFromGraph(page_node->graph())
             ->CanUrgentlyDiscardForTesting(page_node));
}

}  // namespace testing
}  // namespace performance_manager
