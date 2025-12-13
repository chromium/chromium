// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/termination_target_setter.h"

#include "base/test/test_timeouts.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/no_renderer_crashes_assertion.h"

namespace performance_manager {

namespace {

class TerminationTargetSetterBrowserTest : public InProcessBrowserTest {};

class ProcessExitObserver : public content::RenderProcessHostObserver {
 public:
  explicit ProcessExitObserver(content::RenderProcessHost* rph) : rph_(rph) {
    rph_->AddObserver(this);
  }
  ~ProcessExitObserver() override = default;

  void WaitForExit() {
    run_loop_.Run();
    EXPECT_FALSE(rph_);
  }

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override {
    EXPECT_EQ(host, rph_);
    EXPECT_EQ(info.exit_code,
              CHROME_RESULT_CODE_TERMINATED_BY_OTHER_PROCESS_ON_COMMIT_FAILURE);

    rph_->RemoveObserver(this);
    rph_ = nullptr;
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  raw_ptr<content::RenderProcessHost> rph_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(TerminationTargetSetterBrowserTest, Basic) {
  // This test intentionally crashes a renderer.
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  raw_ptr<content::RenderProcessHost> rph =
      contents->GetPrimaryPage().GetMainDocument().GetProcess();
  ProcessExitObserver exit_observer(rph);

  base::WeakPtr<ProcessNode> process_node_to_terminate =
      PerformanceManager::GetProcessNodeForRenderProcessHost(rph);
  ASSERT_TRUE(process_node_to_terminate);

  TerminationTargetSetter setter;
  setter.SetTerminationTarget(process_node_to_terminate.get());

  partition_alloc::TerminateAnotherProcessOnCommitFailure();

  exit_observer.WaitForExit();
}

IN_PROC_BROWSER_TEST_F(TerminationTargetSetterBrowserTest, SetNull) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  raw_ptr<content::RenderProcessHost> rph =
      contents->GetPrimaryPage().GetMainDocument().GetProcess();

  base::WeakPtr<ProcessNode> process_node_to_terminate =
      PerformanceManager::GetProcessNodeForRenderProcessHost(rph);
  ASSERT_TRUE(process_node_to_terminate);

  TerminationTargetSetter setter;

  // Set a termination target.
  setter.SetTerminationTarget(process_node_to_terminate.get());

  // Reset the termination target.
  setter.SetTerminationTarget(nullptr);

  // This should not terminate a process.
  partition_alloc::TerminateAnotherProcessOnCommitFailure();

  // Let the browser run for some time. If the process was terminated, that is
  // necessary for the `rph` to be notified.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  EXPECT_TRUE(rph->IsInitializedAndNotDead());
}

}  // namespace performance_manager
