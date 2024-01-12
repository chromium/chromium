// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_CUPS_PROXY_SERVICE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_PRINTING_CUPS_PROXY_SERVICE_DELEGATE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/printing/printer_configurer.h"
#include "chrome/services/cups_proxy/cups_proxy_service_delegate.h"
#include "chromeos/printing/printer_configuration.h"

class Profile;

namespace ash {

class CupsPrintersManager;
class PrinterConfigurer;

// This delegate implementation grants the CupsProxyService access to its chrome
// printing stack dependencies, i.e. PrinterConfigurer & CupsPrintersManager.
// This class can be created and sequenced anywhere but must be accessed from a
// sequenced context.
class CupsProxyServiceDelegateImpl
    : public cups_proxy::CupsProxyServiceDelegate {
 public:
  explicit CupsProxyServiceDelegateImpl(Profile* profile);
  ~CupsProxyServiceDelegateImpl() override;

  bool IsPrinterAccessAllowed() const override;

  // Look for a printer with the given id in any class.  Returns a copy of the
  // printer if found, nullptr otherwise.
  std::optional<chromeos::Printer> GetPrinter(const std::string& id) override;

  // Get the currently known list of printers.
  std::vector<chromeos::Printer> GetPrinters(
      chromeos::PrinterClass printer_class) override;

  // Get the recently used printer list from print_preview_sticky_settings.
  std::vector<std::string> GetRecentlyUsedPrinters() override;

  // Returns whether |printer| is currently installed in CUPS with this config.
  bool IsPrinterInstalled(const chromeos::Printer& printer) override;

  // Returns an IO-thread task runner.
  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() override;

  // Install |printer| into CUPS.
  void SetupPrinter(const chromeos::Printer& printer,
                    cups_proxy::SetupPrinterCallback cb) override;

 private:
  // Conducts SetupPrinter call on UI thread.
  void SetupPrinterOnUIThread(const chromeos::Printer& printer,
                              cups_proxy::SetupPrinterCallback cb);
  void OnSetupPrinter(cups_proxy::SetupPrinterCallback cb,
                      PrinterSetupResult result);

  // Current/active Profile. Not owned.
  const raw_ptr<Profile> profile_;

  // Handle to a CupsPrintersManager associated with profile_. Not owned.
  const raw_ptr<CupsPrintersManager> printers_manager_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CupsProxyServiceDelegateImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_CUPS_PROXY_SERVICE_DELEGATE_IMPL_H_
