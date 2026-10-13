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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <string>

#include "base/files/file_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/posix/eintr_wrapper.h"
#include "base/test/test_future.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "printing/print_settings.h"
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

TEST_F(PrintViewManagerBasicTest, AndroidPrintNowAndGetDefaultPrintSettings) {
  auto* print_view_manager =
      PrintViewManagerBasic::FromWebContents(web_contents());
  ASSERT_TRUE(print_view_manager);

  auto settings = std::make_unique<PrintSettings>();
  settings->set_dpi(300);
  int width_in_dots = 2400;
  int height_in_dots = 3300;
  gfx::Size physical_size_device_units(width_in_dots, height_in_dots);
  gfx::Rect printable_area_device_units(0, 0, width_in_dots, height_in_dots);
  settings->SetPrinterPrintableArea(physical_size_device_units,
                                    printable_area_device_units, false);

  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
  ASSERT_TRUE(base::CreatePipe(&read_fd, &write_fd));

  EXPECT_TRUE(print_view_manager->PrintNowAndroid(
      main_rfh(), std::move(settings), std::move(write_fd)));
  EXPECT_TRUE(print_view_manager->GetPrintingRFHForTesting());

  mojo::AssociatedRemote<mojom::PrintManagerHost> host;
  PrintViewManagerBasic::BindPrintManagerHost(
      host.BindNewEndpointAndPassDedicatedReceiver(), main_rfh());

  // (a) PrintNowAndroid() sets settings that GetDefaultPrintSettings() returns.
  base::test::TestFuture<mojom::PrintParamsPtr> settings_future;
  host->GetDefaultPrintSettings(settings_future.GetCallback());
  mojom::PrintParamsPtr params = settings_future.Take();
  ASSERT_TRUE(params);
  EXPECT_EQ(params->dpi.width(), 300);

  // ScriptedPrint() rejects a mismatched cookie without overwriting the active
  // job's cookie, and succeeds when given the cookie from PrintNowAndroid().
  auto bad_scripted_params = mojom::ScriptedPrintParams::New();
  bad_scripted_params->cookie = params->document_cookie + 1;
  base::test::TestFuture<mojom::PrintPagesParamsPtr> bad_scripted_future;
  host->ScriptedPrint(std::move(bad_scripted_params),
                      bad_scripted_future.GetCallback());
  EXPECT_FALSE(bad_scripted_future.Take());

  auto scripted_params = mojom::ScriptedPrintParams::New();
  scripted_params->cookie = params->document_cookie;
  base::test::TestFuture<mojom::PrintPagesParamsPtr> scripted_future;
  host->ScriptedPrint(std::move(scripted_params),
                      scripted_future.GetCallback());
  mojom::PrintPagesParamsPtr page_params = scripted_future.Take();
  ASSERT_TRUE(page_params);
  EXPECT_EQ(page_params->params->document_cookie, params->document_cookie);

  // (b) Another frame in the same WebContents cannot consume the settings.
  auto* sub_frame =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("sub");
  ASSERT_TRUE(sub_frame);
  content::RenderFrameHostTester::For(sub_frame)
      ->InitializeRenderFrameIfNeeded();

  mojo::AssociatedRemote<mojom::PrintManagerHost> sub_host;
  PrintViewManagerBasic::BindPrintManagerHost(
      sub_host.BindNewEndpointAndPassDedicatedReceiver(), sub_frame);

  base::test::TestFuture<mojom::PrintParamsPtr> sub_settings_future;
  sub_host->GetDefaultPrintSettings(sub_settings_future.GetCallback());
  EXPECT_FALSE(sub_settings_future.Take());

  // (c) Nor can it consume the descriptor the job writes into. The descriptor
  // is still held by the manager, so the read end of the pipe must not be at
  // end of file.
  auto did_print_params = mojom::DidPrintDocumentParams::New();
  did_print_params->content = mojom::DidPrintContentParams::New();
  base::test::TestFuture<bool> did_print_future;
  sub_host->DidPrintDocument(std::move(did_print_params),
                             did_print_future.GetCallback());
  EXPECT_FALSE(did_print_future.Take());

  ASSERT_TRUE(base::SetNonBlocking(read_fd.get()));
  char byte;
  EXPECT_EQ(-1, HANDLE_EINTR(read(read_fd.get(), &byte, 1)));
  EXPECT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);

  // (d) FinishPrint() clears the Android settings.
  print_view_manager->FinishPrint(main_rfh());
  EXPECT_FALSE(print_view_manager->GetPrintingRFHForTesting());
}

TEST_F(PrintViewManagerBasicTest, StaleWriteCompletionKeepsNextJob) {
  auto* print_view_manager =
      PrintViewManagerBasic::FromWebContents(web_contents());
  ASSERT_TRUE(print_view_manager);

  auto make_settings = [] {
    auto settings = std::make_unique<PrintSettings>();
    settings->set_dpi(300);
    settings->SetPrinterPrintableArea(gfx::Size(2400, 3300),
                                      gfx::Rect(0, 0, 2400, 3300), false);
    // Keeps DidPrintDocument() off the PrintCompositeClient path, which this
    // harness does not provide.
    settings->set_is_modifiable(false);
    return settings;
  };

  base::ScopedFD read_a;
  base::ScopedFD write_a;
  ASSERT_TRUE(base::CreatePipe(&read_a, &write_a));
  // Fill the pipe so that the job's write on the worker thread blocks until the
  // test drains it, i.e. until after the next job has started.
  ASSERT_TRUE(base::SetNonBlocking(write_a.get()));
  const std::string chunk(4096, 'x');
  while (HANDLE_EINTR(write(write_a.get(), chunk.data(), chunk.size())) > 0) {
  }
  ASSERT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);
  const int flags = fcntl(write_a.get(), F_GETFL);
  ASSERT_NE(-1, fcntl(write_a.get(), F_SETFL, flags & ~O_NONBLOCK));

  ASSERT_TRUE(print_view_manager->PrintNowAndroid(main_rfh(), make_settings(),
                                                  std::move(write_a)));

  mojo::AssociatedRemote<mojom::PrintManagerHost> host;
  PrintViewManagerBasic::BindPrintManagerHost(
      host.BindNewEndpointAndPassDedicatedReceiver(), main_rfh());

  base::test::TestFuture<mojom::PrintParamsPtr> settings_future;
  host->GetDefaultPrintSettings(settings_future.GetCallback());
  mojom::PrintParamsPtr params = settings_future.Take();
  ASSERT_TRUE(params);

  base::MappedReadOnlyRegion pdf = base::ReadOnlySharedMemoryRegion::Create(4);
  ASSERT_TRUE(pdf.IsValid());
  auto did_print_params = mojom::DidPrintDocumentParams::New();
  did_print_params->document_cookie = params->document_cookie;
  did_print_params->content = mojom::DidPrintContentParams::New();
  did_print_params->content->metafile_data_region = std::move(pdf.region);
  base::test::TestFuture<bool> did_print_future;
  host->DidPrintDocument(std::move(did_print_params),
                         did_print_future.GetCallback());
  host.FlushForTesting();

  print_view_manager->FinishPrint(main_rfh());

  base::ScopedFD read_b;
  base::ScopedFD write_b;
  ASSERT_TRUE(base::CreatePipe(&read_b, &write_b));
  ASSERT_TRUE(print_view_manager->PrintNowAndroid(main_rfh(), make_settings(),
                                                  std::move(write_b)));

  // Unblock the first job's write and wait until the worker has closed its
  // descriptor, then for the completion to reach the manager.
  char buffer[4096];
  while (HANDLE_EINTR(read(read_a.get(), buffer, sizeof(buffer))) > 0) {
  }
  ASSERT_TRUE(did_print_future.Wait());

  // The second job still owns its descriptor.
  ASSERT_TRUE(base::SetNonBlocking(read_b.get()));
  EXPECT_EQ(-1, HANDLE_EINTR(read(read_b.get(), buffer, 1)));
  EXPECT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);

  print_view_manager->FinishPrint(main_rfh());
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace printing
