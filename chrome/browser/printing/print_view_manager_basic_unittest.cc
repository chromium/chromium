// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_basic.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/printing/printing_init.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_process_host.h"
#include "printing/mojom/print.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "url/gurl.h"
#endif

namespace printing {

class PrintViewManagerBasicTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    InitializePrintingForWebContents(web_contents());
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
  }
};

TEST_F(PrintViewManagerBasicTest, PrintSubFrameAndDestroy) {
  auto* sub_frame =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  ASSERT_TRUE(sub_frame);

  auto* print_view_manager =
      PrintViewManagerBasic::FromWebContents(web_contents());
  ASSERT_TRUE(print_view_manager);
  EXPECT_FALSE(print_view_manager->GetPrintingRFHForTesting());

  print_view_manager->PrintNow(sub_frame);
  EXPECT_TRUE(print_view_manager->GetPrintingRFHForTesting());

  content::RenderFrameHostTester::For(sub_frame)->Detach();
  EXPECT_FALSE(print_view_manager->GetPrintingRFHForTesting());
}

TEST_F(PrintViewManagerBasicTest, CancelJobDuringDestruction) {
  auto* print_view_manager =
      PrintViewManagerBasic::FromWebContents(web_contents());
  ASSERT_TRUE(print_view_manager);

  ASSERT_TRUE(print_view_manager->PrintNow(main_rfh()));

  // Setup enough of a PrinterQuery to make GetPrintedPagesCount work
  auto queue = g_browser_process->print_job_manager()->queue();
  auto query = queue->CreatePrinterQuery(main_rfh()->GetGlobalId());
  base::RunLoop runloop;
  query->SetSettings(test::GetPrintTicket(mojom::PrinterType::kLocal),
                     runloop.QuitClosure());
  runloop.Run();
  auto cookie = query->cookie();
  queue->QueuePrinterQuery(std::move(query));

  // Fake DidGetPrintedPagesCount() call to cause print_job to be created
  print_view_manager->DidGetPrintedPagesCount(cookie, 1);

  DeleteContents();
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(PrintViewManagerBasicTest, InitiatePrintAndFinishPrint) {
  auto* print_view_manager =
      PrintViewManagerBasic::FromWebContents(web_contents());
  ASSERT_TRUE(print_view_manager);

  EXPECT_TRUE(print_view_manager->InitiatePrint(main_rfh()));
  print_view_manager->FinishPrint(main_rfh());

  // Null frame should safely return false for InitiatePrint()
  // and no-op without crashing for FinishPrint().
  EXPECT_FALSE(print_view_manager->InitiatePrint(/*rfh=*/nullptr));
  print_view_manager->FinishPrint(/*rfh=*/nullptr);
}

TEST_F(PrintViewManagerBasicTest, CrashedRenderer) {
  auto* print_view_manager =
      PrintViewManagerBasic::FromWebContents(web_contents());
  ASSERT_TRUE(print_view_manager);

  // Simulate render process crash.
  auto* rph =
      static_cast<content::MockRenderProcessHost*>(main_rfh()->GetProcess());
  rph->SimulateCrash();

  EXPECT_TRUE(web_contents()->IsCrashed());
  EXPECT_FALSE(main_rfh()->IsRenderFrameLive());
  EXPECT_FALSE(print_view_manager->InitiatePrint(main_rfh()));
  // FinishPrint() should safely no-op without crashing when renderer is
  // crashed.
  print_view_manager->FinishPrint(main_rfh());
}

TEST_F(PrintViewManagerBasicTest, InactiveOrBackForwardCachedFrame) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  // Navigate to the first page.
  NavigateAndCommit(kUrl1);

  auto* print_view_manager =
      PrintViewManagerBasic::FromWebContents(web_contents());
  ASSERT_TRUE(print_view_manager);

  // Navigate to new cross-site page, allocating a speculative (inactive) frame.
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      kUrl2, web_contents());
  simulator->ReadyToCommit();
  content::RenderFrameHost* speculative_rfh =
      simulator->GetFinalRenderFrameHost();
  ASSERT_TRUE(speculative_rfh);
  content::RenderFrameHostTester::For(speculative_rfh)
      ->InitializeRenderFrameIfNeeded();

  ASSERT_FALSE(speculative_rfh->IsActive());
  ASSERT_TRUE(speculative_rfh->IsRenderFrameLive());

  // Inactive frames cannot initiate print.
  EXPECT_FALSE(print_view_manager->InitiatePrint(speculative_rfh));

  // FinishPrint() must still deliver teardown to live frames even when
  // inactive.
  print_view_manager->FinishPrint(speculative_rfh);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace printing
