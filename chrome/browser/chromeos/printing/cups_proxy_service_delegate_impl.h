// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PROXY_SERVICE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PROXY_SERVICE_DELEGATE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/sequence_checker.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/services/cups_proxy/cups_proxy_service_delegate.h"
#include "chromeos/printing/printer_configuration.h"

#include "base/task/post_task.h"

class Profile;

namespace chromeos {

class CupsPrintersManager;
class PrinterConfigurer;

// This delegate implementation grants the CupsProxyService access to its chrome
// printing stack dependencies, i.e. PrinterConfigurer & CupsPrintersManager.
// This class can be created and sequenced anywhere but must be accessed from a
// sequenced context.
class CupsProxyServiceDelegateImpl
    : public cups_proxy::CupsProxyServiceDelegate {
 public:
  CupsProxyServiceDelegateImpl();
  ~CupsProxyServiceDelegateImpl() override;

  // Look for a printer with the given id in any class.  Returns a copy of the
  // printer if found, nullptr otherwise.
  base::Optional<Printer> GetPrinter(const std::string& id) override;

  // Get the currently known list of printers.
  std::vector<Printer> GetPrinters(PrinterClass printer_class) override;

  // Returns whether |printer| is currently installed in CUPS with this config.
  bool IsPrinterInstalled(const Printer& printer) override;

  // Records that |printer| has been installed into CUPS with this config.
  void PrinterInstalled(const Printer& printer) override;

  // Returns an IO-thread task runner.
  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() override;

  // Install |printer| into CUPS.
  void SetupPrinter(const Printer& printer,
                    cups_proxy::SetupPrinterCallback cb) override;

 private:
  // Conducts SetupPrinter call on UI thread.
  void SetupPrinterOnThread(const Printer& printer,
                            scoped_refptr<base::SequencedTaskRunner> cb_runner,
                            cups_proxy::SetupPrinterCallback cb);
  void OnSetupPrinter(scoped_refptr<base::SequencedTaskRunner> cb_runner,
                      cups_proxy::SetupPrinterCallback cb,
                      PrinterSetupResult result);

  // Current/active Profile. Not owned.
  Profile* const profile_;

  // Handle to a CupsPrintersManager associated with profile_. Not owned.
  CupsPrintersManager* const printers_manager_;

  // Handle to the PrinterConfigurer associated with profile_.
  // Must be created/accessed on the UI thread.
  std::unique_ptr<PrinterConfigurer> printer_configurer_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CupsProxyServiceDelegateImpl> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PROXY_SERVICE_DELEGATE_IMPL_H_
