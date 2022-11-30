// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/spare_render_process_host_task_provider.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_manager {

class SpareRenderProcessHostTaskTest : public testing::Test,
                                       public TaskProviderObserver {
 public:
  SpareRenderProcessHostTaskTest() = default;
  ~SpareRenderProcessHostTaskTest() override = default;

  // task_manager::TaskProviderObserver:
  void TaskAdded(Task* task) override { provided_task_ = task; }

  void TaskRemoved(Task* task) override {
    ASSERT_EQ(provided_task_, task);
    provided_task_ = nullptr;
  }

  void SpareRenderProcessHostTaskChanged(
      SpareRenderProcessHostTaskProvider* provider,
      content::RenderProcessHost* render_process) {
    provider->SpareRenderProcessHostTaskChanged(render_process);
  }

 protected:
  raw_ptr<Task> provided_task_ = nullptr;

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(SpareRenderProcessHostTaskTest, Basic) {
  SpareRenderProcessHostTaskProvider provider;
  provider.SetObserver(this);
  EXPECT_EQ(nullptr, provided_task_.get());

  auto browser_context = std::make_unique<TestingProfile>();
  auto render_process =
      std::make_unique<content::MockRenderProcessHost>(browser_context.get());

  SpareRenderProcessHostTaskChanged(&provider, render_process.get());
  EXPECT_NE(nullptr, provided_task_);

  SpareRenderProcessHostTaskChanged(&provider, nullptr);
  EXPECT_EQ(nullptr, provided_task_.get());

  provider.ClearObserver();
}

}  // namespace task_manager
