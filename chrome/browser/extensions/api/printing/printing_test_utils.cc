// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/printing/printing_test_utils.h"

#include <string_view>

#include "base/check_deref.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/printing/local_printer_utils_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "extensions/test/test_extension_dir.h"
#include "printing/backend/cups_ipp_constants.h"
#include "printing/backend/print_backend.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/printing/cups_print_job_manager.h"
#include "chrome/browser/ash/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/fake_cups_print_job_manager.h"
#include "chrome/browser/ash/printing/fake_cups_printers_manager.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "printing/backend/test_print_backend.h"
#include "printing/printing_context.h"
#endif

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "base/feature_list.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/printing_features.h"
#endif

namespace extensions {

namespace {

constexpr int kHorizontalDpi = 300;
constexpr int kVerticalDpi = 400;

// Size of ISO A4 paper in microns.
constexpr int kIsoA4Width = 210000;
constexpr int kIsoA4Height = 297000;

// Size of NA Letter paper in microns.
constexpr int kNaLetterWidth = 215900;
constexpr int kNaLetterHeight = 279400;

// Size of a custom paper with variable height in microns.
constexpr int kCustomPaperWidth = 200000;
constexpr int kCustomPaperHeight = 250000;
constexpr int kCustomPaperMaxHeight = 300000;

// Mapping of the different extension types used in the test to the specific
// manifest file names to create an extension of that type. The actual location
// of these files is at //chrome/test/data/extensions/api_test/printing/.
static constexpr auto kManifestFileNames =
    base::MakeFixedFlatMap<ExtensionType, std::string_view>(
        {{ExtensionType::kChromeApp, "manifest_chrome_app.json"},
         {ExtensionType::kExtensionMV2, "manifest_extension.json"},
         {ExtensionType::kExtensionMV3, "manifest_v3_extension.json"}});

#if BUILDFLAG(IS_CHROMEOS_ASH)
// This class uses methods from FakeCupsPrintJobManager while connecting it to
// the rest of the printing pipeline so that it no longer has to be directly
// invoked by the test code.
class FakePrintJobManagerWithDocDone : public ash::FakeCupsPrintJobManager {
 public:
  explicit FakePrintJobManagerWithDocDone(Profile* profile)
      : FakeCupsPrintJobManager(profile) {
    subscription_ = g_browser_process->print_job_manager()->AddDocDoneCallback(
        base::BindRepeating(&FakePrintJobManagerWithDocDone::OnDocDone,
                            base::Unretained(this)));
  }

  void OnDocDone(printing::PrintJob* job,
                 printing::PrintedDocument* document,
                 int job_id) {
    const auto& settings = document->settings();
    // ash::printing::proto::PrintSettings are only useful for real pipelines
    // for logging print data; this can be omitted in tests.
    CreatePrintJob(
        base::UTF16ToUTF8(settings.device_name()),
        base::UTF16ToUTF8(settings.title()), job_id,
        /*total_page_number=*/document->page_count() * settings.copies(),
        job->source(), job->source_id(), ash::printing::proto::PrintSettings());
  }

 private:
  printing::PrintJobManager::DocDoneCallbackList::Subscription subscription_;
};

std::unique_ptr<KeyedService> BuildFakeCupsPrintJobManagerWithDocDone(
    content::BrowserContext* context) {
  return std::make_unique<FakePrintJobManagerWithDocDone>(
      Profile::FromBrowserContext(context));
}

std::unique_ptr<KeyedService> BuildFakeCupsPrintersManager(
    content::BrowserContext* context) {
  return std::make_unique<ash::FakeCupsPrintersManager>();
}
#endif

}  // namespace

PrintingBackendInfrastructureHelper::PrintingBackendInfrastructureHelper()
    : test_print_backend_(base::MakeRefCounted<printing::TestPrintBackend>()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  printing::PrintBackend::SetPrintBackendForTesting(test_print_backend_.get());
  printing::PrintingContext::SetPrintingContextFactoryForTest(
      &test_printing_context_factory_);

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (base::FeatureList::IsEnabled(
          printing::features::kEnableOopPrintDrivers)) {
    print_backend_service_ =
        printing::PrintBackendServiceTestImpl::LaunchForTesting(
            test_remote_, test_print_backend_.get(), /*sandboxed=*/true);
    // Replace disconnect handler.
    test_remote_.reset_on_disconnect();
  }
#endif
}

PrintingBackendInfrastructureHelper::~PrintingBackendInfrastructureHelper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  printing::PrintingContext::SetPrintingContextFactoryForTest(nullptr);
  printing::PrintBackend::SetPrintBackendForTesting(nullptr);

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (base::FeatureList::IsEnabled(
          printing::features::kEnableOopPrintDrivers)) {
    printing::PrintBackendServiceManager::GetInstance().ResetForTesting();
  }
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
PrintingTestHelper::PrintingTestHelper() {
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
              &PrintingTestHelper::OnWillCreateBrowserContextServices,
              base::Unretained(this)));
}

PrintingTestHelper::~PrintingTestHelper() = default;

void PrintingTestHelper::Init(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  profile_ = profile;
  printing_infra_helper_ =
      std::make_unique<PrintingBackendInfrastructureHelper>();
}

void PrintingTestHelper::AddAvailablePrinter(
    const std::string& printer_id,
    const std::string& printer_display_name,
    std::unique_ptr<printing::PrinterSemanticCapsAndDefaults> capabilities) {
  CHECK(profile_);
  CHECK(printing_infra_helper_);

  chromeos::Printer printer;
  printer.set_id(printer_id);
  printer.set_display_name(printer_display_name);
  printer.SetUri("ipp://192.168.1.0");

  auto* printers_manager = static_cast<ash::FakeCupsPrintersManager*>(
      ash::CupsPrintersManagerFactory::GetForBrowserContext(profile_));
  printers_manager->AddPrinter(printer, chromeos::PrinterClass::kEnterprise);
  chromeos::CupsPrinterStatus status(printer_id);
  status.AddStatusReason(
      chromeos::CupsPrinterStatus::CupsPrinterStatusReason::Reason::
          kPrinterUnreachable,
      chromeos::CupsPrinterStatus::CupsPrinterStatusReason::Severity::kError);
  printers_manager->SetPrinterStatus(status);
  printing_infra_helper_->test_print_backend().AddValidPrinter(
      printer_id, std::move(capabilities), nullptr);

  // Printers in the test context are identified by `printer_id`.
  printing_infra_helper_->test_printing_context_factory()
      .SetPrinterNameForSubsequentContexts(printer_id);
}

void PrintingTestHelper::OnWillCreateBrowserContextServices(
    content::BrowserContext* context) {
  ash::CupsPrintJobManagerFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&BuildFakeCupsPrintJobManagerWithDocDone));
  ash::CupsPrintersManagerFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&BuildFakeCupsPrintersManager));
}
#endif

std::unique_ptr<TestExtensionDir> CreatePrintingExtension(ExtensionType type) {
  auto extension_dir = std::make_unique<TestExtensionDir>();

  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  base::FilePath printing_dir = test_data_dir.AppendASCII("extensions")
                                    .AppendASCII("api_test")
                                    .AppendASCII("printing");

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::CopyDirectory(printing_dir, extension_dir->UnpackedPath(),
                      /*recursive=*/false);
  extension_dir->CopyFileTo(printing_dir.AppendASCII(CHECK_DEREF(
                                base::FindOrNull(kManifestFileNames, type))),
                            extensions::kManifestFilename);

  return extension_dir;
}

std::unique_ptr<printing::PrinterSemanticCapsAndDefaults>
ConstructPrinterCapabilities() {
  auto capabilities =
      std::make_unique<printing::PrinterSemanticCapsAndDefaults>();
  capabilities->bw_model = printing::mojom::ColorModel::kGray;
  capabilities->color_model = printing::mojom::ColorModel::kColor;
  capabilities->duplex_default = printing::mojom::DuplexMode::kSimplex;
  capabilities->duplex_modes.push_back(printing::mojom::DuplexMode::kSimplex);
  capabilities->copies_max = 2;
  capabilities->default_dpi = {kHorizontalDpi, kVerticalDpi};
  capabilities->dpis.emplace_back(capabilities->default_dpi);
  printing::PrinterSemanticCapsAndDefaults::Paper iso_a4_paper(
      /*display_name=*/"", /*vendor_id=*/"", {kIsoA4Width, kIsoA4Height});
  printing::PrinterSemanticCapsAndDefaults::Paper na_letter_paper(
      /*display_name=*/"", /*vendor_id=*/"", {kNaLetterWidth, kNaLetterHeight});
  printing::PrinterSemanticCapsAndDefaults::Paper custom_paper(
      /*display_name=*/"", /*vendor_id=*/"",
      {kCustomPaperWidth, kCustomPaperHeight},
      /*printable_area_um=*/{kCustomPaperWidth, kCustomPaperHeight},
      /*max_height_um=*/kCustomPaperMaxHeight);
  capabilities->default_paper = iso_a4_paper;
  capabilities->papers = {std::move(iso_a4_paper), std::move(na_letter_paper),
                          std::move(custom_paper)};
  capabilities->collate_capable = true;
  std::vector<printing::AdvancedCapabilityValue> media_source_vals(
      {{"auto", ""}, {"tray-1", ""}});
  capabilities->advanced_capabilities.emplace_back(
      /*name=*/printing::kIppMediaSource, /*localized_name=*/"",
      printing::AdvancedCapability::Type::kString, /*default_value=*/"auto",
      /*values=*/std::move(media_source_vals));
  return capabilities;
}

std::vector<crosapi::mojom::LocalDestinationInfoPtr>
ConstructGetPrintersResponse(const std::string& printer_id,
                             const std::string& printer_name) {
  chromeos::Printer printer;
  printer.set_id(printer_id);
  printer.set_display_name(printer_name);
  std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers;
  printers.push_back(printing::PrinterToMojom(printer));
  return printers;
}

}  // namespace extensions
