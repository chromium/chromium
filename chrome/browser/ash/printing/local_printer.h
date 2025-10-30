// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_LOCAL_PRINTER_H_
#define CHROME_BROWSER_ASH_PRINTING_LOCAL_PRINTER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "chromeos/printing/printer_configuration.h"
#include "printing/backend/print_backend.h"

class AccountId;

namespace ash {

// A utility class for managing local printers.
class LocalPrinter {
 public:
  using GetPrintersCallback =
      base::OnceCallback<void(std::vector<chromeos::Printer>)>;
  using GetCapabilityCallback = base::OnceCallback<void(
      const std::optional<::printing::PrinterSemanticCapsAndDefaults>&)>;

  virtual ~LocalPrinter() = default;

  // Gets a list of printers.
  virtual void GetPrinters(const AccountId& accountId,
                           GetPrintersCallback callback) = 0;

  // Gets capabilities for a printer as a PrinterSemanticCapsAndDefaults
  // object.
  virtual void GetCapability(const AccountId& accountId,
                             const std::string& printer_id,
                             GetCapabilityCallback callback) = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_LOCAL_PRINTER_H_
