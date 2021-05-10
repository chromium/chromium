// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_PAGE_DISCARDING_UTILS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_PAGE_DISCARDING_UTILS_H_

#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace performance_manager {

class FrameNodeImpl;
class PageNodeImpl;
class ProcessNodeImpl;

namespace testing {

// Mock version of a performance_manager::mechanism::PageDiscarder.
class LenientMockPageDiscarder
    : public performance_manager::mechanism::PageDiscarder {
 public:
  LenientMockPageDiscarder();
  ~LenientMockPageDiscarder() override;
  LenientMockPageDiscarder(const LenientMockPageDiscarder& other) = delete;
  LenientMockPageDiscarder& operator=(const LenientMockPageDiscarder&) = delete;

  MOCK_METHOD1(DiscardPageNodeImpl, bool(const PageNode* page_node));

 private:
  void DiscardPageNode(const PageNode* page_node,
                       base::OnceCallback<void(bool)> post_discard_cb) override;
};
using MockPageDiscarder = ::testing::StrictMock<LenientMockPageDiscarder>;

// Specialization of a GraphTestHarness that uses a MockPageDiscarder to
// do the discard attempts.
class GraphTestHarnessWithMockDiscarder : public GraphTestHarness {
 public:
  GraphTestHarnessWithMockDiscarder();
  ~GraphTestHarnessWithMockDiscarder() override;
  GraphTestHarnessWithMockDiscarder(
      const GraphTestHarnessWithMockDiscarder& other) = delete;
  GraphTestHarnessWithMockDiscarder& operator=(
      const GraphTestHarnessWithMockDiscarder&) = delete;

  void SetUp() override;
  void TearDown() override;

 protected:
  PageNodeImpl* page_node() { return page_node_.get(); }
  ProcessNodeImpl* process_node() { return process_node_.get(); }
  FrameNodeImpl* frame_node() { return main_frame_node_.get(); }
  SystemNodeImpl* system_node() { return system_node_.get(); }
  void ResetFrameNode() { main_frame_node_.reset(); }
  testing::MockPageDiscarder* discarder() { return mock_discarder_; }

 private:
  testing::MockPageDiscarder* mock_discarder_;
  performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>
      page_node_;
  performance_manager::TestNodeWrapper<performance_manager::ProcessNodeImpl>
      process_node_;
  performance_manager::TestNodeWrapper<performance_manager::FrameNodeImpl>
      main_frame_node_;
  performance_manager::TestNodeWrapper<performance_manager::SystemNodeImpl>
      system_node_;
};

// Make sure that |page_node| is discardable.
void MakePageNodeDiscardable(PageNodeImpl* page_node,
                             content::BrowserTaskEnvironment& task_env);

}  // namespace testing
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_TEST_SUPPORT_PAGE_DISCARDING_UTILS_H_
