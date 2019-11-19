// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/render_process_host_proxy.h"

#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/test/bind_test_util.h"
// TODO(https://crbug.com/953031): Remove these dependencies and move this
//     test to the component directory.
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/render_process_user_data.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class RenderProcessHostProxyTest : public ChromeRenderViewHostTestHarness {
 protected:
  RenderProcessHostProxyTest() {}
  ~RenderProcessHostProxyTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    perf_man_ = PerformanceManagerImpl::Create(base::DoNothing());
  }
  void TearDown() override {
    // Have the performance manager destroy itself.
    PerformanceManagerImpl::Destroy(std::move(perf_man_));
    task_environment()->RunUntilIdle();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    std::unique_ptr<content::WebContents> contents =
        ChromeRenderViewHostTestHarness::CreateTestWebContents();
    PerformanceManagerTabHelper::CreateForWebContents(contents.get());
    return contents;
  }

 private:
  std::unique_ptr<PerformanceManagerImpl> perf_man_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostProxyTest);
};

TEST_F(RenderProcessHostProxyTest, RPHDeletionInvalidatesProxy) {
  //  content::RenderProcessHost* host(
  //      rph_factory_->CreateRenderProcessHost(profile_, nullptr));
  std::unique_ptr<TestingProfileManager> profile_manager(
      new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  ASSERT_TRUE(profile_manager->SetUp());

  // Owned by profile_manager.
  TestingProfile* profile(
      profile_manager->CreateTestingProfile("RPHTestProfile"));

  std::unique_ptr<content::MockRenderProcessHostFactory> rph_factory(
      new content::MockRenderProcessHostFactory());
  scoped_refptr<content::SiteInstance> site_instance(
      content::SiteInstance::Create(profile));

  // Owned by rph_factory.
  content::RenderProcessHost* host(
      rph_factory->CreateRenderProcessHost(profile, site_instance.get()));

  // Now create a RenderProcessUserData which creates a ProcessNode.
  auto* render_process_user_data =
      RenderProcessUserData::GetOrCreateForRenderProcessHost(host);
  ASSERT_NE(render_process_user_data, nullptr);
  ProcessNode* process_node = render_process_user_data->process_node();
  ASSERT_NE(process_node, nullptr);

  content::RenderProcessHost* proxy_contents = nullptr;

  auto deref_proxy = base::BindLambdaForTesting(
      [&proxy_contents](const RenderProcessHostProxy& proxy,
                        base::OnceClosure quit_loop) {
        proxy_contents = proxy.Get();
        std::move(quit_loop).Run();
      });

  // Bounce over to the PM sequence, retrieve the proxy, bounce back to the UI
  // thread, dereference it if possible, and save the returned contents. To be
  // fair, it's entirely valid to grab the weak pointer directly on the UI
  // thread, as the lifetime of the process node is managed there and the
  // property being accessed is thread safe. However, this test aims to simulate
  // what would happen with a policy message being posted from the graph.
  {
    base::RunLoop run_loop;
    PerformanceManagerImpl::GetInstance()->CallOnGraphImpl(
        FROM_HERE,
        base::BindLambdaForTesting(
            [&deref_proxy, process_node,
             quit_loop = run_loop.QuitClosure()](GraphImpl* graph) {
              base::PostTask(
                  FROM_HERE, {content::BrowserThread::UI},
                  base::BindOnce(deref_proxy,
                                 process_node->GetRenderProcessHostProxy(),
                                 std::move(quit_loop)));
            }));
    run_loop.Run();

    // We should see the RPH via the proxy.
    EXPECT_EQ(host, proxy_contents);
  }

  // Run the same test but make sure the RPH is gone first.
  {
    base::RunLoop run_loop;
    PerformanceManagerImpl::GetInstance()->CallOnGraphImpl(
        FROM_HERE,
        base::BindLambdaForTesting([&rph_factory, &deref_proxy, process_node,
                                    host, quit_loop = run_loop.QuitClosure()](
                                       GraphImpl* graph) {
          base::PostTask(
              FROM_HERE, {content::BrowserThread::UI},
              base::BindLambdaForTesting([&rph_factory, host]() {
                rph_factory->Remove(
                    reinterpret_cast<content::MockRenderProcessHost*>(host));
                delete host;
              }));
          base::PostTask(
              FROM_HERE, {content::BrowserThread::UI},
              base::BindOnce(deref_proxy,
                             process_node->GetRenderProcessHostProxy(),
                             std::move(quit_loop)));
        }));
    run_loop.Run();

    // The contents was destroyed on the UI thread prior to dereferencing the
    // proxy, so it should return nullptr.
    EXPECT_EQ(proxy_contents, nullptr);
  }
}

}  // namespace performance_manager
