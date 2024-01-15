// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_

#include <optional>

#include "chrome/common/extensions/api/document_scan.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

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
      std::optional<api::document_scan::ScanResults> scan_result,
      std::optional<std::string> error);
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

class DocumentScanGetOptionGroupsFunction : public ExtensionFunction {
 public:
  DocumentScanGetOptionGroupsFunction();
  DocumentScanGetOptionGroupsFunction(
      const DocumentScanGetOptionGroupsFunction&) = delete;
  DocumentScanGetOptionGroupsFunction& operator=(
      const DocumentScanGetOptionGroupsFunction&) = delete;

 protected:
  ~DocumentScanGetOptionGroupsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnResponseReceived(api::document_scan::GetOptionGroupsResponse response);
  DECLARE_EXTENSION_FUNCTION("documentScan.getOptionGroups",
                             DOCUMENTSCAN_GETOPTIONGROUPS)
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

class DocumentScanSetOptionsFunction : public ExtensionFunction {
 public:
  DocumentScanSetOptionsFunction();
  DocumentScanSetOptionsFunction(const DocumentScanSetOptionsFunction&) =
      delete;
  DocumentScanSetOptionsFunction& operator=(
      const DocumentScanSetOptionsFunction&) = delete;

 protected:
  ~DocumentScanSetOptionsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnResponseReceived(api::document_scan::SetOptionsResponse response);
  DECLARE_EXTENSION_FUNCTION("documentScan.setOptions", DOCUMENTSCAN_SETOPTIONS)
};

class DocumentScanStartScanFunction : public ExtensionFunction {
 public:
  DocumentScanStartScanFunction();
  DocumentScanStartScanFunction(const DocumentScanStartScanFunction&) = delete;
  DocumentScanStartScanFunction& operator=(
      const DocumentScanStartScanFunction&) = delete;

 protected:
  ~DocumentScanStartScanFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnResponseReceived(api::document_scan::StartScanResponse response);
  DECLARE_EXTENSION_FUNCTION("documentScan.startScan", DOCUMENTSCAN_STARTSCAN)
};

class DocumentScanCancelScanFunction : public ExtensionFunction {
 public:
  DocumentScanCancelScanFunction();
  DocumentScanCancelScanFunction(const DocumentScanCancelScanFunction&) =
      delete;
  DocumentScanCancelScanFunction& operator=(
      const DocumentScanCancelScanFunction&) = delete;

 protected:
  ~DocumentScanCancelScanFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnResponseReceived(api::document_scan::CancelScanResponse response);
  DECLARE_EXTENSION_FUNCTION("documentScan.cancelScan", DOCUMENTSCAN_CANCELSCAN)
};

class DocumentScanReadScanDataFunction : public ExtensionFunction {
 public:
  DocumentScanReadScanDataFunction();
  DocumentScanReadScanDataFunction(const DocumentScanReadScanDataFunction&) =
      delete;
  DocumentScanReadScanDataFunction& operator=(
      const DocumentScanReadScanDataFunction&) = delete;

 protected:
  ~DocumentScanReadScanDataFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnResponseReceived(api::document_scan::ReadScanDataResponse response);
  DECLARE_EXTENSION_FUNCTION("documentScan.readScanData",
                             DOCUMENTSCAN_READSCANDATA)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_
