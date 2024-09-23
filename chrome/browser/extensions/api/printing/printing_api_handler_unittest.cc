// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/printing/printing_api_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/printing/test_cups_wrapper.h"
#include "chrome/browser/extensions/api/printing/fake_print_job_controller.h"
#include "chrome/browser/extensions/api/printing/print_job_submitter.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/api/printing.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/chromeos/printing/fake_local_printer_chromeos.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class PrintingEventObserver : public TestEventRouter::EventObserver {
 public:
  // The observer will only listen to events with the `event_name`.
  PrintingEventObserver(TestEventRouter* event_router,
                        const std::string& event_name)
      : event_router_(event_router), event_name_(event_name) {
    event_router_->AddEventObserver(this);
  }

  PrintingEventObserver(const PrintingEventObserver&) = delete;
  PrintingEventObserver& operator=(const PrintingEventObserver&) = delete;

  void CheckJobStatusEvent(const std::string& expected_extension_id,
                           const std::string& expected_job_id,
                           api::printing::JobStatus expected_status) const {
    EXPECT_EQ(expected_extension_id, extension_id_);
    ASSERT_TRUE(event_args_.is_list());
    ASSERT_EQ(2u, event_args_.GetList().size());
    const base::Value& job_id = event_args_.GetList()[0];
    ASSERT_TRUE(job_id.is_string());
    EXPECT_EQ(expected_job_id, job_id.GetString());
    const base::Value& job_status = event_args_.GetList()[1];
    ASSERT_TRUE(job_status.is_string());
    EXPECT_EQ(expected_status,
              api::printing::ParseJobStatus(job_status.GetString()));
  }

  ~PrintingEventObserver() override {
    event_router_->RemoveEventObserver(this);
  }

  // TestEventRouter::EventObserver:
  void OnDispatchEventToExtension(const std::string& extension_id,
                                  const Event& event) override {
    if (event.event_name == event_name_) {
      extension_id_ = extension_id;
      event_args_ = base::Value(event.event_args.Clone());
    }
  }

  const ExtensionId& extension_id() const { return extension_id_; }

  const base::Value& event_args() const { return event_args_; }

 private:
  // Event router this class should observe.
  const raw_ptr<TestEventRouter> event_router_;

  // The name of the observed event.
  const std::string event_name_;

  // The extension id passed for the last observed event.
  ExtensionId extension_id_;

  // The arguments passed for the last observed event.
  base::Value event_args_;
};

constexpr char kExtensionId[] = "abcdefghijklmnopqrstuvwxyzabcdef";
constexpr char kExtensionId2[] = "abcdefghijklmnopqrstuvwxyzaaaaaa";
constexpr char kPrinterId[] = "printer";

constexpr char kId1[] = "id1";
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

constexpr char kPdfExample[] =
    "%PDF- This is a string starting with a PDF's magic bytes and long enough "
    "to be seen as a PDF by LooksLikePdf.";

constexpr char kPngExample[] =
    "\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x02\x00\x00\x00\x02\x08"
    "\x02\x00\x00\x00\xfd\xd4\x9as\x00\x00\x00\x16IDAT\x08\xd7"
    "c\xfc\xcf\xc0"
    "\xc0\xc0\xc0\xc0\xc4\xc0\xc0\xc0\xc0\xc0\x00\x00\r\x1d\x01\x03+\xe9\xa6"
    "\xc8\x00\x00\x00\x00IEND\xae"
    "B`\x82";
constexpr size_t kPngExampleSize = 79;

std::optional<api::printing::SubmitJob::Params> ConstructSubmitJobParams(
    const std::string& printer_id,
    const std::string& title,
    const std::string& ticket,
    const std::string& content_type,
    std::optional<std::string> document_blob_uuid) {
  api::printing::SubmitJobRequest request;
  request.job.printer_id = printer_id;
  request.job.title = title;
  if (auto result = api::printer_provider::PrintJob::Ticket::FromValue(
          base::test::ParseJsonDict(ticket))) {
    request.job.ticket = std::move(result).value();
  } else {
    ADD_FAILURE() << "Failed to parse ticket \"" << ticket << "\".";
  }
  request.job.content_type = content_type;
  request.document_blob_uuid = std::move(document_blob_uuid);

  base::Value::List args;
  args.Append(base::Value(request.ToValue()));
  return api::printing::SubmitJob::Params::Create(args);
}

std::optional<printing::PrinterSemanticCapsAndDefaults>
ConstructPrinterCapabilities() {
  printing::PrinterSemanticCapsAndDefaults capabilities;
  capabilities.color_model = printing::mojom::ColorModel::kColor;
  capabilities.duplex_modes.push_back(printing::mojom::DuplexMode::kSimplex);
  capabilities.copies_max = 5;
  capabilities.dpis.emplace_back(kHorizontalDpi, kVerticalDpi);
  printing::PrinterSemanticCapsAndDefaults::Paper paper(
      /*display_name=*/"", kMediaSizeVendorId,
      {kMediaSizeWidth, kMediaSizeHeight});
  capabilities.papers.push_back(std::move(paper));
  capabilities.collate_capable = true;
  return capabilities;
}

std::unique_ptr<content::BlobHandle> CreateMemoryBackedBlob(
    content::BrowserContext* browser_context,
    const std::string& content,
    const std::string& content_type) {
  base::test::TestFuture<std::unique_ptr<content::BlobHandle>> blob_future;
  browser_context->CreateMemoryBackedBlob(
      base::as_bytes(base::make_span(content)), content_type,
      blob_future.GetCallback());
  return blob_future.Take();
}

using GetPrintersFuture =
    base::test::TestFuture<std::vector<api::printing::Printer>>;
using GetPrinterInfoFuture =
    base::test::TestFuture<std::optional<base::Value>,
                           std::optional<api::printing::PrinterStatus>,
                           std::optional<std::string>>;
using SubmitJobFuture =
    base::test::TestFuture<std::optional<api::printing::SubmitJobStatus>,
                           std::optional<std::string>,
                           std::optional<std::string>>;

}  // namespace

class TestLocalPrinter : public FakeLocalPrinter {
 public:
  struct JobInfo {
    std::string printer_id;
    unsigned int job_id;
  };

  TestLocalPrinter() = default;
  TestLocalPrinter(TestLocalPrinter&) = delete;
  TestLocalPrinter& operator=(const TestLocalPrinter&) = delete;
  ~TestLocalPrinter() override { EXPECT_TRUE(print_jobs_.empty()); }

  std::vector<JobInfo> jobs_cancelled() { return jobs_cancelled_; }

  std::vector<crosapi::mojom::PrintJobPtr> TakePrintJobs() {
    std::vector<crosapi::mojom::PrintJobPtr> print_jobs;
    std::swap(print_jobs, print_jobs_);
    return print_jobs;
  }

  void AddPrinter(crosapi::mojom::LocalDestinationInfoPtr printer) {
    printers_.push_back(std::move(printer));
  }

  void SetCaps(const std::string& id,
               crosapi::mojom::CapabilitiesResponsePtr caps) {
    DCHECK(caps);
    caps_map_[id] = std::move(caps);
  }

  void CreatePrintJob(crosapi::mojom::PrintJobPtr job,
                      CreatePrintJobCallback cb) override {
    print_jobs_.push_back(std::move(job));
    std::move(cb).Run();
  }

  void GetPrinters(GetPrintersCallback cb) override {
    std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers;
    std::swap(printers, printers_);
    std::move(cb).Run(std::move(printers));
  }

  void GetCapability(const std::string& id, GetCapabilityCallback cb) override {
    auto it = caps_map_.find(id);
    if (it == caps_map_.end()) {
      std::move(cb).Run(nullptr);
      return;
    }
    std::move(cb).Run(std::move(it->second));
    caps_map_.erase(it);
  }

  void CancelPrintJob(const std::string& printer_id,
                      unsigned int job_id,
                      CancelPrintJobCallback cb) override {
    jobs_cancelled_.push_back(JobInfo{printer_id, job_id});
    std::move(cb).Run(true);
  }

 private:
  std::vector<crosapi::mojom::PrintJobPtr> print_jobs_;
  std::vector<JobInfo> jobs_cancelled_;
  std::map<std::string, crosapi::mojom::CapabilitiesResponsePtr> caps_map_;
  std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers_;
};

class PrintingAPIHandlerUnittest : public testing::Test {
 public:
  PrintingAPIHandlerUnittest()
      : disable_pdf_flattening_reset_(
            PrintJobSubmitter::DisablePdfFlatteningForTesting()) {}
  PrintingAPIHandlerUnittest(const PrintingAPIHandlerUnittest&) = delete;
  PrintingAPIHandlerUnittest& operator=(const PrintingAPIHandlerUnittest&) =
      delete;
  ~PrintingAPIHandlerUnittest() override = default;

  std::vector<crosapi::mojom::PrintJobPtr> TakePrintJobs() {
    return local_printer_.TakePrintJobs();
  }

  std::vector<TestLocalPrinter::JobInfo> GetJobsCancelled() {
    return local_printer_.jobs_cancelled();
  }

  void AddPrinter(const crosapi::mojom::LocalDestinationInfo& printer) {
    local_printer_.AddPrinter(printer.Clone());
  }

  void SetCaps(const std::string& id,
               crosapi::mojom::CapabilitiesResponsePtr caps) {
    local_printer_.SetCaps(id, std::move(caps));
  }

  std::string SubmitJob(std::string document_data = kPdfExample,
                        const char* content_type = "application/pdf") {
    auto caps = crosapi::mojom::CapabilitiesResponse::New();
    caps->basic_info = crosapi::mojom::LocalDestinationInfo::New();
    caps->capabilities = ConstructPrinterCapabilities();
    SetCaps(kPrinterId, std::move(caps));

    // Create Blob with given data.
    std::unique_ptr<content::BlobHandle> blob = CreateMemoryBackedBlob(
        testing_profile_, document_data, /*content_type=*/"");
    auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt,
                                           content_type, blob->GetUUID());
    EXPECT_TRUE(params);

    SubmitJobFuture job_future;
    printing_api_handler_->SubmitJob(
        /*native_window=*/nullptr, extension_, std::move(params),
        job_future.GetCallback());

    auto [submit_job_status, job_id, error] = job_future.Take();
    EXPECT_FALSE(error);
    EXPECT_TRUE(job_id);
    EXPECT_TRUE(submit_job_status);
    EXPECT_EQ(api::printing::SubmitJobStatus::kOk, submit_job_status);
    // Only lacros needs to report the print job to ash chrome.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    EXPECT_EQ(1u, TakePrintJobs().size());
#else
    EXPECT_EQ(0u, TakePrintJobs().size());
#endif
    return *job_id;
  }

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ =
        profile_manager_->CreateTestingProfile(chrome::kInitialProfile);

    base::Value::List extensions_list;
    extensions_list.Append(kExtensionId);
    testing_profile_->GetTestingPrefService()->SetList(
        prefs::kPrintingAPIExtensionsAllowlist, std::move(extensions_list));

    const char kExtensionName[] = "Printing extension";
    const char kPermissionName[] = "printing";
    extension_ = ExtensionBuilder(kExtensionName)
                     .SetID(kExtensionId)
                     .AddAPIPermission(kPermissionName)
                     .Build();
    ExtensionRegistry::Get(testing_profile_)->AddEnabled(extension_);

    auto print_job_controller = std::make_unique<FakePrintJobController>();
    print_job_controller_ = print_job_controller.get();
    auto cups_wrapper = std::make_unique<chromeos::TestCupsWrapper>();
    cups_wrapper_ = cups_wrapper.get();
    event_router_ = CreateAndUseTestEventRouter(testing_profile_);

    printing_api_handler_ = PrintingAPIHandler::CreateForTesting(
        testing_profile_, event_router_,
        ExtensionRegistry::Get(testing_profile_),
        std::move(print_job_controller), std::move(cups_wrapper),
        &local_printer_);
  }

  void TearDown() override {
    cups_wrapper_ = nullptr;
    print_job_controller_ = nullptr;
    printing_api_handler_.reset();
    event_router_ = nullptr;
    testing_profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
    profile_manager_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<TestingProfile> testing_profile_ = nullptr;
  raw_ptr<TestEventRouter> event_router_ = nullptr;
  raw_ptr<FakePrintJobController> print_job_controller_ = nullptr;
  raw_ptr<chromeos::TestCupsWrapper> cups_wrapper_ = nullptr;
  std::unique_ptr<PrintingAPIHandler> printing_api_handler_;
  scoped_refptr<const Extension> extension_;

 private:
  TestLocalPrinter local_printer_;
  // Resets `disable_pdf_flattening_for_testing` back to false automatically
  // after the test is over.
  base::AutoReset<bool> disable_pdf_flattening_reset_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
};

struct Param {
  crosapi::mojom::PrintJobStatus status;
  api::printing::JobStatus expected_status;
};

class PrintingAPIHandlerParam : public PrintingAPIHandlerUnittest,
                                public testing::WithParamInterface<Param> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    PrintingAPIHandlerParam,
    testing::Values(Param{crosapi::mojom::PrintJobStatus::kUnknown,
                          api::printing::JobStatus::kPending},
                    Param{crosapi::mojom::PrintJobStatus::kStarted,
                          api::printing::JobStatus::kInProgress},
                    Param{crosapi::mojom::PrintJobStatus::kDone,
                          api::printing::JobStatus::kPrinted},
                    Param{crosapi::mojom::PrintJobStatus::kError,
                          api::printing::JobStatus::kFailed},
                    Param{crosapi::mojom::PrintJobStatus::kCancelled,
                          api::printing::JobStatus::kCanceled}));

// Test that `OnJobStatusChanged` is dispatched when the print job status is
// changed.
TEST_P(PrintingAPIHandlerParam, EventIsDispatched) {
  PrintingEventObserver event_observer(
      event_router_, api::printing::OnJobStatusChanged::kEventName);
  const auto job_id = SubmitJob();
  ASSERT_TRUE(job_id.size() > 1);
  int index = job_id.size() - 1;
  const Param& param = GetParam();
  if (param.status != crosapi::mojom::PrintJobStatus::kUnknown) {
    auto update = crosapi::mojom::PrintJobUpdate::New();
    update->status = param.status;
    printing_api_handler_->OnPrintJobUpdate(
        job_id.substr(0, index), job_id[index] - '0', std::move(update));
  }

  event_observer.CheckJobStatusEvent(kExtensionId, job_id,
                                     param.expected_status);
}

// Test that calling GetPrinters() returns no printers before any are added to
// the profile.
TEST_F(PrintingAPIHandlerUnittest, GetPrinters_NoPrinters) {
  GetPrintersFuture printers_future;
  printing_api_handler_->GetPrinters(printers_future.GetCallback());
  EXPECT_TRUE(printers_future.Get().empty());
}

// Test that calling GetPrinters() returns the mock printer.
TEST_F(PrintingAPIHandlerUnittest, GetPrinters_OnePrinter) {
  AddPrinter(crosapi::mojom::LocalDestinationInfo(
      kId1, kName, kDescription, true, std::make_optional(kUri)));

  GetPrintersFuture printers_future;
  printing_api_handler_->GetPrinters(printers_future.GetCallback());

  auto printers = printers_future.Take();
  ASSERT_EQ(1u, printers.size());
  const api::printing::Printer& idl_printer = printers.front();

  EXPECT_EQ(kId1, idl_printer.id);
  EXPECT_EQ(kName, idl_printer.name);
  EXPECT_EQ(kDescription, idl_printer.description);
  EXPECT_EQ(kUri, idl_printer.uri);
  EXPECT_EQ(api::printing::PrinterSource::kPolicy, idl_printer.source);
  EXPECT_FALSE(idl_printer.is_default);
  EXPECT_EQ(std::nullopt, idl_printer.recently_used_rank);
}

// Test that calling GetPrinters() returns printers with correct `is_default`
// flag.
TEST_F(PrintingAPIHandlerUnittest, GetPrinters_IsDefault) {
  testing_profile_->GetPrefs()->SetString(
      prefs::kPrintPreviewDefaultDestinationSelectionRules,
      R"({"kind": "local", "idPattern": "id.*"})");
  AddPrinter(crosapi::mojom::LocalDestinationInfo(
      kId1, kName, kDescription, true, std::make_optional(kUri)));

  GetPrintersFuture printers_future;
  printing_api_handler_->GetPrinters(printers_future.GetCallback());

  auto printers = printers_future.Take();
  ASSERT_EQ(1u, printers.size());
  api::printing::Printer idl_printer = std::move(printers.front());

  EXPECT_EQ(kId1, idl_printer.id);
  EXPECT_TRUE(idl_printer.is_default);
}

// Test that calling GetPrinters() returns printers with correct
// `recently_used_rank` flag.
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
  AddPrinter(crosapi::mojom::LocalDestinationInfo(
      kId1, kName, kDescription, true, std::make_optional(kUri)));

  GetPrintersFuture printers_future;
  printing_api_handler_->GetPrinters(printers_future.GetCallback());

  auto printers = printers_future.Take();
  ASSERT_EQ(1u, printers.size());
  api::printing::Printer idl_printer = std::move(printers.front());

  EXPECT_EQ(kId1, idl_printer.id);
  ASSERT_TRUE(idl_printer.recently_used_rank);
  // The "id1" printer is listed as second printer in the recently used printers
  // list, so we expect 1 as its rank.
  EXPECT_EQ(1, *idl_printer.recently_used_rank);
}

TEST_F(PrintingAPIHandlerUnittest, GetPrinterInfo_InvalidId) {
  GetPrinterInfoFuture printer_info_future;
  printing_api_handler_->GetPrinterInfo(kPrinterId,
                                        printer_info_future.GetCallback());

  auto [capabilities, printer_status, error] = printer_info_future.Take();
  // Printer is not added to CupsPrintersManager, so we expect "Invalid printer
  // id" error.
  EXPECT_FALSE(capabilities);
  EXPECT_FALSE(printer_status);
  ASSERT_TRUE(error);
  EXPECT_EQ("Invalid printer ID", error);
}

TEST_F(PrintingAPIHandlerUnittest, GetPrinterInfo_NoCapabilities) {
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New();
  SetCaps(kPrinterId, std::move(caps));

  GetPrinterInfoFuture printer_info_future;
  printing_api_handler_->GetPrinterInfo(kPrinterId,
                                        printer_info_future.GetCallback());

  auto [capabilities, printer_status, error] = printer_info_future.Take();
  EXPECT_FALSE(capabilities);
  ASSERT_TRUE(printer_status);
  EXPECT_EQ(api::printing::PrinterStatus::kUnreachable, printer_status);
  EXPECT_FALSE(error);
}

TEST_F(PrintingAPIHandlerUnittest, GetPrinterInfo_OutOfPaper) {
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New();
  caps->capabilities = printing::PrinterSemanticCapsAndDefaults();
  SetCaps(kPrinterId, std::move(caps));

  // Mock CUPS wrapper to return predefined status for given printer.
  printing::PrinterStatus::PrinterReason reason{
      printing::PrinterStatus::PrinterReason::Reason::kMediaEmpty,
      printing::PrinterStatus::PrinterReason::Severity::kWarning};
  cups_wrapper_->SetPrinterStatus(kPrinterId, reason);

  std::map<std::string, crosapi::mojom::PrinterStatusPtr> status_map_;

  GetPrinterInfoFuture printer_info_future;
  printing_api_handler_->GetPrinterInfo(kPrinterId,
                                        printer_info_future.GetCallback());

  auto [capabilities, printer_status, error] = printer_info_future.Take();
  ASSERT_TRUE(capabilities);
  const base::Value::Dict* capabilities_dict =
      capabilities->GetDict().FindDict("printer");
  ASSERT_TRUE(capabilities_dict);

  const base::Value::Dict* color = capabilities_dict->FindDict("color");
  ASSERT_TRUE(color);
  const base::Value::List* color_options = color->FindList("option");
  ASSERT_TRUE(color_options);
  ASSERT_EQ(1u, color_options->size());
  const std::string* color_type =
      (*color_options)[0].GetDict().FindString("type");
  ASSERT_TRUE(color_type);
  EXPECT_EQ("STANDARD_MONOCHROME", *color_type);

  const base::Value::Dict* page_orientation =
      capabilities_dict->FindDict("page_orientation");
  ASSERT_TRUE(page_orientation);
  const base::Value::List* page_orientation_options =
      page_orientation->FindList("option");
  ASSERT_TRUE(page_orientation_options);
  ASSERT_EQ(3u, page_orientation_options->size());
  std::vector<std::string> page_orientation_types;
  for (const base::Value& page_orientation_option : *page_orientation_options) {
    const std::string* page_orientation_type =
        page_orientation_option.GetDict().FindString("type");
    ASSERT_TRUE(page_orientation_type);
    page_orientation_types.push_back(*page_orientation_type);
  }
  EXPECT_THAT(page_orientation_types,
              testing::UnorderedElementsAre("PORTRAIT", "LANDSCAPE", "AUTO"));

  ASSERT_TRUE(printer_status);
  EXPECT_EQ(api::printing::PrinterStatus::kOutOfPaper, printer_status);
  EXPECT_FALSE(error);
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_UnsupportedContentType) {
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New();
  caps->capabilities = printing::PrinterSemanticCapsAndDefaults();
  SetCaps(kPrinterId, std::move(caps));

  auto params =
      ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt, "image/jpeg",
                               /*document_blob_uuid=*/std::nullopt);
  ASSERT_TRUE(params);

  SubmitJobFuture job_future;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      job_future.GetCallback());

  auto [submit_job_status, job_id, error] = job_future.Take();
  // According to the documentation only "application/pdf" content type is
  // supported, so we expect an error as a result of API call.
  ASSERT_TRUE(error);
  EXPECT_EQ("Unsupported content type", error);
  EXPECT_FALSE(submit_job_status);
  EXPECT_FALSE(job_id);
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_InvalidPrintTicket) {
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New();
  caps->capabilities = ConstructPrinterCapabilities();
  SetCaps(kPrinterId, std::move(caps));

  auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"",
                                         kIncompleteCjt, "application/pdf",
                                         /*document_blob_uuid=*/std::nullopt);
  ASSERT_TRUE(params);

  SubmitJobFuture job_future;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      job_future.GetCallback());

  auto [submit_job_status, job_id, error] = job_future.Take();
  // Some fields of the print ticket are missing, so we expect an error as a
  // result of API call.
  ASSERT_TRUE(error);
  EXPECT_EQ("Invalid ticket", error);
  EXPECT_FALSE(submit_job_status);
  EXPECT_FALSE(job_id);
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_InvalidPrinterId) {
  auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt,
                                         "application/pdf",
                                         /*document_blob_uuid=*/std::nullopt);
  ASSERT_TRUE(params);

  SubmitJobFuture job_future;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      job_future.GetCallback());

  auto [submit_job_status, job_id, error] = job_future.Take();
  // The printer is not added, so we expect an error as a result of API call.
  ASSERT_TRUE(error);
  EXPECT_EQ("Invalid printer ID", error);
  EXPECT_FALSE(submit_job_status);
  EXPECT_FALSE(job_id);
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_PrinterUnavailable) {
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New();
  SetCaps(kPrinterId, std::move(caps));

  auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt,
                                         "application/pdf",
                                         /*document_blob_uuid=*/std::nullopt);
  ASSERT_TRUE(params);

  SubmitJobFuture job_future;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      job_future.GetCallback());

  auto [submit_job_status, job_id, error] = job_future.Take();
  // Even though the printer is added, it's not able to accept jobs until it's
  // added as valid printer, so we expect an error as a result of API call.
  ASSERT_TRUE(error);
  EXPECT_EQ("Printer is unavailable at the moment", error);
  EXPECT_FALSE(submit_job_status);
  EXPECT_FALSE(job_id);
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_UnsupportedTicket) {
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New();
  caps->capabilities = printing::PrinterSemanticCapsAndDefaults();
  SetCaps(kPrinterId, std::move(caps));

  auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt,
                                         "application/pdf",
                                         /*document_blob_uuid=*/std::nullopt);
  ASSERT_TRUE(params);

  SubmitJobFuture job_future;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      job_future.GetCallback());

  auto [submit_job_status, job_id, error] = job_future.Take();
  // Print ticket requires some non-default parameters as DPI and media size
  // which are not supported for default capabilities, so we expect an error as
  // a result of API call.
  ASSERT_TRUE(error);
  EXPECT_EQ("Ticket is unsupported on the given printer", error);
  EXPECT_FALSE(submit_job_status);
  EXPECT_FALSE(job_id);
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_InvalidData) {
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New();
  caps->capabilities = ConstructPrinterCapabilities();
  SetCaps(kPrinterId, std::move(caps));

  auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt,
                                         "application/pdf", "invalid_uuid");
  ASSERT_TRUE(params);

  SubmitJobFuture job_future;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      job_future.GetCallback());

  auto [submit_job_status, job_id, error] = job_future.Take();
  // We can't fetch actual document data without Blob UUID, so we expect an
  // error as a result of API call.
  ASSERT_TRUE(error);
  EXPECT_EQ("Invalid document", error);
  EXPECT_FALSE(submit_job_status);
  EXPECT_FALSE(job_id);
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_InvalidDataPNG) {
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New();
  caps->capabilities = ConstructPrinterCapabilities();
  SetCaps(kPrinterId, std::move(caps));

  auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt,
                                         "image/png", "invalid_uuid");
  ASSERT_TRUE(params);

  SubmitJobFuture job_future;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      job_future.GetCallback());

  auto [submit_job_status, job_id, error] = job_future.Take();
  // We can't fetch actual document data without Blob UUID, so we expect an
  // error as a result of API call.
  ASSERT_TRUE(error);
  EXPECT_EQ("Invalid document", error);
  EXPECT_FALSE(submit_job_status);
  EXPECT_FALSE(job_id);
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_PrintingFailed) {
  print_job_controller_->set_fail(true);
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New();
  caps->capabilities = ConstructPrinterCapabilities();
  SetCaps(kPrinterId, std::move(caps));

  // Create Blob with given data.
  std::unique_ptr<content::BlobHandle> blob = CreateMemoryBackedBlob(
      testing_profile_, kPdfExample, /*content_type=*/"");
  auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt,
                                         "application/pdf", blob->GetUUID());
  ASSERT_TRUE(params);

  SubmitJobFuture job_future;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      job_future.GetCallback());

  auto [submit_job_status, job_id, error] = job_future.Take();
  ASSERT_TRUE(error);
  EXPECT_EQ("Printing failed", error);
  EXPECT_FALSE(submit_job_status);
  EXPECT_FALSE(job_id);
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob) {
  SubmitJob();
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_PNG) {
  SubmitJob(std::string(kPngExample, kPngExampleSize), "image/png");
}

TEST_F(PrintingAPIHandlerUnittest, CancelJob_InvalidId) {
  std::optional<std::string> error =
      printing_api_handler_->CancelJob(kExtensionId, "job_id");

  ASSERT_TRUE(error);
  EXPECT_EQ("No active print job with given ID", error);
  EXPECT_TRUE(GetJobsCancelled().empty());
}

TEST_F(PrintingAPIHandlerUnittest, CancelJob_InvalidId_OtherExtension) {
  const auto job_id = SubmitJob();

  // Try to cancel print job from other extension.
  std::optional<std::string> error =
      printing_api_handler_->CancelJob(kExtensionId2, job_id);

  ASSERT_TRUE(error);
  EXPECT_EQ("No active print job with given ID", error);
  EXPECT_TRUE(GetJobsCancelled().empty());
}

TEST_F(PrintingAPIHandlerUnittest, CancelJob_InvalidState) {
  const auto job_id = SubmitJob();

  // Explicitly complete started print job.
  ASSERT_TRUE(job_id.size() > 1);
  int index = job_id.size() - 1;
  auto update = crosapi::mojom::PrintJobUpdate::New();
  update->status = crosapi::mojom::PrintJobStatus::kDone;
  printing_api_handler_->OnPrintJobUpdate(
      job_id.substr(0, index), job_id[index] - '0', std::move(update));

  // Try to cancel already completed print job.
  std::optional<std::string> error =
      printing_api_handler_->CancelJob(kExtensionId, job_id);

  ASSERT_TRUE(error);
  EXPECT_EQ("No active print job with given ID", error);
  EXPECT_TRUE(GetJobsCancelled().empty());
}

TEST_F(PrintingAPIHandlerUnittest, CancelJob) {
  const auto job_id = SubmitJob();

  PrintingEventObserver event_observer(
      event_router_, api::printing::OnJobStatusChanged::kEventName);

  // Cancel started print job.
  std::optional<std::string> error =
      printing_api_handler_->CancelJob(kExtensionId, job_id);

  EXPECT_FALSE(error);
  ASSERT_EQ(1u, GetJobsCancelled().size());
  EXPECT_EQ(job_id,
            PrintingAPIHandler::CreateUniqueId(GetJobsCancelled()[0].printer_id,
                                               GetJobsCancelled()[0].job_id));
  // Job should not be canceled yet.
  EXPECT_EQ("", event_observer.extension_id());
  EXPECT_TRUE(event_observer.event_args().is_none());

  auto update = crosapi::mojom::PrintJobUpdate::New();
  update->status = crosapi::mojom::PrintJobStatus::kCancelled;
  printing_api_handler_->OnPrintJobUpdate(GetJobsCancelled()[0].printer_id,
                                          GetJobsCancelled()[0].job_id,
                                          std::move(update));

  // Now the job is canceled.
  event_observer.CheckJobStatusEvent(kExtensionId, job_id,
                                     api::printing::JobStatus::kCanceled);
}

}  // namespace extensions
