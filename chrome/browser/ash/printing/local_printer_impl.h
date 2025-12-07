// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_LOCAL_PRINTER_IMPL_H_
#define CHROME_BROWSER_ASH_PRINTING_LOCAL_PRINTER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ash/printing/local_printer.h"

namespace ash {

class LocalPrinterImpl : public LocalPrinter {
 public:
  static void Initialize();
  static LocalPrinter* Get();

  LocalPrinterImpl();
  LocalPrinterImpl(const LocalPrinterImpl&) = delete;
  LocalPrinterImpl& operator=(const LocalPrinterImpl&) = delete;
  ~LocalPrinterImpl() override;

  // LocalPrinter override:
  void GetPrinters(const AccountId& accountId,
                   GetPrintersCallback callback) override;
  void GetCapability(const AccountId& accountId,
                     const std::string& printer_id,
                     GetCapabilityCallback callback) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_LOCAL_PRINTER_IMPL_H_
