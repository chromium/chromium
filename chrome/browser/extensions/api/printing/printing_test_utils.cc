// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/printing/printing_test_utils.h"

#include "base/check_deref.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/ash/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/fake_cups_printers_manager.h"
#include "chrome/browser/ash/printing/test_cups_print_job_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/common/constants.h"
#include "extensions/test/test_extension_dir.h"
#include "printing/backend/test_print_backend.h"

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
constexpr int kMediaSizeWidth = 210000;
constexpr int kMediaSizeHeight = 297000;
constexpr char kMediaSizeVendorId[] = "iso_a4_210x297mm";

// Mapping of the different extension types used in the test to the specific
// manifest file names to create an extension of that type. The actual location
// of these files is at //chrome/test/data/extensions/api_test/printing/.
static constexpr auto kManifestFileNames =
    base::MakeFixedFlatMap<ExtensionType, base::StringPiece>(
        {{ExtensionType::kChromeApp, "manifest_chrome_app.json"},
         {ExtensionType::kExtensionMV2, "manifest_extension.json"},
         {ExtensionType::kExtensionMV3, "manifest_v3_extension.json"}});

std::unique_ptr<KeyedService> BuildTestCupsPrintJobManager(
    content::BrowserContext* context) {
  return std::make_unique<ash::TestCupsPrintJobManager>(
      Profile::FromBrowserContext(context));
}

std::unique_ptr<KeyedService> BuildFakeCupsPrintersManager(
    content::BrowserContext* context) {
  return std::make_unique<ash::FakeCupsPrintersManager>();
}

}  // namespace

PrintingTestHelper::PrintingTestHelper()
    : test_print_backend_(base::MakeRefCounted<printing::TestPrintBackend>()) {
  CHECK(BrowserContextDependencyManager::GetInstance());
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
              &PrintingTestHelper::OnWillCreateBrowserContextServices,
              base::Unretained(this)));
}

PrintingTestHelper::~PrintingTestHelper() {
  printing::PrintBackend::SetPrintBackendForTesting(nullptr);
  print_backend_service_.reset();
  test_remote_.reset_on_disconnect();
  printing::PrintBackendServiceManager::GetInstance().ResetForTesting();
}

void PrintingTestHelper::Init(Profile* profile) {
  profile_ = profile;
  printing::PrintBackend::SetPrintBackendForTesting(test_print_backend_.get());

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (base::FeatureList::IsEnabled(
          printing::features::kEnableOopPrintDrivers)) {
    print_backend_service_ =
        printing::PrintBackendServiceTestImpl::LaunchForTesting(
            test_remote_, test_print_backend_.get(), /*sandboxed=*/true);
  }
#endif
}

ash::TestCupsPrintJobManager* PrintingTestHelper::GetPrintJobManager() {
  CHECK(profile_);
  return static_cast<ash::TestCupsPrintJobManager*>(
      ash::CupsPrintJobManagerFactory::GetForBrowserContext(profile_));
}

ash::FakeCupsPrintersManager* PrintingTestHelper::GetPrintersManager() {
  CHECK(profile_);
  return static_cast<ash::FakeCupsPrintersManager*>(
      ash::CupsPrintersManagerFactory::GetForBrowserContext(profile_));
}

void PrintingTestHelper::AddAvailablePrinter(
    const std::string& printer_id,
    std::unique_ptr<printing::PrinterSemanticCapsAndDefaults> capabilities) {
  auto printer = chromeos::Printer(printer_id);
  printer.SetUri("ipp://192.168.1.0");
  GetPrintersManager()->AddPrinter(printer,
                                   chromeos::PrinterClass::kEnterprise);
  chromeos::CupsPrinterStatus status(printer_id);
  status.AddStatusReason(
      chromeos::CupsPrinterStatus::CupsPrinterStatusReason::Reason::
          kPrinterUnreachable,
      chromeos::CupsPrinterStatus::CupsPrinterStatusReason::Severity::kError);
  GetPrintersManager()->SetPrinterStatus(status);
  test_print_backend_->AddValidPrinter(printer_id, std::move(capabilities),
                                       nullptr);
}

void PrintingTestHelper::OnWillCreateBrowserContextServices(
    content::BrowserContext* context) {
  ash::CupsPrintJobManagerFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&BuildTestCupsPrintJobManager));
  ash::CupsPrintersManagerFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&BuildFakeCupsPrintersManager));
}

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
  capabilities->color_model = printing::mojom::ColorModel::kColor;
  capabilities->duplex_modes.push_back(printing::mojom::DuplexMode::kSimplex);
  capabilities->copies_max = 2;
  capabilities->dpis.emplace_back(kHorizontalDpi, kVerticalDpi);
  printing::PrinterSemanticCapsAndDefaults::Paper paper(
      /*display_name=*/"", kMediaSizeVendorId,
      {kMediaSizeWidth, kMediaSizeHeight});
  capabilities->papers.push_back(std::move(paper));
  capabilities->collate_capable = true;
  return capabilities;
}

}  // namespace extensions
