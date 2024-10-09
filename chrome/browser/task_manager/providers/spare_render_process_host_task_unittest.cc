// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/task_manager/providers/spare_render_process_host_task_provider.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Contains;
using testing::Matcher;
using testing::Not;
using testing::Property;
using testing::Test;

namespace task_manager {

namespace {

// Matcher that checks if the set contains an element where
// GetChildProcessUniqueID() == rph_id;
Matcher<std::set<Task*>> ContainsRphId(int rph_id) {
  return Contains(Property(&Task::GetChildProcessUniqueID, rph_id));
}

}  // namespace

class SpareRenderProcessHostTaskTest : public Test,
                                       public TaskProviderObserver {
 public:
  SpareRenderProcessHostTaskTest() = default;
  ~SpareRenderProcessHostTaskTest() override = default;

  // task_manager::TaskProviderObserver:
  void TaskAdded(Task* task) override {
    bool inserted = provided_tasks_.insert(task).second;
    CHECK(inserted);
  }

  void TaskRemoved(Task* task) override {
    size_t removed = provided_tasks_.erase(task);
    CHECK_EQ(removed, 1u);
  }

  void OnSpareRenderProcessHostReady(
      SpareRenderProcessHostTaskProvider* provider,
      content::RenderProcessHost* render_process) {
    provider->OnSpareRenderProcessHostReady(render_process);
  }

  void OnSpareRenderProcessHostRemoved(
      SpareRenderProcessHostTaskProvider* provider,
      content::RenderProcessHost* render_process) {
    provider->OnSpareRenderProcessHostRemoved(render_process);
  }

  const std::set<Task*>& provided_tasks() const { return provided_tasks_; }

 private:
  std::set<Task*> provided_tasks_;

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(SpareRenderProcessHostTaskTest, Basic) {
  SpareRenderProcessHostTaskProvider provider;
  provider.SetObserver(this);

  auto browser_context = std::make_unique<TestingProfile>();
  auto render_process1 =
      std::make_unique<content::MockRenderProcessHost>(browser_context.get());
  auto render_process2 =
      std::make_unique<content::MockRenderProcessHost>(browser_context.get());

  EXPECT_TRUE(provided_tasks().empty());

  OnSpareRenderProcessHostReady(&provider, render_process1.get());
  EXPECT_THAT(provided_tasks(), ContainsRphId(render_process1->GetID()));
  EXPECT_THAT(provided_tasks(), Not(ContainsRphId(render_process2->GetID())));

  OnSpareRenderProcessHostReady(&provider, render_process2.get());
  EXPECT_THAT(provided_tasks(), ContainsRphId(render_process1->GetID()));
  EXPECT_THAT(provided_tasks(), ContainsRphId(render_process2->GetID()));

  OnSpareRenderProcessHostRemoved(&provider, render_process1.get());
  EXPECT_THAT(provided_tasks(), Not(ContainsRphId(render_process1->GetID())));
  EXPECT_THAT(provided_tasks(), ContainsRphId(render_process2->GetID()));

  OnSpareRenderProcessHostRemoved(&provider, render_process2.get());
  EXPECT_TRUE(provided_tasks().empty());

  provider.ClearObserver();
}

}  // namespace task_manager
