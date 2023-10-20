// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_

#include "chrome/common/extensions/api/document_scan.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

class DocumentScanScanFunction : public ExtensionFunction {
 public:
  DocumentScanScanFunction();
  DocumentScanScanFunction(const DocumentScanScanFunction&) = delete;
  DocumentScanScanFunction& operator=(const DocumentScanScanFunction&) = delete;

 protected:
  ~DocumentScanScanFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnScanCompleted(
      absl::optional<api::document_scan::ScanResults> scan_result,
      absl::optional<std::string> error);
  DECLARE_EXTENSION_FUNCTION("documentScan.scan", DOCUMENT_SCAN_SCAN)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_
