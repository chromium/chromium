// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINTING_TEST_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINTING_TEST_UTILS_H_

#include <memory>
#include <string>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/printing/browser_printing_context_factory_for_test.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/callback_list.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

class Profile;

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace ash {
class TestCupsPrintJobManager;
class FakeCupsPrintersManager;
}  // namespace ash
#endif

namespace content {
class BrowserContext;
}  // namespace content

namespace printing {
struct PrinterSemanticCapsAndDefaults;
class PrintBackendServiceTestImpl;
class TestPrintBackend;
}  // namespace printing

namespace extensions {

class TestExtensionDir;

// Enum used to initialize the parameterized test with different types of
// extensions.
enum class ExtensionType {
  kChromeApp,
  kExtensionMV2,
  kExtensionMV3,
};

// Manages various printing-related test infra classes. This class is supposed
// to be used on the main thread.
class PrintingBackendInfrastructureHelper {
 public:
  PrintingBackendInfrastructureHelper();
  ~PrintingBackendInfrastructureHelper();

  printing::TestPrintBackend& test_print_backend() {
    return *test_print_backend_;
  }
  printing::BrowserPrintingContextFactoryForTest&
  test_printing_context_factory() {
    return test_printing_context_factory_;
  }

 private:
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  mojo::Remote<printing::mojom::PrintBackendService> test_remote_;
  std::unique_ptr<printing::PrintBackendServiceTestImpl> print_backend_service_;
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  scoped_refptr<printing::TestPrintBackend> test_print_backend_;
  printing::BrowserPrintingContextFactoryForTest test_printing_context_factory_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
class PrintingTestHelper {
 public:
  // BrowserContextDependencyManager subscriptions should be established before
  // the profile becomes available; for this reason `Profile*` is not provided
  // as a constructor parameter but rather passed in Init().
  // Note that most methods of this class (other than the constructor) are
  // supposed to be called from the main thread.
  PrintingTestHelper();
  ~PrintingTestHelper();

  // Does the necessary setup; intended to be used from SetUpOnMainThread().
  void Init(Profile* profile);

  // Adds a printer with the given `printer_id`, `printer_display_name` and
  // `capabilities` to the printers manager and the test backend.
  void AddAvailablePrinter(
      const std::string& printer_id,
      const std::string& printer_display_name,
      std::unique_ptr<printing::PrinterSemanticCapsAndDefaults> capabilities);

  PrintingBackendInfrastructureHelper& printing_infra_helper() {
    return *printing_infra_helper_;
  }

 private:
  // Creates test factories for ash::TestCupsPrintJobManager and
  // ash::FakeCupsPrintersManager.
  void OnWillCreateBrowserContextServices(content::BrowserContext* context);

  raw_ptr<Profile> profile_ = nullptr;

  base::CallbackListSubscription create_services_subscription_;

  std::unique_ptr<PrintingBackendInfrastructureHelper> printing_infra_helper_;
};
#endif

// Creates a printing extension with the correct manifest for the given `type`.
std::unique_ptr<TestExtensionDir> CreatePrintingExtension(ExtensionType type);

// Constructs a printer with some predefined capabilities.
std::unique_ptr<printing::PrinterSemanticCapsAndDefaults>
ConstructPrinterCapabilities();

// Constructs a response to LocalPrinter::GetPrinters() with a single printer.
std::vector<crosapi::mojom::LocalDestinationInfoPtr>
ConstructGetPrintersResponse(const std::string& printer_id,
                             const std::string& printer_name);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINTING_TEST_UTILS_H_
