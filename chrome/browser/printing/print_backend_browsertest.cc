// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/backend/test_print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/metafile.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/print_settings_conversion.h"
#include "printing/printing_context.h"
#include "printing/printing_context_factory_for_test.h"
#include "printing/printing_features.h"
#include "printing/test_printing_context.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "printing/emf_win.h"
#else
#include "printing/metafile_skia.h"
#endif

namespace printing {

using ::testing::UnorderedElementsAreArray;

namespace {

constexpr char kDefaultPrinterName[] = "default-test-printer";
constexpr char16_t kDefaultPrinterName16[] = u"default-test-printer";
constexpr char kAnotherPrinterName[] = "another-test-printer";
constexpr char kInvalidPrinterName[] = "invalid-test-printer";
constexpr char16_t kInvalidPrinterName16[] = u"invalid-test-printer";
constexpr char kAccessDeniedPrinterName[] = "access-denied-test-printer";

const PrinterBasicInfoOptions kDefaultPrintInfoOptions{{"opt1", "123"},
                                                       {"opt2", "456"}};

const PrinterBasicInfo kDefaultPrinterInfo(
    /*printer_name=*/kDefaultPrinterName,
    /*display_name=*/"default test printer",
    /*printer_description=*/"Default printer for testing.",
    /*printer_status=*/0,
    /*is_default=*/true,
    kDefaultPrintInfoOptions);
const PrinterBasicInfo kAnotherPrinterInfo(
    /*printer_name=*/kAnotherPrinterName,
    /*display_name=*/"another test printer",
    /*printer_description=*/"Another printer for testing.",
    /*printer_status=*/5,
    /*is_default=*/false,
    /*options=*/{});

constexpr int32_t kCopiesMax = 123;
constexpr int kPrintSettingsCopies = 42;
constexpr int kPrintSettingsDefaultDpi = 300;
constexpr int kPrintSettingsOverrideDpi = 150;

const int32_t kTestDocumentCookie = PrintSettings::NewCookie();

bool LoadMetafileDataFromFile(const std::string& file_name,
                              Metafile& metafile) {
  base::FilePath data_file;
  if (!base::PathService::Get(chrome::DIR_TEST_DATA, &data_file))
    return false;

  data_file =
      data_file.Append(FILE_PATH_LITERAL("printing")).AppendASCII(file_name);
  std::string data;
  {
    base::ScopedAllowBlockingForTesting allow_block;
    if (!base::ReadFileToString(data_file, &data))
      return false;
  }
  return metafile.InitFromData(base::as_bytes(base::make_span(data)));
}

}  // namespace

class PrintBackendBrowserTest : public InProcessBrowserTest {
 public:
  PrintBackendBrowserTest() = default;
  ~PrintBackendBrowserTest() override = default;

  void SetUp() override {
    // Tests of the Print Backend service always imply out-of-process.
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kEnableOopPrintDrivers,
        {{features::kEnableOopPrintDriversJobPrint.name, "true"}});

    // Do local setup before calling base class, since the base class enters
    // the main run loop.
    PrintBackend::SetPrintBackendForTesting(test_print_backend_.get());
    PrintingContext::SetPrintingContextFactoryForTest(
        &test_printing_context_factory_);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    LaunchService();
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDown() override {
    // Call base class teardown before local teardown, to be in opposite order
    // of `SetUp`.
    InProcessBrowserTest::TearDown();
    PrintingContext::SetPrintingContextFactoryForTest(/*factory=*/nullptr);
    PrintBackend::SetPrintBackendForTesting(/*print_backend=*/nullptr);
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    PrintBackendServiceManager::ResetForTesting();
  }

  // Load the test backend with a default printer driver.
  void AddDefaultPrinter() {
    // Only explicitly specify capabilities that we pay attention to in the
    // tests.
    auto default_caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
    default_caps->copies_max = kCopiesMax;
    default_caps->default_paper = test::kPaperLetter;
    default_caps->papers.push_back(test::kPaperLetter);
    default_caps->papers.push_back(test::kPaperLegal);
    test_print_backend_->AddValidPrinter(
        kDefaultPrinterName, std::move(default_caps),
        std::make_unique<PrinterBasicInfo>(kDefaultPrinterInfo));
  }

  // Load the test backend with another (non-default) printer.
  void AddAnotherPrinter() {
    test_print_backend_->AddValidPrinter(
        kAnotherPrinterName, std::make_unique<PrinterSemanticCapsAndDefaults>(),
        std::make_unique<PrinterBasicInfo>(kAnotherPrinterInfo));
  }

  void AddAccessDeniedPrinter() {
    test_print_backend_->AddAccessDeniedPrinter(kAccessDeniedPrinterName);
  }

  void SetPrinterNameForSubsequentContexts(const std::string& printer_name) {
    test_printing_context_factory_.SetPrinterNameForSubsequentContexts(
        printer_name);
  }

  // `PrintBackendServiceTestImpl` does a debug check on shutdown that there
  // are no residual persistent printing contexts left in the service.  For
  // tests which are known to break this (either by design, for test simplicity
  // or because a related change is only partly implemented), use this method
  // to notify the service to not DCHECK on such a condition.
  void SkipPersistentContextsCheckOnShutdown() {
    print_backend_service_->SkipPersistentContextsCheckOnShutdown();
  }

  // Common helpers to perform particular stages of printing a document.
  uint32_t EstablishPrintingContextAndWait() {
    constexpr uint32_t kContextId = 7;
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
    constexpr uint32_t kParentWindowId = 8;
#endif

    GetPrintBackendService()->EstablishPrintingContext(kContextId
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
                                                       ,
                                                       kParentWindowId
#endif
    );
    return kContextId;
  }

  mojom::PrintSettingsResultPtr UpdatePrintSettingsAndWait(
      uint32_t context_id,
      const PrintSettings& print_settings) {
    base::Value::Dict job_settings =
        PrintSettingsToJobSettingsDebug(print_settings);
    job_settings.Set(kSettingPrinterType,
                     static_cast<int>(mojom::PrinterType::kLocal));

    // Safe to use base::Unretained(this) since waiting locally on the callback
    // forces a shorter lifetime than `this`.
    mojom::PrintSettingsResultPtr settings;
    GetPrintBackendService()->UpdatePrintSettings(
        context_id, std::move(job_settings),
        base::BindOnce(&PrintBackendBrowserTest::CapturePrintSettings,
                       base::Unretained(this), std::ref(settings)));
    WaitUntilCallbackReceived();
    return settings;
  }

  mojom::ResultCode StartPrintingAndWait(uint32_t context_id,
                                         const PrintSettings& print_settings) {
    UpdatePrintSettingsAndWait(context_id, print_settings);

    // Safe to use base::Unretained(this) since waiting locally on the callback
    // forces a shorter lifetime than `this`.
    mojom::ResultCode result;
    int job_id;
    GetPrintBackendService()->StartPrinting(
        context_id, kTestDocumentCookie, u"document name",
#if !BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
        /*settings=*/std::nullopt,
#endif
        base::BindOnce(&PrintBackendBrowserTest::CaptureStartPrintingResult,
                       base::Unretained(this), std::ref(result),
                       std::ref(job_id)));
    WaitUntilCallbackReceived();
    return result;
  }

#if BUILDFLAG(IS_WIN)
  std::optional<mojom::ResultCode> RenderPageAndWait() {
    // Load a sample EMF file for a single page for testing handling.
    Emf metafile;
    if (!LoadMetafileDataFromFile("test1.emf", metafile))
      return std::nullopt;

    base::MappedReadOnlyRegion region_mapping =
        metafile.GetDataAsSharedMemoryRegion();
    if (!region_mapping.IsValid())
      return std::nullopt;

    // Safe to use base::Unretained(this) since waiting locally on the callback
    // forces a shorter lifetime than `this`.
    mojom::ResultCode result;
    GetPrintBackendService()->RenderPrintedPage(
        kTestDocumentCookie,
        /*page_index=*/0, metafile.GetDataType(),
        std::move(region_mapping.region),
        /*page_size=*/gfx::Size(200, 200),
        /*page_content_rect=*/gfx::Rect(0, 0, 200, 200),
        /*shrink_factor=*/1.0f,
        base::BindOnce(&PrintBackendBrowserTest::CaptureResult,
                       base::Unretained(this), std::ref(result)));
    WaitUntilCallbackReceived();
    return result;
  }
#endif  // BUILDFLAG(IS_WIN)

// TODO(crbug.com/40100562)  Include Windows once XPS print pipeline is enabled.
#if !BUILDFLAG(IS_WIN)
  std::optional<mojom::ResultCode> RenderDocumentAndWait() {
    // Load a sample PDF file for a single page for testing handling.
    MetafileSkia metafile;
    if (!LoadMetafileDataFromFile("embedded_images.pdf", metafile))
      return std::nullopt;

    base::MappedReadOnlyRegion region_mapping =
        metafile.GetDataAsSharedMemoryRegion();
    if (!region_mapping.IsValid())
      return std::nullopt;

    // Safe to use base::Unretained(this) since waiting locally on the callback
    // forces a shorter lifetime than `this`.
    mojom::ResultCode result;
    GetPrintBackendService()->RenderPrintedDocument(
        kTestDocumentCookie, /*page_count=*/1u, metafile.GetDataType(),
        std::move(region_mapping.region),
        base::BindOnce(&PrintBackendBrowserTest::CaptureResult,
                       base::Unretained(this), std::ref(result)));
    WaitUntilCallbackReceived();
    return result;
  }
#endif  // !BUILDFLAG(IS_WIN)

  mojom::ResultCode DocumentDoneAndWait() {
    mojom::ResultCode result;

    // Safe to use base::Unretained(this) since waiting locally on the callback
    // forces a shorter lifetime than `this`.
    GetPrintBackendService()->DocumentDone(
        kTestDocumentCookie,
        base::BindOnce(&PrintBackendBrowserTest::CaptureResult,
                       base::Unretained(this), std::ref(result)));
    WaitUntilCallbackReceived();
    return result;
  }

  void CancelAndWait() {
    // Safe to use base::Unretained(this) since waiting locally on the callback
    // forces a shorter lifetime than `this`.
    GetPrintBackendService()->Cancel(
        kTestDocumentCookie,
        base::BindOnce(&PrintBackendBrowserTest::CheckForQuit,
                       base::Unretained(this)));
    WaitUntilCallbackReceived();
  }

  // Public callbacks used by tests.
  void OnDidEnumeratePrinters(mojom::PrinterListResultPtr& capture_printer_list,
                              mojom::PrinterListResultPtr printer_list) {
    capture_printer_list = std::move(printer_list);
    CheckForQuit();
  }

  void OnDidGetDefaultPrinterName(
      mojom::DefaultPrinterNameResultPtr& capture_printer_name,
      mojom::DefaultPrinterNameResultPtr printer_name) {
    capture_printer_name = std::move(printer_name);
    CheckForQuit();
  }

  void OnDidGetPrinterSemanticCapsAndDefaults(
      mojom::PrinterSemanticCapsAndDefaultsResultPtr& capture_printer_caps,
      mojom::PrinterSemanticCapsAndDefaultsResultPtr printer_caps) {
    capture_printer_caps = std::move(printer_caps);
    CheckForQuit();
  }

  void OnDidFetchCapabilities(
      mojom::PrinterCapsAndInfoResultPtr& capture_caps_and_info,
      mojom::PrinterCapsAndInfoResultPtr caps_and_info) {
    capture_caps_and_info = std::move(caps_and_info);
    CheckForQuit();
  }

#if BUILDFLAG(IS_WIN)
  void OnDidGetPaperPrintableArea(gfx::Rect& capture_printable_area_um,
                                  const gfx::Rect& printable_area_um) {
    capture_printable_area_um = printable_area_um;
    CheckForQuit();
  }
#endif

  void CapturePrintSettings(
      mojom::PrintSettingsResultPtr& capture_print_settings,
      mojom::PrintSettingsResultPtr print_settings) {
    capture_print_settings = std::move(print_settings);
    CheckForQuit();
  }

  void CaptureStartPrintingResult(mojom::ResultCode& capture_result,
                                  int& capture_job_id,
                                  mojom::ResultCode result,
                                  int job_id) {
    capture_result = result;
    capture_job_id = job_id;
    CheckForQuit();
  }

  void CaptureResult(mojom::ResultCode& capture_result,
                     mojom::ResultCode result) {
    capture_result = result;
    CheckForQuit();
  }

  // The following are helper functions for having a wait loop in the test and
  // exit when expected messages are received.  Expect to only have to wait for
  // one message.
  void WaitUntilCallbackReceived() {
    // If callback happened before getting here, then no need to wait.
    if (!received_message_) {
      base::RunLoop run_loop;
      quit_callback_ = run_loop.QuitClosure();
      run_loop.Run();
    }

    // Reset for possible subsequent test.
    received_message_ = false;
  }

  void CheckForQuit() {
    received_message_ = true;
    if (quit_callback_)
      std::move(quit_callback_).Run();
  }

  // Get the print backend service being tested.
  mojom::PrintBackendService* GetPrintBackendService() const {
    return print_backend_service_.get();
  }

 private:
  class PrintBackendPrintingContextFactoryForTest
      : public PrintingContextFactoryForTest {
   public:
    std::unique_ptr<PrintingContext> CreatePrintingContext(
        PrintingContext::Delegate* delegate,
        PrintingContext::ProcessBehavior process_behavior) override {
      auto context =
          std::make_unique<TestPrintingContext>(delegate, process_behavior);

      auto settings = std::make_unique<PrintSettings>();
      settings->set_copies(kPrintSettingsCopies);
      settings->set_dpi(kPrintSettingsDefaultDpi);
      settings->set_device_name(base::ASCIIToUTF16(printer_name_));
      context->SetDeviceSettings(printer_name_, std::move(settings));

      return std::move(context);
    }

    void SetPrinterNameForSubsequentContexts(const std::string& printer_name) {
      printer_name_ = printer_name;
    }

   private:
    std::string printer_name_;
  };

  // Initialize and load the backend service with some test print drivers.
  void LaunchService() {
    print_backend_service_ = PrintBackendServiceTestImpl::LaunchForTesting(
        remote_, test_print_backend_, /*sandboxed=*/true);
  }

  base::test::ScopedFeatureList feature_list_;
  bool received_message_ = false;
  base::OnceClosure quit_callback_;

  mojo::Remote<mojom::PrintBackendService> remote_;
  scoped_refptr<TestPrintBackend> test_print_backend_ =
      base::MakeRefCounted<TestPrintBackend>();
  TestPrintingContextDelegate test_printing_context_delegate_;
  PrintBackendPrintingContextFactoryForTest test_printing_context_factory_;
  std::unique_ptr<PrintBackendServiceTestImpl> print_backend_service_;
};

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, EnumeratePrinters) {
  AddDefaultPrinter();
  AddAnotherPrinter();

  const PrinterList kPrinterListExpected = {kDefaultPrinterInfo,
                                            kAnotherPrinterInfo};

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  mojom::PrinterListResultPtr printer_list;
  GetPrintBackendService()->EnumeratePrinters(
      base::BindOnce(&PrintBackendBrowserTest::OnDidEnumeratePrinters,
                     base::Unretained(this), std::ref(printer_list)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(printer_list->is_printer_list());
  EXPECT_THAT(printer_list->get_printer_list(),
              UnorderedElementsAreArray(kPrinterListExpected));
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, GetDefaultPrinterName) {
  AddDefaultPrinter();

  mojom::DefaultPrinterNameResultPtr default_printer_name;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->GetDefaultPrinterName(
      base::BindOnce(&PrintBackendBrowserTest::OnDidGetDefaultPrinterName,
                     base::Unretained(this), std::ref(default_printer_name)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(default_printer_name->is_default_printer_name());
  EXPECT_EQ(default_printer_name->get_default_printer_name(),
            kDefaultPrinterName);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest,
                       GetPrinterSemanticCapsAndDefaults) {
  AddDefaultPrinter();

  mojom::PrinterSemanticCapsAndDefaultsResultPtr printer_caps;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->GetPrinterSemanticCapsAndDefaults(
      kDefaultPrinterName,
      base::BindOnce(
          &PrintBackendBrowserTest::OnDidGetPrinterSemanticCapsAndDefaults,
          base::Unretained(this), std::ref(printer_caps)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(printer_caps->is_printer_caps());
  EXPECT_EQ(printer_caps->get_printer_caps().copies_max, kCopiesMax);

  // Requesting for an invalid printer should not return capabilities.
  GetPrintBackendService()->GetPrinterSemanticCapsAndDefaults(
      kInvalidPrinterName,
      base::BindOnce(
          &PrintBackendBrowserTest::OnDidGetPrinterSemanticCapsAndDefaults,
          base::Unretained(this), std::ref(printer_caps)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(printer_caps->is_result_code());
  EXPECT_EQ(printer_caps->get_result_code(), mojom::ResultCode::kFailed);
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest,
                       GetPrinterSemanticCapsAndDefaultsAccessDenied) {
  AddAccessDeniedPrinter();

  mojom::PrinterSemanticCapsAndDefaultsResultPtr printer_caps;

  // Requesting for a printer which requires elevated privileges should not
  // return capabilities, and should indicate that access was denied.
  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->GetPrinterSemanticCapsAndDefaults(
      kAccessDeniedPrinterName,
      base::BindOnce(
          &PrintBackendBrowserTest::OnDidGetPrinterSemanticCapsAndDefaults,
          base::Unretained(this), std::ref(printer_caps)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(printer_caps->is_result_code());
  EXPECT_EQ(printer_caps->get_result_code(), mojom::ResultCode::kAccessDenied);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, FetchCapabilities) {
  AddDefaultPrinter();

  mojom::PrinterCapsAndInfoResultPtr caps_and_info;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->FetchCapabilities(
      kDefaultPrinterName,
      base::BindOnce(&PrintBackendBrowserTest::OnDidFetchCapabilities,
                     base::Unretained(this), std::ref(caps_and_info)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(caps_and_info->is_printer_caps_and_info());
  EXPECT_EQ(caps_and_info->get_printer_caps_and_info()->printer_caps.copies_max,
            kCopiesMax);

  // Requesting for an invalid printer should not return capabilities.
  GetPrintBackendService()->FetchCapabilities(
      kInvalidPrinterName,
      base::BindOnce(&PrintBackendBrowserTest::OnDidFetchCapabilities,
                     base::Unretained(this), std::ref(caps_and_info)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(caps_and_info->is_result_code());
  EXPECT_EQ(caps_and_info->get_result_code(), mojom::ResultCode::kFailed);
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, FetchCapabilitiesAccessDenied) {
  AddAccessDeniedPrinter();

  mojom::PrinterCapsAndInfoResultPtr caps_and_info;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->FetchCapabilities(
      kAccessDeniedPrinterName,
      base::BindOnce(&PrintBackendBrowserTest::OnDidFetchCapabilities,
                     base::Unretained(this), std::ref(caps_and_info)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(caps_and_info->is_result_code());
  EXPECT_EQ(caps_and_info->get_result_code(), mojom::ResultCode::kAccessDenied);
}

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, GetPaperPrintableArea) {
  AddDefaultPrinter();

  mojom::PrinterCapsAndInfoResultPtr caps_and_info;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->FetchCapabilities(
      kDefaultPrinterName,
      base::BindOnce(&PrintBackendBrowserTest::OnDidFetchCapabilities,
                     base::Unretained(this), std::ref(caps_and_info)));
  WaitUntilCallbackReceived();

  // Fetching capabiliities only provides the paper printable area for the
  // default paper size.  Find a paper which is not the default, which should
  // have been given an incorrect printable area that matches the paper size.
  ASSERT_TRUE(caps_and_info->is_printer_caps_and_info());
  std::optional<PrinterSemanticCapsAndDefaults::Paper> non_default_paper;
  const PrinterSemanticCapsAndDefaults::Paper& default_paper =
      caps_and_info->get_printer_caps_and_info()->printer_caps.default_paper;
  const PrinterSemanticCapsAndDefaults::Papers& papers =
      caps_and_info->get_printer_caps_and_info()->printer_caps.papers;
  for (const auto& paper : papers) {
    if (paper != default_paper) {
      non_default_paper = paper;
      break;
    }
  }
  ASSERT_TRUE(non_default_paper.has_value());
  EXPECT_EQ(non_default_paper->printable_area_um(),
            gfx::Rect(non_default_paper->size_um()));

  // Request the printable area for this paper size, which should no longer
  // match the physical size but have real printable area values.
  gfx::Rect printable_area_um;
  PrintSettings::RequestedMedia media(
      /*.size_microns =*/non_default_paper->size_um(),
      /*.vendor_id = */ non_default_paper->vendor_id());
  GetPrintBackendService()->GetPaperPrintableArea(
      kDefaultPrinterName, media,
      base::BindOnce(&PrintBackendBrowserTest::OnDidGetPaperPrintableArea,
                     base::Unretained(this), std::ref(printable_area_um)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(!printable_area_um.IsEmpty());
  EXPECT_NE(printable_area_um, non_default_paper->printable_area_um());
}
#endif  // BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, UseDefaultSettings) {
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  // Isolated call has no corresponding cleanup of the printing context.
  SkipPersistentContextsCheckOnShutdown();

  const uint32_t context_id = EstablishPrintingContextAndWait();

  mojom::PrintSettingsResultPtr settings;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->UseDefaultSettings(
      context_id, base::BindOnce(&PrintBackendBrowserTest::CapturePrintSettings,
                                 base::Unretained(this), std::ref(settings)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(settings->is_settings());
  EXPECT_EQ(settings->get_settings().copies(), kPrintSettingsCopies);
  EXPECT_EQ(settings->get_settings().dpi(), kPrintSettingsDefaultDpi);
}

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, AskUserForSettings) {
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  // Isolated call has no corresponding cleanup of the printing context.
  SkipPersistentContextsCheckOnShutdown();

  const uint32_t context_id = EstablishPrintingContextAndWait();

  mojom::PrintSettingsResultPtr settings;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->AskUserForSettings(
      context_id,
      /*max_pages=*/1, /*has_selection=*/false, /*is_scripted=*/false,
      base::BindOnce(&PrintBackendBrowserTest::CapturePrintSettings,
                     base::Unretained(this), std::ref(settings)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(settings->is_settings());
  EXPECT_EQ(settings->get_settings().copies(), kPrintSettingsCopies);
  EXPECT_EQ(settings->get_settings().dpi(), kPrintSettingsDefaultDpi);
}
#endif  // BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, UpdatePrintSettings) {
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  // Isolated call has no corresponding cleanup of the printing context.
  SkipPersistentContextsCheckOnShutdown();

  const uint32_t context_id = EstablishPrintingContextAndWait();

  PrintSettings print_settings;
  print_settings.set_device_name(kDefaultPrinterName16);
  print_settings.set_dpi(kPrintSettingsOverrideDpi);
  print_settings.set_copies(kPrintSettingsCopies);

  mojom::PrintSettingsResultPtr settings =
      UpdatePrintSettingsAndWait(context_id, print_settings);
  ASSERT_TRUE(settings->is_settings());
  EXPECT_EQ(settings->get_settings().copies(), kPrintSettingsCopies);
  EXPECT_EQ(settings->get_settings().dpi(), kPrintSettingsOverrideDpi);
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_CUPS)
  const PrintSettings::AdvancedSettings& advanced_settings =
      settings->get_settings().advanced_settings();
  EXPECT_EQ(advanced_settings.size(), kDefaultPrintInfoOptions.size());
  for (const auto& advanced_setting : advanced_settings) {
    auto option = kDefaultPrintInfoOptions.find(advanced_setting.first);
    ASSERT_NE(option, kDefaultPrintInfoOptions.end());
    EXPECT_EQ(option->second, advanced_setting.second.GetString());
  }
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_CUPS)

  // Updating for an invalid printer should not return print settings.
  print_settings.set_device_name(kInvalidPrinterName16);

  settings = UpdatePrintSettingsAndWait(context_id, print_settings);
  ASSERT_TRUE(settings->is_result_code());
  EXPECT_EQ(settings->get_result_code(), mojom::ResultCode::kFailed);
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, StartPrinting) {
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  const uint32_t context_id = EstablishPrintingContextAndWait();

  PrintSettings print_settings;
  print_settings.set_device_name(kDefaultPrinterName16);
  ASSERT_TRUE(UpdatePrintSettingsAndWait(context_id, print_settings));

  EXPECT_EQ(StartPrintingAndWait(context_id, print_settings),
            mojom::ResultCode::kSuccess);
}

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, RenderPrintedPage) {
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  const uint32_t context_id = EstablishPrintingContextAndWait();

  PrintSettings print_settings;
  print_settings.set_device_name(kDefaultPrinterName16);
  ASSERT_TRUE(UpdatePrintSettingsAndWait(context_id, print_settings));

  ASSERT_EQ(StartPrintingAndWait(context_id, print_settings),
            mojom::ResultCode::kSuccess);

  std::optional<mojom::ResultCode> result = RenderPageAndWait();
  EXPECT_EQ(result, mojom::ResultCode::kSuccess);
}
#endif  // BUILDFLAG(IS_WIN)

// TODO(crbug.com/40100562)  Include Windows for this test once XPS print
// pipeline is enabled.
#if !BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, RenderPrintedDocument) {
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  const uint32_t context_id = EstablishPrintingContextAndWait();

  PrintSettings print_settings;
  print_settings.set_device_name(kDefaultPrinterName16);
  ASSERT_TRUE(UpdatePrintSettingsAndWait(context_id, print_settings));

  ASSERT_EQ(StartPrintingAndWait(context_id, print_settings),
            mojom::ResultCode::kSuccess);

  std::optional<mojom::ResultCode> result = RenderDocumentAndWait();
  EXPECT_EQ(result, mojom::ResultCode::kSuccess);
}
#endif  // !BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, DocumentDone) {
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  const uint32_t context_id = EstablishPrintingContextAndWait();

  PrintSettings print_settings;
  print_settings.set_device_name(kDefaultPrinterName16);
  ASSERT_TRUE(UpdatePrintSettingsAndWait(context_id, print_settings));

  ASSERT_EQ(StartPrintingAndWait(context_id, print_settings),
            mojom::ResultCode::kSuccess);

  // TODO(crbug.com/40100562)  Include Windows coverage for RenderDocument()
  // path once XPS print pipeline is enabled.
#if BUILDFLAG(IS_WIN)
  std::optional<mojom::ResultCode> result = RenderPageAndWait();
#else
  std::optional<mojom::ResultCode> result = RenderDocumentAndWait();
#endif
  EXPECT_EQ(result, mojom::ResultCode::kSuccess);

  EXPECT_EQ(DocumentDoneAndWait(), mojom::ResultCode::kSuccess);
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, Cancel) {
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  const uint32_t context_id = EstablishPrintingContextAndWait();

  PrintSettings print_settings;
  print_settings.set_device_name(kDefaultPrinterName16);
  ASSERT_TRUE(UpdatePrintSettingsAndWait(context_id, print_settings));

  EXPECT_EQ(StartPrintingAndWait(context_id, print_settings),
            mojom::ResultCode::kSuccess);

  CancelAndWait();
}

}  // namespace printing
