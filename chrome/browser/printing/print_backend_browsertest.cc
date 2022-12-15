// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/metafile.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/print_settings_conversion.h"
#include "printing/printing_context.h"
#include "printing/printing_context_factory_for_test.h"
#include "printing/test_printing_context.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

constexpr int32_t kTestDocumentCookie = 1;

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
    // Do local setup before calling base class, since the base class enters
    // the main run loop.
    PrintBackend::SetPrintBackendForTesting(test_print_backend_.get());
    PrintingContext::SetPrintingContextFactoryForTest(
        &test_printing_context_factory_);
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    // Call base class teardown before local teardown, to be in opposite order
    // of `SetUp`.
    InProcessBrowserTest::TearDown();
    PrintingContext::SetPrintingContextFactoryForTest(/*factory=*/nullptr);
    PrintBackend::SetPrintBackendForTesting(/*print_backend=*/nullptr);
  }

  // Initialize and load the backend service with some test print drivers.
  void LaunchService() {
    print_backend_service_ = PrintBackendServiceTestImpl::LaunchForTesting(
        remote_, test_print_backend_, /*sandboxed=*/true);
  }

  // Load the test backend with a default printer driver.
  void AddDefaultPrinter() {
    // Only explicitly specify capabilities that we pay attention to in the
    // tests.
    auto default_caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
    default_caps->copies_max = kCopiesMax;
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

  // Common helpers to perform particular stages of printing a document.
  mojom::ResultCode StartPrintingAndWait(const PrintSettings& print_settings) {
    mojom::ResultCode result;

    // Safe to use base::Unretained(this) since waiting locally on the callback
    // forces a shorter lifetime than `this`.
    GetPrintBackendService()->StartPrinting(
        kTestDocumentCookie, u"document name",
        mojom::PrintTargetType::kDirectToDevice, print_settings,
        base::BindOnce(&PrintBackendBrowserTest::CaptureResult,
                       base::Unretained(this), std::ref(result)));
    WaitUntilCallbackReceived();
    return result;
  }

#if BUILDFLAG(IS_WIN)
  absl::optional<mojom::ResultCode> RenderPageAndWait() {
    // Load a sample EMF file for a single page for testing handling.
    Emf metafile;
    if (!LoadMetafileDataFromFile("test1.emf", metafile))
      return absl::nullopt;

    base::MappedReadOnlyRegion region_mapping =
        metafile.GetDataAsSharedMemoryRegion();
    if (!region_mapping.IsValid())
      return absl::nullopt;

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

// TODO(crbug.com/1008222)  Include Windows once XPS print pipeline is enabled.
#if !BUILDFLAG(IS_WIN)
  absl::optional<mojom::ResultCode> RenderDocumentAndWait() {
    // Load a sample PDF file for a single page for testing handling.
    MetafileSkia metafile;
    if (!LoadMetafileDataFromFile("embedded_images.pdf", metafile))
      return absl::nullopt;

    base::MappedReadOnlyRegion region_mapping =
        metafile.GetDataAsSharedMemoryRegion();
    if (!region_mapping.IsValid())
      return absl::nullopt;

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

  void CapturePrintSettings(
      mojom::PrintSettingsResultPtr& capture_print_settings,
      mojom::PrintSettingsResultPtr print_settings) {
    capture_print_settings = std::move(print_settings);
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
        bool skip_system_calls) override {
      auto context =
          std::make_unique<TestPrintingContext>(delegate, skip_system_calls);

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
  LaunchService();
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
  LaunchService();
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

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest,
                       GetPrinterSemanticCapsAndDefaults) {
  LaunchService();
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
  LaunchService();
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

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, FetchCapabilities) {
  LaunchService();
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
  LaunchService();
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

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, UseDefaultSettings) {
  LaunchService();
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  mojom::PrintSettingsResultPtr settings;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->UseDefaultSettings(
      base::BindOnce(&PrintBackendBrowserTest::CapturePrintSettings,
                     base::Unretained(this), std::ref(settings)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(settings->is_settings());
  EXPECT_EQ(settings->get_settings().copies(), kPrintSettingsCopies);
  EXPECT_EQ(settings->get_settings().dpi(), kPrintSettingsDefaultDpi);
}

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, AskUserForSettings) {
  LaunchService();
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  mojom::PrintSettingsResultPtr settings;

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->AskUserForSettings(
      /*parent_window_id=*/8,
      /*max_pages=*/1, /*has_selection=*/false, /*is_scripted=*/false,
      base::BindOnce(&PrintBackendBrowserTest::CapturePrintSettings,
                     base::Unretained(this), std::ref(settings)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(settings->is_settings());
  EXPECT_EQ(settings->get_settings().copies(), kPrintSettingsCopies);
  EXPECT_EQ(settings->get_settings().dpi(), kPrintSettingsDefaultDpi);
}
#endif  // BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, UpdatePrintSettings) {
  LaunchService();
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  mojom::PrintSettingsResultPtr settings;

  PrintSettings print_settings;
  print_settings.set_device_name(kDefaultPrinterName16);
  print_settings.set_dpi(kPrintSettingsOverrideDpi);
  base::Value::Dict job_settings =
      PrintSettingsToJobSettingsDebug(print_settings);
  job_settings.Set(kSettingPrinterType,
                   static_cast<int>(mojom::PrinterType::kLocal));

  // Safe to use base::Unretained(this) since waiting locally on the callback
  // forces a shorter lifetime than `this`.
  GetPrintBackendService()->UpdatePrintSettings(
      std::move(job_settings),
      base::BindOnce(&PrintBackendBrowserTest::CapturePrintSettings,
                     base::Unretained(this), std::ref(settings)));
  WaitUntilCallbackReceived();
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
  job_settings = PrintSettingsToJobSettingsDebug(print_settings);
  job_settings.Set(kSettingPrinterType,
                   static_cast<int>(mojom::PrinterType::kLocal));
  GetPrintBackendService()->UpdatePrintSettings(
      std::move(job_settings),
      base::BindOnce(&PrintBackendBrowserTest::CapturePrintSettings,
                     base::Unretained(this), std::ref(settings)));
  WaitUntilCallbackReceived();
  ASSERT_TRUE(settings->is_result_code());
  EXPECT_EQ(settings->get_result_code(), mojom::ResultCode::kFailed);
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, StartPrintingValidPrinter) {
  LaunchService();
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  PrintSettings print_settings;
  print_settings.set_device_name(kDefaultPrinterName16);

  EXPECT_EQ(StartPrintingAndWait(print_settings), mojom::ResultCode::kSuccess);
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, StartPrintingInvalidPrinter) {
  LaunchService();
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  PrintSettings print_settings;
  print_settings.set_device_name(kInvalidPrinterName16);

  EXPECT_EQ(StartPrintingAndWait(print_settings), mojom::ResultCode::kFailed);
}

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, RenderPrintedPage) {
  LaunchService();
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  PrintSettings print_settings;
  print_settings.set_device_name(kDefaultPrinterName16);

  EXPECT_EQ(StartPrintingAndWait(print_settings), mojom::ResultCode::kSuccess);

  absl::optional<mojom::ResultCode> result = RenderPageAndWait();
  EXPECT_EQ(result, mojom::ResultCode::kSuccess);
}
#endif  // BUILDFLAG(IS_WIN)

// TODO(crbug.com/1008222)  Include Windows for this test once XPS print
// pipeline is enabled.
#if !BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, RenderPrintedDocument) {
  LaunchService();
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  PrintSettings print_settings;
  print_settings.set_device_name(kDefaultPrinterName16);

  EXPECT_EQ(StartPrintingAndWait(print_settings), mojom::ResultCode::kSuccess);

  absl::optional<mojom::ResultCode> result = RenderDocumentAndWait();
  EXPECT_EQ(result, mojom::ResultCode::kSuccess);
}
#endif  // !BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, DocumentDone) {
  LaunchService();
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  PrintSettings print_settings;
  print_settings.set_device_name(kDefaultPrinterName16);

  EXPECT_EQ(StartPrintingAndWait(print_settings), mojom::ResultCode::kSuccess);

  // TODO(crbug.com/1008222)  Include Windows coverage for RenderDocument()
  // path once XPS print pipeline is enabled.
#if BUILDFLAG(IS_WIN)
  absl::optional<mojom::ResultCode> result = RenderPageAndWait();
#else
  absl::optional<mojom::ResultCode> result = RenderDocumentAndWait();
#endif
  EXPECT_EQ(result, mojom::ResultCode::kSuccess);

  EXPECT_EQ(DocumentDoneAndWait(), mojom::ResultCode::kSuccess);
}

IN_PROC_BROWSER_TEST_F(PrintBackendBrowserTest, Cancel) {
  LaunchService();
  AddDefaultPrinter();
  SetPrinterNameForSubsequentContexts(kDefaultPrinterName);

  PrintSettings print_settings;
  print_settings.set_device_name(kDefaultPrinterName16);

  EXPECT_EQ(StartPrintingAndWait(print_settings), mojom::ResultCode::kSuccess);

  CancelAndWait();
}

}  // namespace printing
