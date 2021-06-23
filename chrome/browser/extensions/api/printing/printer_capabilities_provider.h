// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINTER_CAPABILITIES_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINTER_CAPABILITIES_PROVIDER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/containers/mru_cache.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
class CupsPrintersManager;
class Printer;
}  // namespace chromeos

namespace printing {
struct PrinterSemanticCapsAndDefaults;
}  // namespace printing

namespace extensions {

// Provides the capabilities for printers installed in Chrome OS.
// Is used for chrome.printing API handling.
class PrinterCapabilitiesProvider {
 public:
  using GetPrinterCapabilitiesCallback = base::OnceCallback<void(
      absl::optional<printing::PrinterSemanticCapsAndDefaults> capabilities)>;

  PrinterCapabilitiesProvider(
      chromeos::CupsPrintersManager* printers_manager,
      std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer);
  ~PrinterCapabilitiesProvider();

  void GetPrinterCapabilities(const std::string& printer_id,
                              GetPrinterCapabilitiesCallback callback);

 private:
  using PrinterCapabilitiesCache =
      base::MRUCache<std::string, printing::PrinterSemanticCapsAndDefaults>;

  void OnPrinterInstalled(const chromeos::Printer& printer,
                          GetPrinterCapabilitiesCallback callback,
                          chromeos::PrinterSetupResult result);

  void FetchCapabilities(const std::string& printer_id,
                         GetPrinterCapabilitiesCallback callback);

  void OnCapabilitiesFetched(
      const std::string& printer_id,
      GetPrinterCapabilitiesCallback callback,
      absl::optional<printing::PrinterSemanticCapsAndDefaults> capabilities);

  chromeos::CupsPrintersManager* const printers_manager_;
  std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer_;

  // Stores mapping from printer id to cached printer capabilities.
  PrinterCapabilitiesCache printer_capabilities_cache_;

  base::WeakPtrFactory<PrinterCapabilitiesProvider> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINTER_CAPABILITIES_PROVIDER_H_
