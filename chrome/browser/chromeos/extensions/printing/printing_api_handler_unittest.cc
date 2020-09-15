// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/printing_api_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/printing/fake_print_job_controller.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/test_cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/test_cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/test_cups_wrapper.h"
#include "chrome/browser/chromeos/printing/test_printer_configurer.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/api/printing.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/common/extension_builder.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class PrintingEventObserver : public TestEventRouter::EventObserver {
 public:
  // The observer will only listen to events with the |event_name|.
  PrintingEventObserver(TestEventRouter* event_router,
                        const std::string& event_name)
      : event_router_(event_router), event_name_(event_name) {
    event_router_->AddEventObserver(this);
  }

  ~PrintingEventObserver() override {
    event_router_->RemoveEventObserver(this);
  }

  // TestEventRouter::EventObserver:
  void OnDispatchEventToExtension(const std::string& extension_id,
                                  const Event& event) override {
    if (event.event_name == event_name_) {
      extension_id_ = extension_id;
      event_args_ = event.event_args->Clone();
    }
  }

  const std::string& extension_id() const { return extension_id_; }

  const base::Value& event_args() const { return event_args_; }

 private:
  // Event router this class should observe.
  TestEventRouter* const event_router_;

  // The name of the observed event.
  const std::string event_name_;

  // The extension id passed for the last observed event.
  std::string extension_id_;

  // The arguments passed for the last observed event.
  base::Value event_args_;

  DISALLOW_COPY_AND_ASSIGN(PrintingEventObserver);
};

constexpr char kExtensionId[] = "abcdefghijklmnopqrstuvwxyzabcdef";
constexpr char kExtensionId2[] = "abcdefghijklmnopqrstuvwxyzaaaaaa";
constexpr char kPrinterId[] = "printer";
constexpr int kJobId = 10;

constexpr char kId1[] = "id1";
constexpr char kId2[] = "id2";
constexpr char kId3[] = "id3";
constexpr char kName[] = "name";
constexpr char kDescription[] = "description";
constexpr char kUri[] = "ipp://1.2.3.4";

constexpr int kHorizontalDpi = 300;
constexpr int kVerticalDpi = 400;
constexpr int kMediaSizeWidth = 210000;
constexpr int kMediaSizeHeight = 297000;
constexpr char kMediaSizeVendorId[] = "iso_a4_210x297mm";

// CJT stands for Cloud Job Ticket. It should be passed as a print settings
// ticket to chrome.printing.submitJob() method.
constexpr char kCjt[] = R"(
    {
      "version": "1.0",
      "print": {
        "color": {
          "type": "STANDARD_COLOR"
        },
        "duplex": {
          "type": "NO_DUPLEX"
        },
        "page_orientation": {
          "type": "LANDSCAPE"
        },
        "copies": {
          "copies": 5
        },
        "dpi": {
          "horizontal_dpi": 300,
          "vertical_dpi": 400
        },
        "media_size": {
          "width_microns": 210000,
          "height_microns": 297000,
          "vendor_id": "iso_a4_210x297mm"
        },
        "collate": {
          "collate": false
        }
      }
    })";

constexpr char kIncompleteCjt[] = R"(
    {
      "version": "1.0",
      "print": {
        "color": {
          "type": "STANDARD_MONOCHROME"
        },
        "duplex": {
          "type": "NO_DUPLEX"
        },
        "copies": {
          "copies": 5
        },
        "dpi": {
          "horizontal_dpi": 300,
          "vertical_dpi": 400
        }
      }
    })";

constexpr char kPdfExample[] = "%PDF";

std::unique_ptr<api::printing::SubmitJob::Params> ConstructSubmitJobParams(
    const std::string& printer_id,
    const std::string& title,
    const std::string& ticket,
    const std::string& content_type,
    std::unique_ptr<std::string> document_blob_uuid) {
  api::printing::SubmitJobRequest request;
  request.job.printer_id = printer_id;
  request.job.title = title;
  base::Optional<base::Value> ticket_value = base::JSONReader::Read(ticket);
  DCHECK(ticket_value.has_value());
  EXPECT_TRUE(api::printer_provider::PrintJob::Ticket::Populate(
      ticket_value.value(), &request.job.ticket));
  request.job.content_type = content_type;
  request.document_blob_uuid = std::move(document_blob_uuid);

  base::ListValue args;
  args.Set(0, request.ToValue());
  return api::printing::SubmitJob::Params::Create(args);
}

chromeos::Printer ConstructPrinter(const std::string& id,
                                   const std::string& name,
                                   const std::string& description,
                                   const std::string& uri,
                                   chromeos::Printer::Source source) {
  chromeos::Printer printer(id);
  printer.set_display_name(name);
  printer.set_description(description);
  EXPECT_TRUE(printer.SetUri(uri));
  printer.set_source(source);
  return printer;
}

std::unique_ptr<printing::PrinterSemanticCapsAndDefaults>
ConstructPrinterCapabilities() {
  auto capabilities =
      std::make_unique<printing::PrinterSemanticCapsAndDefaults>();
  capabilities->color_model = printing::mojom::ColorModel::kColor;
  capabilities->duplex_modes.push_back(printing::mojom::DuplexMode::kSimplex);
  capabilities->copies_max = 5;
  capabilities->dpis.push_back(gfx::Size(kHorizontalDpi, kVerticalDpi));
  printing::PrinterSemanticCapsAndDefaults::Paper paper;
  paper.vendor_id = kMediaSizeVendorId;
  paper.size_um = gfx::Size(kMediaSizeWidth, kMediaSizeHeight);
  capabilities->papers.push_back(paper);
  capabilities->collate_capable = true;
  return capabilities;
}

std::unique_ptr<content::BlobHandle> CreateMemoryBackedBlob(
    content::BrowserContext* browser_context,
    const std::string& content,
    const std::string& content_type) {
  std::unique_ptr<content::BlobHandle> result;
  base::RunLoop run_loop;
  content::BrowserContext::CreateMemoryBackedBlob(
      browser_context, base::as_bytes(base::make_span(content)), content_type,
      base::BindOnce(
          [](std::unique_ptr<content::BlobHandle>* out_blob,
             base::OnceClosure closure,
             std::unique_ptr<content::BlobHandle> blob) {
            *out_blob = std::move(blob);
            std::move(closure).Run();
          },
          &result, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(result);
  return result;
}

}  // namespace

class PrintingAPIHandlerUnittest : public testing::Test {
 public:
  PrintingAPIHandlerUnittest()
      : disable_pdf_flattening_reset_(
            PrintJobSubmitter::DisablePdfFlatteningForTesting()) {}
  ~PrintingAPIHandlerUnittest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ =
        profile_manager_->CreateTestingProfile(chrome::kInitialProfile);

    base::Value extensions_list(base::Value::Type::LIST);
    extensions_list.Append(base::Value(kExtensionId));
    testing_profile_->GetTestingPrefService()->Set(
        prefs::kPrintingAPIExtensionsAllowlist, std::move(extensions_list));

    const char kExtensionName[] = "Printing extension";
    const char kPermissionName[] = "printing";
    extension_ = ExtensionBuilder(kExtensionName)
                     .SetID(kExtensionId)
                     .AddPermission(kPermissionName)
                     .Build();
    ExtensionRegistry::Get(testing_profile_)->AddEnabled(extension_);

    print_job_manager_ =
        std::make_unique<chromeos::TestCupsPrintJobManager>(testing_profile_);
    printers_manager_ = std::make_unique<chromeos::TestCupsPrintersManager>();
    auto print_job_controller = std::make_unique<FakePrintJobController>(
        print_job_manager_.get(), printers_manager_.get());
    print_job_controller_ = print_job_controller.get();
    auto cups_wrapper = std::make_unique<chromeos::TestCupsWrapper>();
    cups_wrapper_ = cups_wrapper.get();
    test_backend_ = base::MakeRefCounted<printing::TestPrintBackend>();
    printing::PrintBackend::SetPrintBackendForTesting(test_backend_.get());
    event_router_ = CreateAndUseTestEventRouter(testing_profile_);

    printing_api_handler_ = PrintingAPIHandler::CreateForTesting(
        testing_profile_, event_router_,
        ExtensionRegistry::Get(testing_profile_), print_job_manager_.get(),
        printers_manager_.get(), std::move(print_job_controller),
        std::make_unique<chromeos::TestPrinterConfigurer>(),
        std::move(cups_wrapper));
  }

  void TearDown() override {
    printing_api_handler_.reset();

    test_backend_.reset();
    printers_manager_.reset();
    print_job_manager_.reset();

    testing_profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
  }

  void AddUnavailablePrinter(const std::string& printer_id) {
    chromeos::Printer printer = chromeos::Printer(printer_id);
    printers_manager_->AddPrinter(printer, chromeos::PrinterClass::kEnterprise);
  }

  void AddAvailablePrinter(
      const std::string& printer_id,
      std::unique_ptr<printing::PrinterSemanticCapsAndDefaults> capabilities) {
    AddUnavailablePrinter(printer_id);

    // Add printer capabilities to |test_backend_|.
    test_backend_->AddValidPrinter(printer_id, std::move(capabilities));
  }

  void OnJobSubmitted(base::RepeatingClosure run_loop_closure,
                      base::Optional<api::printing::SubmitJobStatus> status,
                      std::unique_ptr<std::string> job_id,
                      base::Optional<std::string> error) {
    submit_job_status_ = status;
    job_id_ = std::move(job_id);
    error_ = error;
    run_loop_closure.Run();
  }

  void OnPrinterInfoRetrieved(
      base::RepeatingClosure run_loop_closure,
      base::Optional<base::Value> capabilities,
      base::Optional<api::printing::PrinterStatus> printer_status,
      base::Optional<std::string> error) {
    if (capabilities)
      capabilities_ = capabilities.value().Clone();
    else
      capabilities_ = base::nullopt;
    printer_status_ = printer_status;
    error_ = error;
    run_loop_closure.Run();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile* testing_profile_;
  scoped_refptr<printing::TestPrintBackend> test_backend_;
  TestEventRouter* event_router_ = nullptr;
  std::unique_ptr<chromeos::TestCupsPrintJobManager> print_job_manager_;
  std::unique_ptr<chromeos::TestCupsPrintersManager> printers_manager_;
  FakePrintJobController* print_job_controller_;
  chromeos::TestCupsWrapper* cups_wrapper_;
  std::unique_ptr<PrintingAPIHandler> printing_api_handler_;
  scoped_refptr<const Extension> extension_;
  base::Optional<api::printing::SubmitJobStatus> submit_job_status_;
  std::unique_ptr<std::string> job_id_;
  base::Optional<base::Value> capabilities_;
  base::Optional<api::printing::PrinterStatus> printer_status_;
  base::Optional<std::string> error_;

 private:
  // Resets |disable_pdf_flattening_for_testing| back to false automatically
  // after the test is over.
  base::AutoReset<bool> disable_pdf_flattening_reset_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

  DISALLOW_COPY_AND_ASSIGN(PrintingAPIHandlerUnittest);
};

// Test that |OnJobStatusChanged| is dispatched when the print job status is
// changed.
TEST_F(PrintingAPIHandlerUnittest, EventIsDispatched) {
  PrintingEventObserver event_observer(
      event_router_, api::printing::OnJobStatusChanged::kEventName);

  std::unique_ptr<chromeos::CupsPrintJob> print_job =
      std::make_unique<chromeos::CupsPrintJob>(
          chromeos::Printer(kPrinterId), kJobId, "title",
          /*total_page_number=*/3, ::printing::PrintJob::Source::EXTENSION,
          kExtensionId, chromeos::printing::proto::PrintSettings());
  print_job_manager_->CreatePrintJob(print_job.get());

  EXPECT_EQ(kExtensionId, event_observer.extension_id());
  const base::Value& event_args = event_observer.event_args();
  ASSERT_TRUE(event_args.is_list());
  ASSERT_EQ(2u, event_args.GetList().size());
  base::Value job_id = event_args.GetList()[0].Clone();
  ASSERT_TRUE(job_id.is_string());
  EXPECT_EQ(chromeos::CupsPrintJob::CreateUniqueId(kPrinterId, kJobId),
            job_id.GetString());
  base::Value job_status = event_args.GetList()[1].Clone();
  ASSERT_TRUE(job_status.is_string());
  EXPECT_EQ(api::printing::JOB_STATUS_PENDING,
            api::printing::ParseJobStatus(job_status.GetString()));
}

// Test that |OnJobStatusChanged| is not dispatched if the print job was created
// on Print Preview page.
TEST_F(PrintingAPIHandlerUnittest, PrintPreviewEventIsNotDispatched) {
  PrintingEventObserver event_observer(
      event_router_, api::printing::OnJobStatusChanged::kEventName);

  std::unique_ptr<chromeos::CupsPrintJob> print_job =
      std::make_unique<chromeos::CupsPrintJob>(
          chromeos::Printer(kPrinterId), kJobId, "title",
          /*total_page_number=*/3, ::printing::PrintJob::Source::PRINT_PREVIEW,
          /*source_id=*/"", chromeos::printing::proto::PrintSettings());
  print_job_manager_->CreatePrintJob(print_job.get());

  // Check that the print job created on Print Preview doesn't show up.
  EXPECT_EQ("", event_observer.extension_id());
  EXPECT_TRUE(event_observer.event_args().is_none());
}

// Test that calling GetPrinters() returns no printers before any are added to
// the profile.
TEST_F(PrintingAPIHandlerUnittest, GetPrinters_NoPrinters) {
  std::vector<api::printing::Printer> printers =
      printing_api_handler_->GetPrinters();
  EXPECT_TRUE(printers.empty());
}

// Test that calling GetPrinters() returns the mock printer.
TEST_F(PrintingAPIHandlerUnittest, GetPrinters_OnePrinter) {
  chromeos::Printer printer = ConstructPrinter(kId1, kName, kDescription, kUri,
                                               chromeos::Printer::SRC_POLICY);
  printers_manager_->AddPrinter(printer, chromeos::PrinterClass::kEnterprise);

  std::vector<api::printing::Printer> printers =
      printing_api_handler_->GetPrinters();

  ASSERT_EQ(1u, printers.size());
  const api::printing::Printer& idl_printer = printers[0];

  EXPECT_EQ(kId1, idl_printer.id);
  EXPECT_EQ(kName, idl_printer.name);
  EXPECT_EQ(kDescription, idl_printer.description);
  EXPECT_EQ(kUri, idl_printer.uri);
  EXPECT_EQ(api::printing::PRINTER_SOURCE_POLICY, idl_printer.source);
  EXPECT_FALSE(idl_printer.is_default);
  EXPECT_EQ(nullptr, idl_printer.recently_used_rank);
}

// Test that calling GetPrinters() returns printers of all classes.
TEST_F(PrintingAPIHandlerUnittest, GetPrinters_ThreePrinters) {
  chromeos::Printer printer1 = chromeos::Printer(kId1);
  chromeos::Printer printer2 = chromeos::Printer(kId2);
  chromeos::Printer printer3 = chromeos::Printer(kId3);
  printers_manager_->AddPrinter(printer1, chromeos::PrinterClass::kEnterprise);
  printers_manager_->AddPrinter(printer2, chromeos::PrinterClass::kSaved);
  printers_manager_->AddPrinter(printer3, chromeos::PrinterClass::kAutomatic);

  std::vector<api::printing::Printer> printers =
      printing_api_handler_->GetPrinters();

  ASSERT_EQ(3u, printers.size());
  std::vector<std::string> printer_ids;
  for (const auto& printer : printers)
    printer_ids.push_back(printer.id);
  EXPECT_THAT(printer_ids, testing::UnorderedElementsAre(kId1, kId2, kId3));
}

// Test that calling GetPrinters() returns printers with correct |is_default|
// flag.
TEST_F(PrintingAPIHandlerUnittest, GetPrinters_IsDefault) {
  testing_profile_->GetPrefs()->SetString(
      prefs::kPrintPreviewDefaultDestinationSelectionRules,
      R"({"kind": "local", "idPattern": "id.*"})");
  chromeos::Printer printer = ConstructPrinter(kId1, kName, kDescription, kUri,
                                               chromeos::Printer::SRC_POLICY);
  printers_manager_->AddPrinter(printer, chromeos::PrinterClass::kEnterprise);

  std::vector<api::printing::Printer> printers =
      printing_api_handler_->GetPrinters();

  ASSERT_EQ(1u, printers.size());
  api::printing::Printer idl_printer = std::move(printers[0]);

  EXPECT_EQ(kId1, idl_printer.id);
  EXPECT_TRUE(idl_printer.is_default);
}

// Test that calling GetPrinters() returns printers with correct
// |recently_used_rank| flag.
TEST_F(PrintingAPIHandlerUnittest, GetPrinters_RecentlyUsedRank) {
  printing::PrintPreviewStickySettings* sticky_settings =
      printing::PrintPreviewStickySettings::GetInstance();
  sticky_settings->StoreAppState(R"({
    "version": 2,
    "recentDestinations": [
      {
        "id": "id3"
      },
      {
        "id": "id1"
      }
    ]
  })");
  sticky_settings->SaveInPrefs(testing_profile_->GetPrefs());

  chromeos::Printer printer = ConstructPrinter(kId1, kName, kDescription, kUri,
                                               chromeos::Printer::SRC_POLICY);
  printers_manager_->AddPrinter(printer, chromeos::PrinterClass::kEnterprise);

  std::vector<api::printing::Printer> printers =
      printing_api_handler_->GetPrinters();

  ASSERT_EQ(1u, printers.size());
  api::printing::Printer idl_printer = std::move(printers[0]);

  EXPECT_EQ(kId1, idl_printer.id);
  ASSERT_TRUE(idl_printer.recently_used_rank);
  // The "id1" printer is listed as second printer in the recently used printers
  // list, so we expect 1 as its rank.
  EXPECT_EQ(1, *idl_printer.recently_used_rank);
}

TEST_F(PrintingAPIHandlerUnittest, GetPrinterInfo_InvalidId) {
  base::RunLoop run_loop;
  printing_api_handler_->GetPrinterInfo(
      kPrinterId,
      base::BindOnce(&PrintingAPIHandlerUnittest::OnPrinterInfoRetrieved,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // Printer is not added to CupsPrintersManager, so we expect "Invalid printer
  // id" error.
  EXPECT_FALSE(capabilities_.has_value());
  EXPECT_FALSE(printer_status_.has_value());
  ASSERT_TRUE(error_.has_value());
  EXPECT_EQ("Invalid printer ID", error_.value());
}

TEST_F(PrintingAPIHandlerUnittest, GetPrinterInfo_NoCapabilities) {
  AddUnavailablePrinter(kPrinterId);
  printers_manager_->InstallPrinter(kPrinterId);

  base::RunLoop run_loop;
  printing_api_handler_->GetPrinterInfo(
      kPrinterId,
      base::BindOnce(&PrintingAPIHandlerUnittest::OnPrinterInfoRetrieved,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(capabilities_.has_value());
  ASSERT_TRUE(printer_status_.has_value());
  EXPECT_EQ(api::printing::PRINTER_STATUS_UNREACHABLE, printer_status_.value());
  EXPECT_FALSE(error_.has_value());
}

TEST_F(PrintingAPIHandlerUnittest, GetPrinterInfo) {
  AddAvailablePrinter(
      kPrinterId, std::make_unique<printing::PrinterSemanticCapsAndDefaults>());

  // Mock CUPS wrapper to return predefined status for given printer.
  printing::PrinterStatus::PrinterReason reason;
  reason.reason = printing::PrinterStatus::PrinterReason::Reason::kMediaEmpty;
  cups_wrapper_->SetPrinterStatus(kPrinterId, reason);

  base::RunLoop run_loop;
  printing_api_handler_->GetPrinterInfo(
      kPrinterId,
      base::BindOnce(&PrintingAPIHandlerUnittest::OnPrinterInfoRetrieved,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(capabilities_.has_value());
  const base::Value* capabilities_value = capabilities_->FindDictKey("printer");
  ASSERT_TRUE(capabilities_value);

  const base::Value* color = capabilities_value->FindDictKey("color");
  ASSERT_TRUE(color);
  const base::Value* color_options = color->FindListKey("option");
  ASSERT_TRUE(color_options);
  ASSERT_EQ(1u, color_options->GetList().size());
  const std::string* color_type =
      color_options->GetList()[0].FindStringKey("type");
  ASSERT_TRUE(color_type);
  EXPECT_EQ("STANDARD_MONOCHROME", *color_type);

  const base::Value* page_orientation =
      capabilities_value->FindDictKey("page_orientation");
  ASSERT_TRUE(page_orientation);
  const base::Value* page_orientation_options =
      page_orientation->FindListKey("option");
  ASSERT_TRUE(page_orientation_options);
  ASSERT_EQ(3u, page_orientation_options->GetList().size());
  std::vector<std::string> page_orientation_types;
  for (const base::Value& page_orientation_option :
       page_orientation_options->GetList()) {
    const std::string* page_orientation_type =
        page_orientation_option.FindStringKey("type");
    ASSERT_TRUE(page_orientation_type);
    page_orientation_types.push_back(*page_orientation_type);
  }
  EXPECT_THAT(page_orientation_types,
              testing::UnorderedElementsAre("PORTRAIT", "LANDSCAPE", "AUTO"));

  ASSERT_TRUE(printer_status_.has_value());
  EXPECT_EQ(api::printing::PRINTER_STATUS_OUT_OF_PAPER,
            printer_status_.value());
  EXPECT_FALSE(error_.has_value());
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_UnsupportedContentType) {
  AddAvailablePrinter(kPrinterId, ConstructPrinterCapabilities());

  auto params =
      ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt, "image/jpeg",
                               /*document_blob_uuid=*/nullptr);
  ASSERT_TRUE(params);

  base::RunLoop run_loop;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      base::BindOnce(&PrintingAPIHandlerUnittest::OnJobSubmitted,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // According to the documentation only "application/pdf" content type is
  // supported, so we expect an error as a result of API call.
  ASSERT_TRUE(error_.has_value());
  EXPECT_EQ("Unsupported content type", error_.value());
  EXPECT_FALSE(submit_job_status_.has_value());
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_InvalidPrintTicket) {
  AddAvailablePrinter(kPrinterId, ConstructPrinterCapabilities());

  auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"",
                                         kIncompleteCjt, "application/pdf",
                                         /*document_blob_uuid=*/nullptr);
  ASSERT_TRUE(params);

  base::RunLoop run_loop;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      base::BindOnce(&PrintingAPIHandlerUnittest::OnJobSubmitted,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // Some fields of the print ticket are missing, so we expect an error as a
  // result of API call.
  ASSERT_TRUE(error_.has_value());
  EXPECT_EQ("Invalid ticket", error_.value());
  EXPECT_FALSE(submit_job_status_.has_value());
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_InvalidPrinterId) {
  auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt,
                                         "application/pdf",
                                         /*document_blob_uuid=*/nullptr);
  ASSERT_TRUE(params);

  base::RunLoop run_loop;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      base::BindOnce(&PrintingAPIHandlerUnittest::OnJobSubmitted,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // The printer is not added, so we expect an error as a result of API call.
  ASSERT_TRUE(error_.has_value());
  EXPECT_EQ("Invalid printer ID", error_.value());
  EXPECT_FALSE(submit_job_status_.has_value());
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_PrinterUnavailable) {
  AddUnavailablePrinter(kPrinterId);

  auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt,
                                         "application/pdf",
                                         /*document_blob_uuid=*/nullptr);
  ASSERT_TRUE(params);

  base::RunLoop run_loop;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      base::BindOnce(&PrintingAPIHandlerUnittest::OnJobSubmitted,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // Even though the printer is added, it's not able to accept jobs until it's
  // added as valid printer, so we expect an error as a result of API call.
  ASSERT_TRUE(error_.has_value());
  EXPECT_EQ("Printer is unavailable at the moment", error_.value());
  EXPECT_FALSE(submit_job_status_.has_value());
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_UnsupportedTicket) {
  AddAvailablePrinter(
      kPrinterId, std::make_unique<printing::PrinterSemanticCapsAndDefaults>());

  auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt,
                                         "application/pdf",
                                         /*document_blob_uuid=*/nullptr);
  ASSERT_TRUE(params);

  base::RunLoop run_loop;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      base::BindOnce(&PrintingAPIHandlerUnittest::OnJobSubmitted,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // Print ticket requires some non-default parameters as DPI and media size
  // which are not supported for default capabilities, so we expect an error as
  // a result of API call.
  ASSERT_TRUE(error_.has_value());
  EXPECT_EQ("Ticket is unsupported on the given printer", error_.value());
  EXPECT_FALSE(submit_job_status_.has_value());
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_InvalidData) {
  AddAvailablePrinter(kPrinterId, ConstructPrinterCapabilities());

  auto params = ConstructSubmitJobParams(
      kPrinterId, /*title=*/"", kCjt, "application/pdf",
      std::make_unique<std::string>("invalid_uuid"));
  ASSERT_TRUE(params);

  base::RunLoop run_loop;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      base::BindOnce(&PrintingAPIHandlerUnittest::OnJobSubmitted,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // We can't fetch actual document data without Blob UUID, so we expect an
  // error as a result of API call.
  ASSERT_TRUE(error_.has_value());
  EXPECT_EQ("Invalid document", error_.value());
  EXPECT_FALSE(submit_job_status_.has_value());
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob) {
  AddAvailablePrinter(kPrinterId, ConstructPrinterCapabilities());

  // Create Blob with given data.
  std::unique_ptr<content::BlobHandle> blob = CreateMemoryBackedBlob(
      testing_profile_, kPdfExample, /*content_type=*/"");
  auto params = ConstructSubmitJobParams(
      kPrinterId, /*title=*/"", kCjt, "application/pdf",
      std::make_unique<std::string>(blob->GetUUID()));
  ASSERT_TRUE(params);

  base::RunLoop run_loop;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      base::BindOnce(&PrintingAPIHandlerUnittest::OnJobSubmitted,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(error_.has_value());
  ASSERT_TRUE(submit_job_status_.has_value());
  EXPECT_EQ(api::printing::SUBMIT_JOB_STATUS_OK, submit_job_status_.value());
}

TEST_F(PrintingAPIHandlerUnittest, CancelJob_InvalidId) {
  base::Optional<std::string> error =
      printing_api_handler_->CancelJob(kExtensionId, "job_id");

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("No active print job with given ID", error.value());
}

TEST_F(PrintingAPIHandlerUnittest, CancelJob_InvalidId_OtherExtension) {
  std::unique_ptr<chromeos::CupsPrintJob> print_job =
      std::make_unique<chromeos::CupsPrintJob>(
          chromeos::Printer(kPrinterId), kJobId, "title",
          /*total_page_number=*/3, ::printing::PrintJob::Source::EXTENSION,
          kExtensionId, chromeos::printing::proto::PrintSettings());
  print_job_manager_->CreatePrintJob(print_job.get());

  // Try to cancel print job from other extension.
  base::Optional<std::string> error = printing_api_handler_->CancelJob(
      kExtensionId2,
      chromeos::CupsPrintJob::CreateUniqueId(kPrinterId, kJobId));

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("No active print job with given ID", error.value());
}

TEST_F(PrintingAPIHandlerUnittest, CancelJob_InvalidState) {
  AddAvailablePrinter(kPrinterId, ConstructPrinterCapabilities());

  // Create Blob with given data.
  std::unique_ptr<content::BlobHandle> blob = CreateMemoryBackedBlob(
      testing_profile_, kPdfExample, /*content_type=*/"");
  auto params = ConstructSubmitJobParams(
      kPrinterId, /*title=*/"", kCjt, "application/pdf",
      std::make_unique<std::string>(blob->GetUUID()));
  ASSERT_TRUE(params);

  base::RunLoop run_loop;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      base::BindOnce(&PrintingAPIHandlerUnittest::OnJobSubmitted,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // Explicitly complete started print job.
  print_job_manager_->CompletePrintJob(
      print_job_controller_->GetCupsPrintJob(*job_id_));

  // Try to cancel already completed print job.
  base::Optional<std::string> error =
      printing_api_handler_->CancelJob(kExtensionId, *job_id_);

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("No active print job with given ID", error.value());
}

TEST_F(PrintingAPIHandlerUnittest, CancelJob) {
  AddAvailablePrinter(kPrinterId, ConstructPrinterCapabilities());

  // Create Blob with given data.
  std::unique_ptr<content::BlobHandle> blob = CreateMemoryBackedBlob(
      testing_profile_, kPdfExample, /*content_type=*/"");
  auto params = ConstructSubmitJobParams(
      kPrinterId, /*title=*/"", kCjt, "application/pdf",
      std::make_unique<std::string>(blob->GetUUID()));
  ASSERT_TRUE(params);

  base::RunLoop run_loop;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      base::BindOnce(&PrintingAPIHandlerUnittest::OnJobSubmitted,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  // Cancel started print job.
  base::Optional<std::string> error =
      printing_api_handler_->CancelJob(kExtensionId, *job_id_);

  EXPECT_FALSE(error.has_value());
}

}  // namespace extensions
