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
  DECLARE_EXTENSION_FUNCTION("documentScan.scan", DOCUMENTSCAN_SCAN)
};

class DocumentScanGetScannerListFunction : public ExtensionFunction {
 public:
  DocumentScanGetScannerListFunction();
  DocumentScanGetScannerListFunction(
      const DocumentScanGetScannerListFunction&) = delete;
  DocumentScanGetScannerListFunction& operator=(
      const DocumentScanGetScannerListFunction&) = delete;

 protected:
  ~DocumentScanGetScannerListFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnScannerListReceived(
      api::document_scan::GetScannerListResponse response);
  DECLARE_EXTENSION_FUNCTION("documentScan.getScannerList",
                             DOCUMENTSCAN_GETSCANNERLIST)
};

class DocumentScanOpenScannerFunction : public ExtensionFunction {
 public:
  DocumentScanOpenScannerFunction();
  DocumentScanOpenScannerFunction(const DocumentScanOpenScannerFunction&) =
      delete;
  DocumentScanOpenScannerFunction& operator=(
      const DocumentScanOpenScannerFunction&) = delete;

 protected:
  ~DocumentScanOpenScannerFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnResponseReceived(api::document_scan::OpenScannerResponse response);
  DECLARE_EXTENSION_FUNCTION("documentScan.openScanner",
                             DOCUMENTSCAN_OPENSCANNER)
};

class DocumentScanCloseScannerFunction : public ExtensionFunction {
 public:
  DocumentScanCloseScannerFunction();
  DocumentScanCloseScannerFunction(const DocumentScanCloseScannerFunction&) =
      delete;
  DocumentScanCloseScannerFunction& operator=(
      const DocumentScanCloseScannerFunction&) = delete;

 protected:
  ~DocumentScanCloseScannerFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnResponseReceived(api::document_scan::CloseScannerResponse response);
  DECLARE_EXTENSION_FUNCTION("documentScan.closeScanner",
                             DOCUMENTSCAN_CLOSESCANNER)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_
