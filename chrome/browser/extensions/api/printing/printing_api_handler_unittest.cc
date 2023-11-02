// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/printing/printing_api_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
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
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  const std::string& extension_id() const { return extension_id_; }

  const base::Value& event_args() const { return event_args_; }

 private:
  // Event router this class should observe.
  const raw_ptr<TestEventRouter> event_router_;

  // The name of the observed event.
  const std::string event_name_;

  // The extension id passed for the last observed event.
  std::string extension_id_;

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

std::unique_ptr<api::printing::SubmitJob::Params> ConstructSubmitJobParams(
    const std::string& printer_id,
    const std::string& title,
    const std::string& ticket,
    const std::string& content_type,
    absl::optional<std::string> document_blob_uuid) {
  api::printing::SubmitJobRequest request;
  request.job.printer_id = printer_id;
  request.job.title = title;
  EXPECT_TRUE(api::printer_provider::PrintJob::Ticket::Populate(
      base::test::ParseJson(ticket), &request.job.ticket));
  request.job.content_type = content_type;
  request.document_blob_uuid = std::move(document_blob_uuid);

  base::Value::List args;
  args.Append(base::Value(request.ToValue()));
  return api::printing::SubmitJob::Params::Create(args);
}

absl::optional<printing::PrinterSemanticCapsAndDefaults>
ConstructPrinterCapabilities() {
  printing::PrinterSemanticCapsAndDefaults capabilities;
  capabilities.color_model = printing::mojom::ColorModel::kColor;
  capabilities.duplex_modes.push_back(printing::mojom::DuplexMode::kSimplex);
  capabilities.copies_max = 5;
  capabilities.dpis.push_back(gfx::Size(kHorizontalDpi, kVerticalDpi));
  printing::PrinterSemanticCapsAndDefaults::Paper paper;
  paper.vendor_id = kMediaSizeVendorId;
  paper.size_um = gfx::Size(kMediaSizeWidth, kMediaSizeHeight);
  capabilities.papers.push_back(paper);
  capabilities.collate_capable = true;
  return capabilities;
}

std::unique_ptr<content::BlobHandle> CreateMemoryBackedBlob(
    content::BrowserContext* browser_context,
    const std::string& content,
    const std::string& content_type) {
  std::unique_ptr<content::BlobHandle> result;
  base::RunLoop run_loop;
  browser_context->CreateMemoryBackedBlob(
      base::as_bytes(base::make_span(content)), content_type,
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

  void SubmitJob() {
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

    base::RunLoop run_loop;
    printing_api_handler_->SubmitJob(
        /*native_window=*/nullptr, extension_, std::move(params),
        base::BindOnce(&PrintingAPIHandlerUnittest::OnJobSubmitted,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_FALSE(error_.has_value());
    EXPECT_TRUE(job_id_);
    ASSERT_TRUE(submit_job_status_.has_value());
    EXPECT_EQ(api::printing::SUBMIT_JOB_STATUS_OK, submit_job_status_.value());
    // Only lacros needs to report the print job to ash chrome.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    EXPECT_EQ(1u, TakePrintJobs().size());
#else
    EXPECT_EQ(0u, TakePrintJobs().size());
#endif
  }

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
    printing_api_handler_.reset();
    testing_profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
  }

  void OnJobSubmitted(base::RepeatingClosure run_loop_closure,
                      absl::optional<api::printing::SubmitJobStatus> status,
                      absl::optional<std::string> job_id,
                      absl::optional<std::string> error) {
    submit_job_status_ = status;
    job_id_ = std::move(job_id);
    error_ = error;
    run_loop_closure.Run();
  }

  void OnPrintersRetrieved(base::RepeatingClosure run_loop_closure,
                           std::vector<api::printing::Printer> printers) {
    printers_ = std::move(printers);
    run_loop_closure.Run();
  }

  void OnPrinterInfoRetrieved(
      base::RepeatingClosure run_loop_closure,
      absl::optional<base::Value> capabilities,
      absl::optional<api::printing::PrinterStatus> printer_status,
      absl::optional<std::string> error) {
    if (capabilities)
      capabilities_ = capabilities.value().Clone();
    else
      capabilities_ = absl::nullopt;
    printer_status_ = printer_status;
    error_ = error;
    run_loop_closure.Run();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<TestingProfile> testing_profile_;
  raw_ptr<TestEventRouter> event_router_ = nullptr;
  raw_ptr<FakePrintJobController> print_job_controller_;
  raw_ptr<chromeos::TestCupsWrapper> cups_wrapper_;
  std::unique_ptr<PrintingAPIHandler> printing_api_handler_;
  scoped_refptr<const Extension> extension_;
  absl::optional<api::printing::SubmitJobStatus> submit_job_status_;
  absl::optional<std::string> job_id_;
  absl::optional<base::Value> capabilities_;
  absl::optional<api::printing::PrinterStatus> printer_status_;
  absl::optional<std::string> error_;
  std::vector<api::printing::Printer> printers_;

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
                          api::printing::JOB_STATUS_PENDING},
                    Param{crosapi::mojom::PrintJobStatus::kStarted,
                          api::printing::JOB_STATUS_IN_PROGRESS},
                    Param{crosapi::mojom::PrintJobStatus::kDone,
                          api::printing::JOB_STATUS_PRINTED},
                    Param{crosapi::mojom::PrintJobStatus::kError,
                          api::printing::JOB_STATUS_FAILED},
                    Param{crosapi::mojom::PrintJobStatus::kCancelled,
                          api::printing::JOB_STATUS_CANCELED}));

// Test that `OnJobStatusChanged` is dispatched when the print job status is
// changed.
TEST_P(PrintingAPIHandlerParam, EventIsDispatched) {
  PrintingEventObserver event_observer(
      event_router_, api::printing::OnJobStatusChanged::kEventName);
  SubmitJob();
  ASSERT_TRUE(job_id_);
  ASSERT_TRUE(job_id_->size() > 1);
  int index = job_id_->size() - 1;
  const Param& param = GetParam();
  if (param.status != crosapi::mojom::PrintJobStatus::kUnknown) {
    printing_api_handler_->OnPrintJobUpdate(
        job_id_->substr(0, index), (*job_id_)[index] - '0', param.status);
  }

  event_observer.CheckJobStatusEvent(kExtensionId, *job_id_,
                                     param.expected_status);
}

// Test that calling GetPrinters() returns no printers before any are added to
// the profile.
TEST_F(PrintingAPIHandlerUnittest, GetPrinters_NoPrinters) {
  base::RunLoop run_loop;
  printing_api_handler_->GetPrinters(
      base::BindOnce(&PrintingAPIHandlerUnittest::OnPrintersRetrieved,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(printers_.empty());
}

// Test that calling GetPrinters() returns the mock printer.
TEST_F(PrintingAPIHandlerUnittest, GetPrinters_OnePrinter) {
  AddPrinter(crosapi::mojom::LocalDestinationInfo(
      kId1, kName, kDescription, true, absl::make_optional(kUri)));

  base::RunLoop run_loop;
  printing_api_handler_->GetPrinters(
      base::BindOnce(&PrintingAPIHandlerUnittest::OnPrintersRetrieved,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_EQ(1u, printers_.size());
  const api::printing::Printer& idl_printer = printers_.front();

  EXPECT_EQ(kId1, idl_printer.id);
  EXPECT_EQ(kName, idl_printer.name);
  EXPECT_EQ(kDescription, idl_printer.description);
  EXPECT_EQ(kUri, idl_printer.uri);
  EXPECT_EQ(api::printing::PRINTER_SOURCE_POLICY, idl_printer.source);
  EXPECT_FALSE(idl_printer.is_default);
  EXPECT_EQ(absl::nullopt, idl_printer.recently_used_rank);
}

// Test that calling GetPrinters() returns printers with correct `is_default`
// flag.
TEST_F(PrintingAPIHandlerUnittest, GetPrinters_IsDefault) {
  testing_profile_->GetPrefs()->SetString(
      prefs::kPrintPreviewDefaultDestinationSelectionRules,
      R"({"kind": "local", "idPattern": "id.*"})");
  AddPrinter(crosapi::mojom::LocalDestinationInfo(
      kId1, kName, kDescription, true, absl::make_optional(kUri)));

  base::RunLoop run_loop;
  printing_api_handler_->GetPrinters(
      base::BindOnce(&PrintingAPIHandlerUnittest::OnPrintersRetrieved,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_EQ(1u, printers_.size());
  api::printing::Printer idl_printer = std::move(printers_.front());

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
      kId1, kName, kDescription, true, absl::make_optional(kUri)));

  base::RunLoop run_loop;
  printing_api_handler_->GetPrinters(
      base::BindOnce(&PrintingAPIHandlerUnittest::OnPrintersRetrieved,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_EQ(1u, printers_.size());
  api::printing::Printer idl_printer = std::move(printers_.front());

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
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New();
  SetCaps(kPrinterId, std::move(caps));

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
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New();
  caps->capabilities = printing::PrinterSemanticCapsAndDefaults();
  SetCaps(kPrinterId, std::move(caps));

  auto params =
      ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt, "image/jpeg",
                               /*document_blob_uuid=*/absl::nullopt);
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
  EXPECT_FALSE(job_id_);
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_InvalidPrintTicket) {
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New();
  caps->capabilities = ConstructPrinterCapabilities();
  SetCaps(kPrinterId, std::move(caps));

  auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"",
                                         kIncompleteCjt, "application/pdf",
                                         /*document_blob_uuid=*/absl::nullopt);
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
  EXPECT_FALSE(job_id_);
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_InvalidPrinterId) {
  auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt,
                                         "application/pdf",
                                         /*document_blob_uuid=*/absl::nullopt);
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
  EXPECT_FALSE(job_id_);
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_PrinterUnavailable) {
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New();
  SetCaps(kPrinterId, std::move(caps));

  auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt,
                                         "application/pdf",
                                         /*document_blob_uuid=*/absl::nullopt);
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
  EXPECT_FALSE(job_id_);
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_UnsupportedTicket) {
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New();
  caps->capabilities = printing::PrinterSemanticCapsAndDefaults();
  SetCaps(kPrinterId, std::move(caps));

  auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt,
                                         "application/pdf",
                                         /*document_blob_uuid=*/absl::nullopt);
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
  EXPECT_FALSE(job_id_);
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob_InvalidData) {
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New();
  caps->capabilities = ConstructPrinterCapabilities();
  SetCaps(kPrinterId, std::move(caps));

  auto params = ConstructSubmitJobParams(kPrinterId, /*title=*/"", kCjt,
                                         "application/pdf", "invalid_uuid");
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
  EXPECT_FALSE(job_id_);
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

  base::RunLoop run_loop;
  printing_api_handler_->SubmitJob(
      /*native_window=*/nullptr, extension_, std::move(params),
      base::BindOnce(&PrintingAPIHandlerUnittest::OnJobSubmitted,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(error_.has_value());
  EXPECT_EQ("Printing failed", error_.value());
  EXPECT_FALSE(submit_job_status_.has_value());
  EXPECT_FALSE(job_id_);
}

TEST_F(PrintingAPIHandlerUnittest, SubmitJob) {
  SubmitJob();
}

TEST_F(PrintingAPIHandlerUnittest, CancelJob_InvalidId) {
  absl::optional<std::string> error =
      printing_api_handler_->CancelJob(kExtensionId, "job_id");

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("No active print job with given ID", error.value());
  EXPECT_TRUE(GetJobsCancelled().empty());
}

TEST_F(PrintingAPIHandlerUnittest, CancelJob_InvalidId_OtherExtension) {
  SubmitJob();

  // Try to cancel print job from other extension.
  ASSERT_TRUE(job_id_);
  absl::optional<std::string> error =
      printing_api_handler_->CancelJob(kExtensionId2, *job_id_);

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("No active print job with given ID", error.value());
  EXPECT_TRUE(GetJobsCancelled().empty());
}

TEST_F(PrintingAPIHandlerUnittest, CancelJob_InvalidState) {
  SubmitJob();

  // Explicitly complete started print job.
  ASSERT_TRUE(job_id_);
  ASSERT_TRUE(job_id_->size() > 1);
  int index = job_id_->size() - 1;
  printing_api_handler_->OnPrintJobUpdate(
      job_id_->substr(0, index), (*job_id_)[index] - '0',
      crosapi::mojom::PrintJobStatus::kDone);

  // Try to cancel already completed print job.
  absl::optional<std::string> error =
      printing_api_handler_->CancelJob(kExtensionId, *job_id_);

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("No active print job with given ID", error.value());
  EXPECT_TRUE(GetJobsCancelled().empty());
}

TEST_F(PrintingAPIHandlerUnittest, CancelJob) {
  SubmitJob();

  PrintingEventObserver event_observer(
      event_router_, api::printing::OnJobStatusChanged::kEventName);

  // Cancel started print job.
  ASSERT_TRUE(job_id_);
  absl::optional<std::string> error =
      printing_api_handler_->CancelJob(kExtensionId, *job_id_);

  EXPECT_FALSE(error.has_value());
  ASSERT_EQ(1u, GetJobsCancelled().size());
  EXPECT_EQ(*job_id_,
            PrintingAPIHandler::CreateUniqueId(GetJobsCancelled()[0].printer_id,
                                               GetJobsCancelled()[0].job_id));
  // Job should not be canceled yet.
  EXPECT_EQ("", event_observer.extension_id());
  EXPECT_TRUE(event_observer.event_args().is_none());

  printing_api_handler_->OnPrintJobUpdate(
      GetJobsCancelled()[0].printer_id, GetJobsCancelled()[0].job_id,
      crosapi::mojom::PrintJobStatus::kCancelled);

  // Now the job is canceled.
  event_observer.CheckJobStatusEvent(kExtensionId, *job_id_,
                                     api::printing::JOB_STATUS_CANCELED);
}

}  // namespace extensions
