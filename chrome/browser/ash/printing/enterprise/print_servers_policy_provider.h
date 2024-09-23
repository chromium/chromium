// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_PRINT_SERVERS_POLICY_PROVIDER_H_
#define CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_PRINT_SERVERS_POLICY_PROVIDER_H_

#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/printing/print_server.h"
#include "chrome/browser/ash/printing/enterprise/print_servers_provider.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace ash {

using ServerPrintersFetchingMode =
    crosapi::mojom::PrintServersConfig::ServerPrintersFetchingMode;

// This class observes values provided by the DeviceExternalPrintServers and
// ExternalPrintServers policies and calculates resultant list of available
// print servers. This list is propagated to the provided callback.
class PrintServersPolicyProvider : public KeyedService,
                                   public PrintServersProvider::Observer {
 public:
  PrintServersPolicyProvider(
      base::WeakPtr<PrintServersProvider> user_policy_provider,
      base::WeakPtr<PrintServersProvider> device_policy_provider);

  ~PrintServersPolicyProvider() override;

  using OnPrintServersChanged = typename base::RepeatingCallback<
      void(bool, std::map<GURL, PrintServer>, ServerPrintersFetchingMode)>;

  static std::unique_ptr<PrintServersPolicyProvider> Create(Profile* profile);

  static std::unique_ptr<PrintServersPolicyProvider> CreateForTesting(
      base::WeakPtr<PrintServersProvider> user_policy_provider,
      base::WeakPtr<PrintServersProvider> device_policy_provider);

  // Set the callback when print servers has been updated via policy.
  void SetListener(OnPrintServersChanged callback);

  // PrintServersProvider::Observer
  void OnServersChanged(
      bool unused_complete,
      const std::vector<PrintServer>& unused_servers) override;

 private:
  void RecalculateServersAndNotifyListener();

  ServerPrintersFetchingMode GetFetchingMode(
      const std::map<GURL, PrintServer>& all_servers);

  base::WeakPtr<PrintServersProvider> user_policy_provider_;
  base::WeakPtr<PrintServersProvider> device_policy_provider_;

  std::map<GURL, PrintServer> all_servers_;

  OnPrintServersChanged callback_;

  base::WeakPtrFactory<PrintServersPolicyProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_ENTERPRISE_PRINT_SERVERS_POLICY_PROVIDER_H_
