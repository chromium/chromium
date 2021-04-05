// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "chrome/common/extensions/api/document_scan.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

namespace api {

class DocumentScanScanFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("documentScan.scan", DOCUMENT_SCAN_SCAN)
  DocumentScanScanFunction();
  DocumentScanScanFunction(const DocumentScanScanFunction&) = delete;
  DocumentScanScanFunction& operator=(const DocumentScanScanFunction&) = delete;

 protected:
  ~DocumentScanScanFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  friend class DocumentScanScanFunctionTest;

  void OnNamesReceived(std::vector<std::string> scanner_names);
  void OnPageReceived(std::string scanned_image, uint32_t /*page_number*/);
  void OnScanCompleted(bool success,
                       lorgnette::ScanFailureMode /*failure_mode*/);

  base::Optional<std::string> scan_data_;
  std::unique_ptr<document_scan::Scan::Params> params_;
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_
