// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SCANNING_SCAN_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_SCANNING_SCAN_SERVICE_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "chromeos/components/scanning/mojom/scanning.mojom.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

class LorgnetteScannerManager;

// Implementation of the chromeos::scanning::mojom::ScanService interface. Used
// by the scanning WebUI (chrome://scanning) to get connected scanners, obtain
// scanner capabilities, and perform scans.
class ScanService : public scanning::mojom::ScanService, public KeyedService {
 public:
  explicit ScanService(LorgnetteScannerManager* lorgnette_scanner_manager);
  ~ScanService() override;

  ScanService(const ScanService&) = delete;
  ScanService& operator=(const ScanService&) = delete;

  // scanning::mojom::ScanService:
  void GetScanners(GetScannersCallback callback) override;
  void GetScannerCapabilities(const base::UnguessableToken& scanner_id,
                              GetScannerCapabilitiesCallback callback) override;

  // Binds receiver_ by consuming |pending_receiver|.
  void BindInterface(
      mojo::PendingReceiver<scanning::mojom::ScanService> pending_receiver);

 private:
  // KeyedService:
  void Shutdown() override;

  // Processes the result of calling LorgnetteScannerManager::GetScannerNames().
  void OnScannerNamesReceived(GetScannersCallback callback,
                              std::vector<std::string> scanner_names);

  // Processes the result of calling
  // LorgnetteScannerManager::GetScannerCapabilities().
  void OnScannerCapabilitiesReceived(
      GetScannerCapabilitiesCallback callback,
      const base::Optional<lorgnette::ScannerCapabilities>& capabilities);

  // Map of scanner IDs to display names. Used to pass the correct display name
  // to LorgnetteScannerManager when clients provide an ID.
  base::flat_map<base::UnguessableToken, std::string> scanner_names_;

  // Receives and dispatches method calls to this implementation of the
  // chromeos::scanning::mojom::ScanService interface.
  mojo::Receiver<scanning::mojom::ScanService> receiver_{this};

  // Unowned. Used to get scanner information and perform scans.
  LorgnetteScannerManager* lorgnette_scanner_manager_;

  base::WeakPtrFactory<ScanService> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SCANNING_SCAN_SERVICE_H_
