// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_LOCAL_PRINTER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_LOCAL_PRINTER_ASH_H_

#include <string>
#include <vector>

#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi interface for LocalPrinter. Lives in Ash-Chrome on the
// UI thread.
class LocalPrinterAsh : public mojom::LocalPrinter {
 public:
  LocalPrinterAsh();
  LocalPrinterAsh(const LocalPrinterAsh&) = delete;
  LocalPrinterAsh& operator=(const LocalPrinterAsh&) = delete;
  ~LocalPrinterAsh() override;

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
  void AddObserver(mojo::PendingRemote<mojom::PrintServerObserver> remote,
                   AddObserverCallback callback) override;
  void GetPolicies(GetPoliciesCallback callback) override;

 private:
  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::LocalPrinter> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_LOCAL_PRINTER_ASH_H_
