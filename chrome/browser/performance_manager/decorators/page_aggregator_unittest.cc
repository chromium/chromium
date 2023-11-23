// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/page_aggregator.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class PageAggregatorTest : public GraphTestHarness {
 public:
  void SetUp() override {
    GraphTestHarness::SetUp();
    graph()->PassToGraph(std::make_unique<PageAggregator>());
  }
};

}  // namespace

TEST_F(PageAggregatorTest, WebLocksAggregation) {
  // Creates a page containing 2 frames.
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  TestNodeWrapper<FrameNodeImpl> frame_0 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());
  TestNodeWrapper<FrameNodeImpl> frame_1 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());

  // By default the page shouldn't hold any WebLock.
  EXPECT_FALSE(page->IsHoldingWebLock());

  // |frame_0| now holds a WebLock, the corresponding property should be set on
  // the page node.
  frame_0->SetIsHoldingWebLock(true);
  EXPECT_TRUE(page->IsHoldingWebLock());

  // |frame_1| also holding a WebLock shouldn't affect the page property.
  frame_1->SetIsHoldingWebLock(true);
  EXPECT_TRUE(page->IsHoldingWebLock());

  // |frame_1| still holds a WebLock after this.
  frame_0->SetIsHoldingWebLock(false);
  EXPECT_TRUE(page->IsHoldingWebLock());

  // Destroying |frame_1| without explicitly releasing the WebLock it's
  // holding should update the corresponding page property.
  frame_1.reset();
  EXPECT_FALSE(page->IsHoldingWebLock());
}

TEST_F(PageAggregatorTest, IndexedDBLocksAggregation) {
  // Creates a page containing 2 frames.
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  TestNodeWrapper<FrameNodeImpl> frame_0 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());
  TestNodeWrapper<FrameNodeImpl> frame_1 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());

  // By default the page shouldn't hold any IndexedDB lock.
  EXPECT_FALSE(page->IsHoldingIndexedDBLock());

  // |frame_0| now holds an IndexedDB lock, the corresponding property should be
  // set on the page node.
  frame_0->SetIsHoldingIndexedDBLock(true);
  EXPECT_TRUE(page->IsHoldingIndexedDBLock());

  // |frame_1| also holding an IndexedDB lock shouldn't affect the page
  // property.
  frame_1->SetIsHoldingIndexedDBLock(true);
  EXPECT_TRUE(page->IsHoldingIndexedDBLock());

  // |frame_1| still holds an IndexedDB lock after this.
  frame_0->SetIsHoldingIndexedDBLock(false);
  EXPECT_TRUE(page->IsHoldingIndexedDBLock());

  // Destroying |frame_1| without explicitly releasing the IndexedDB lock it's
  // holding should update the corresponding page property.
  frame_1.reset();
  EXPECT_FALSE(page->IsHoldingIndexedDBLock());
}

}  // namespace performance_manager
