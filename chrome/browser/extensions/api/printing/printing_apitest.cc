// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/extensions/api/printing/print_job_submitter.h"
#include "chrome/browser/extensions/api/printing/printing_test_utils.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/printing/local_printer_utils_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/extensions/api/printing/fake_print_job_controller.h"
#include "chrome/browser/extensions/api/printing/printing_api_handler.h"
#include "chrome/test/chromeos/printing/mock_local_printer_chromeos.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/printing/printer_configuration.h"
#endif

namespace extensions {

namespace {

constexpr char kId[] = "id";
constexpr char kName[] = "name";

#if BUILDFLAG(IS_CHROMEOS_LACROS)

using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::NiceMock;
using testing::Return;
using testing::WithArg;
using testing::WithArgs;
using testing::WithoutArgs;

#endif

}  // namespace

class PrintingApiTestBase : public ExtensionApiTest,
                            public testing::WithParamInterface<ExtensionType> {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    PrintJobSubmitter::SkipConfirmationDialogForTesting();
  }

 protected:
  ExtensionType GetExtensionType() const { return GetParam(); }

  void RunTest(const char* html_test_page) {
    auto dir = CreatePrintingExtension(GetExtensionType());
    auto run_options = GetExtensionType() == ExtensionType::kChromeApp
                           ? RunOptions{.custom_arg = html_test_page,
                                        .launch_as_platform_app = true}
                           : RunOptions({.extension_url = html_test_page});
    ASSERT_TRUE(RunExtensionTest(dir->UnpackedPath(), run_options, {}));
  }
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
class PrintingApiTest : public PrintingApiTestBase {
 public:
  void PreRunTestOnMainThread() override {
    PrintingApiTestBase::PreRunTestOnMainThread();
    helper_->Init(browser()->profile());
  }

  void TearDownOnMainThread() override {
    helper_.reset();
    PrintingApiTestBase::TearDownOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    PrintingApiTestBase::SetUpInProcessBrowserTestFixture();
    helper_ = std::make_unique<PrintingTestHelper>();
  }

 protected:
  void AddPrinterWithSemanticCaps(
      const std::string& printer_id,
      const std::string& printer_display_name,
      std::unique_ptr<printing::PrinterSemanticCapsAndDefaults> caps) {
    helper_->AddAvailablePrinter(printer_id, printer_display_name,
                                 std::move(caps));
  }

 private:
  std::unique_ptr<PrintingTestHelper> helper_;
};
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
class PrintingApiTest : public PrintingApiTestBase {
 public:
  void SetUpOnMainThread() override {
    PrintingApiTestBase::SetUpOnMainThread();
    printing_infra_helper_ =
        std::make_unique<PrintingBackendInfrastructureHelper>();
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    PrintingApiTestBase::CreatedBrowserMainParts(browser_main_parts);
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        local_printer_receiver_.BindNewPipeAndPassRemote());

    // When PrintingAPIHandler is initiated, it attempts to bind the observer
    // for print jobs.
    EXPECT_CALL(local_printer(), AddPrintJobObserver(_, _, _))
        .WillOnce(WithArgs<0, 2>(
            [&](mojo::PendingRemote<crosapi::mojom::PrintJobObserver> remote,
                MockLocalPrinter::AddPrintJobObserverCallback callback) {
              observer_remote_.Bind(std::move(remote));
              std::move(callback).Run();
            }));
  }

 protected:
  NiceMock<MockLocalPrinter>& local_printer() { return local_printer_; }
  crosapi::mojom::PrintJobObserver* observer_remote() {
    return observer_remote_.get();
  }
  PrintingBackendInfrastructureHelper& printing_infra_helper() {
    return *printing_infra_helper_;
  }

 private:
  NiceMock<MockLocalPrinter> local_printer_;
  mojo::Receiver<crosapi::mojom::LocalPrinter> local_printer_receiver_{
      &local_printer_};
  mojo::Remote<crosapi::mojom::PrintJobObserver> observer_remote_;

  std::unique_ptr<PrintingBackendInfrastructureHelper> printing_infra_helper_;
};
#endif

using PrintingPromiseApiTest = PrintingApiTest;

IN_PROC_BROWSER_TEST_P(PrintingApiTest, GetPrinters) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  AddPrinterWithSemanticCaps(kId, kName, ConstructPrinterCapabilities());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_CALL(local_printer(), GetPrinters(_))
      .WillOnce(base::test::RunOnceCallback<0>(
          ConstructGetPrintersResponse(kId, kName)));
#endif

  RunTest("get_printers.html");
}

IN_PROC_BROWSER_TEST_P(PrintingApiTest, GetPrinterInfo) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  AddPrinterWithSemanticCaps(kId, kName, ConstructPrinterCapabilities());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_CALL(local_printer(), GetCapability(kId, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          printing::PrinterWithCapabilitiesToMojom(
              chromeos::Printer(kId), *ConstructPrinterCapabilities())));
#endif

  RunTest("get_printer_info.html");
}

// Verifies that:
// a) PrintingHooksDelegate substitutes corresponding Blob UUID and DCHECK
// doesn't fail.
// b) Whole API arguments handling pipeline works correctly.
// We use fake version of PrintJobController because we don't have a mock
// version of PrintingContext which is required to handle sending print job to
// the printer.
IN_PROC_BROWSER_TEST_P(PrintingApiTest, SubmitJob) {
  ASSERT_TRUE(StartEmbeddedTestServer());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  AddPrinterWithSemanticCaps(kId, kName, ConstructPrinterCapabilities());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  InSequence s;

  EXPECT_CALL(local_printer(), GetCapability(kId, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          printing::PrinterWithCapabilitiesToMojom(
              chromeos::Printer(kId), *ConstructPrinterCapabilities())));

  // Acknowledge print job creation so that the mojo callback doesn't hang.
  EXPECT_CALL(local_printer(), CreatePrintJob(_, _))
      .WillOnce(base::test::RunOnceCallback<1>());

  printing_infra_helper()
      .test_printing_context_factory()
      .SetPrinterNameForSubsequentContexts(kId);
#endif

  RunTest("submit_job.html");
}

// As above, but tests using promise based API calls.
IN_PROC_BROWSER_TEST_P(PrintingPromiseApiTest, SubmitJob) {
  ASSERT_TRUE(StartEmbeddedTestServer());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  AddPrinterWithSemanticCaps(kId, kName, ConstructPrinterCapabilities());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  InSequence s;

  EXPECT_CALL(local_printer(), GetCapability(kId, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          printing::PrinterWithCapabilitiesToMojom(
              chromeos::Printer(kId), *ConstructPrinterCapabilities())));

  // Acknowledge print job creation so that the mojo callback doesn't hang.
  EXPECT_CALL(local_printer(), CreatePrintJob(_, _))
      .WillOnce(base::test::RunOnceCallback<1>());

  printing_infra_helper()
      .test_printing_context_factory()
      .SetPrinterNameForSubsequentContexts(kId);
#endif

  RunTest("submit_job_promise.html");
}

// Verifies that:
// a) Cancel job request works smoothly.
// b) OnJobStatusChanged() events are dispatched correctly.
IN_PROC_BROWSER_TEST_P(PrintingApiTest, CancelJob) {
  ASSERT_TRUE(StartEmbeddedTestServer());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  AddPrinterWithSemanticCaps(kId, kName, ConstructPrinterCapabilities());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  InSequence s;

  EXPECT_CALL(local_printer(), GetCapability(kId, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          printing::PrinterWithCapabilitiesToMojom(
              chromeos::Printer(kId), *ConstructPrinterCapabilities())));

  std::optional<uint32_t> job_id;
  // Pretends to acknowledge the incoming Lacros print job creation request and
  // responds with PrintJobStatus::kStarted event.
  // The callback is ignored by the implementation -- for this reason the
  // invocation order doesn't really matter here (however, dropping it would
  // yield a mojo error).
  EXPECT_CALL(local_printer(), CreatePrintJob(_, _))
      .WillOnce(DoAll(WithArg<0>([&](const auto& job) {
                        job_id = job->job_id;
                        auto update = crosapi::mojom::PrintJobUpdate::New();
                        update->status =
                            crosapi::mojom::PrintJobStatus::kStarted;
                        observer_remote()->OnPrintJobUpdate(kId, *job_id,
                                                            std::move(update));
                      }),
                      base::test::RunOnceCallback<1>()));

  // Pretends to acknowledge the incoming Lacros print job cancelation request
  // and responds with PrintJobStatus::kCancelled event.
  // The callback is ignored by the implementation -- for this reason the
  // invocation order doesn't really matter here (however, dropping it would
  // yield a mojo error).
  EXPECT_CALL(local_printer(), CancelPrintJob(_, _, _))
      .WillOnce(DoAll(WithoutArgs([&] {
                        // Thanks to InSequence defined in the beginning of the
                        // test, it's guaranteed that `job_id` will be set
                        // before we get here.
                        ASSERT_TRUE(job_id);
                        auto update = crosapi::mojom::PrintJobUpdate::New();
                        update->status =
                            crosapi::mojom::PrintJobStatus::kCancelled;
                        observer_remote()->OnPrintJobUpdate(kId, *job_id,
                                                            std::move(update));
                      }),
                      base::test::RunOnceCallback<2>(/*canceled=*/true)));

  printing_infra_helper()
      .test_printing_context_factory()
      .SetPrinterNameForSubsequentContexts(kId);
#endif

  RunTest("cancel_job.html");
}

INSTANTIATE_TEST_SUITE_P(/**/,
                         PrintingApiTest,
                         testing::Values(ExtensionType::kChromeApp,
                                         ExtensionType::kExtensionMV2,
                                         ExtensionType::kExtensionMV3));

// We only run the promise based tests for MV3 extensions as promise based API
// calls are only exposed to MV3.
INSTANTIATE_TEST_SUITE_P(/**/,
                         PrintingPromiseApiTest,
                         testing::Values(ExtensionType::kExtensionMV3));

}  // namespace extensions
