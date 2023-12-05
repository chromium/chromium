// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DOCUMENT_SCAN_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DOCUMENT_SCAN_ASH_H_

#include "chromeos/crosapi/mojom/document_scan.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi interface for DocumentScan. Lives in Ash-Chrome on the
// UI thread.
class DocumentScanAsh : public mojom::DocumentScan {
 public:
  DocumentScanAsh();
  DocumentScanAsh(const DocumentScanAsh&) = delete;
  DocumentScanAsh& operator=(const DocumentScanAsh&) = delete;
  ~DocumentScanAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::DocumentScan> receiver);

  // crosapi::mojom::DocumentScan:
  void GetScannerNames(GetScannerNamesCallback callback) override;
  void ScanFirstPage(const std::string& scanner_name,
                     ScanFirstPageCallback callback) override;
  void GetScannerList(const std::string& client_id,
                      mojom::ScannerEnumFilterPtr filter,
                      GetScannerListCallback callback) override;
  void OpenScanner(const std::string& client_id,
                   const std::string& scanner_id,
                   OpenScannerCallback callback) override;
  void CloseScanner(const std::string& scanner_handle,
                    CloseScannerCallback callback) override;
  void StartPreparedScan(const std::string& scanner_handle,
                         mojom::StartScanOptionsPtr options,
                         StartPreparedScanCallback callback) override;
  void ReadScanData(const std::string& job_handle,
                    ReadScanDataCallback callback) override;
  void SetOptions(const std::string& scanner_handle,
                  std::vector<mojom::OptionSettingPtr> options,
                  SetOptionsCallback callback) override;
  void GetOptionGroups(const std::string& scanner_handle,
                       GetOptionGroupsCallback callback) override;
  void CancelScan(const std::string& job_handle,
                  CancelScanCallback callback) override;

 private:
  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::DocumentScan> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DOCUMENT_SCAN_ASH_H_
