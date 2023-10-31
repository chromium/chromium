// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINTING_TEST_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINTING_TEST_UTILS_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

class Profile;

namespace ash {
class TestCupsPrintJobManager;
class FakeCupsPrintersManager;
}  // namespace ash

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

class PrintingTestHelper {
 public:
  // BrowserContextDependencyManager subscriptions should be established before
  // the profile becomes available; for this reason `Profile*` is not provided
  // as a constructor parameter but rather pass in `Init()`.
  PrintingTestHelper();
  ~PrintingTestHelper();

  // Does the necessary setup; intended to be used from SetUpOnMainThread().
  void Init(Profile* profile);

  // No-op unless Init() is called.
  ash::TestCupsPrintJobManager* GetPrintJobManager();

  // No-op unless Init() is called.
  ash::FakeCupsPrintersManager* GetPrintersManager();

  // Adds a printer with the given `printer_id` and `capabilities` to the
  // printers manager and the test backend.
  void AddAvailablePrinter(
      const std::string& printer_id,
      std::unique_ptr<printing::PrinterSemanticCapsAndDefaults> capabilities);

 private:
  // Creates test factories for ash::TestCupsPrintJobManager and
  // ash::FakeCupsPrintersManager.
  void OnWillCreateBrowserContextServices(content::BrowserContext* context);

  raw_ptr<Profile> profile_ = nullptr;

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  mojo::Remote<printing::mojom::PrintBackendService> test_remote_;
  std::unique_ptr<printing::PrintBackendServiceTestImpl> print_backend_service_;
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  base::CallbackListSubscription create_services_subscription_;

  scoped_refptr<printing::TestPrintBackend> test_print_backend_;
};

// Creates a printing extension with the correct manifest for the given `type`.
std::unique_ptr<TestExtensionDir> CreatePrintingExtension(ExtensionType type);

// Constructs a printer with some predefined capabilities.
std::unique_ptr<printing::PrinterSemanticCapsAndDefaults>
ConstructPrinterCapabilities();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINTING_TEST_UTILS_H_
