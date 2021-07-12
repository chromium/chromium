// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_LOCAL_PRINTER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_LOCAL_PRINTER_ASH_H_

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class Profile;

namespace chromeos {
class CupsPrinterStatus;
class PpdProvider;
class Printer;
class PrinterConfigurer;
struct PrintServersConfig;
}  // namespace chromeos

namespace crosapi {

// Implements the crosapi interface for LocalPrinter. Lives in Ash-Chrome on the
// UI thread.
class LocalPrinterAsh : public mojom::LocalPrinter {
 public:
  LocalPrinterAsh();
  LocalPrinterAsh(const LocalPrinterAsh&) = delete;
  LocalPrinterAsh& operator=(const LocalPrinterAsh&) = delete;
  ~LocalPrinterAsh() override;

  // The mojom PrintServersConfig object contains all information in the
  // PrintServersConfig object.
  static mojom::PrintServersConfigPtr ConfigToMojom(
      const chromeos::PrintServersConfig& config);

  // The mojom LocalDestinationInfo object is a subset of the chromeos Printer
  // object.
  static mojom::LocalDestinationInfoPtr PrinterToMojom(
      const chromeos::Printer& printer);

  // The mojom PrinterStatus object contains all information in the
  // CupsPrinterStatus object.
  static mojom::PrinterStatusPtr StatusToMojom(
      const chromeos::CupsPrinterStatus& status);

  void BindReceiver(mojo::PendingReceiver<mojom::LocalPrinter> receiver);

  // crosapi::mojom::LocalPrinter:
  void GetPrinters(GetPrintersCallback callback) override;
  void GetCapability(const std::string& printer_id,
                     GetCapabilityCallback callback) override;
  void GetEulaUrl(const std::string& printer_id,
                  GetEulaUrlCallback callback) override;
  void GetStatus(const std::string& printer_id,
                 GetStatusCallback callback) override;
  void ShowSystemPrintSettings(
      ShowSystemPrintSettingsCallback callback) override;
  void CreatePrintJob(mojom::PrintJobPtr job,
                      CreatePrintJobCallback callback) override;
  void GetPrintServersConfig(GetPrintServersConfigCallback callback) override;
  void ChoosePrintServers(const std::vector<std::string>& print_server_ids,
                          ChoosePrintServersCallback callback) override;
  void AddPrintServerObserver(
      mojo::PendingRemote<mojom::PrintServerObserver> remote,
      AddPrintServerObserverCallback callback) override;
  void GetPolicies(GetPoliciesCallback callback) override;
  void GetUsernamePerPolicy(GetUsernamePerPolicyCallback callback) override;
  void GetPrinterTypeDenyList(GetPrinterTypeDenyListCallback callback) override;

 private:
  // Exposed so that unit tests can override them.
  virtual Profile* GetActiveUserProfile();
  virtual scoped_refptr<chromeos::PpdProvider> CreatePpdProvider(
      Profile* profile);
  virtual std::unique_ptr<chromeos::PrinterConfigurer> CreatePrinterConfigurer(
      Profile* profile);

  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::LocalPrinter> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_LOCAL_PRINTER_ASH_H_
