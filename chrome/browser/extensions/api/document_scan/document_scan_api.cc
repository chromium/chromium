// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_api.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/extensions/api/document_scan/document_scan_api_handler.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/document_scan_ash.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

namespace extensions {

namespace {

// Error messages that can be included in a response when scanning fails.
constexpr char kUserGestureRequiredError[] =
    "User gesture required to perform scan";
constexpr char kScanImageError[] = "Failed to scan image";

}  // namespace

DocumentScanScanFunction::DocumentScanScanFunction() = default;
DocumentScanScanFunction::~DocumentScanScanFunction() = default;

ExtensionFunction::ResponseAction DocumentScanScanFunction::Run() {
  auto params = api::document_scan::Scan::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!user_gesture()) {
    return RespondNow(Error(kUserGestureRequiredError));
  }

  std::vector<std::string> mime_types;
  if (params->options.mime_types) {
    mime_types = std::move(*params->options.mime_types);
  }

  DocumentScanAPIHandler::Get(browser_context())
      ->SimpleScan(
          mime_types,
          base::BindOnce(&DocumentScanScanFunction::OnScanCompleted, this));

  return RespondLater();
}

void DocumentScanScanFunction::OnScanCompleted(
    std::optional<api::document_scan::ScanResults> scan_results,
    std::optional<std::string> error) {
  if (error) {
    Respond(Error(*error));
    return;
  }

  if (!scan_results) {
    Respond(Error(kScanImageError));
    return;
  }

  Respond(WithArguments(scan_results->ToValue()));
}

DocumentScanGetScannerListFunction::DocumentScanGetScannerListFunction() =
    default;
DocumentScanGetScannerListFunction::~DocumentScanGetScannerListFunction() =
    default;

ExtensionFunction::ResponseAction DocumentScanGetScannerListFunction::Run() {
  auto params = api::document_scan::GetScannerList::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  DocumentScanAPIHandler::Get(browser_context())
      ->GetScannerList(
          ChromeExtensionFunctionDetails(this).GetNativeWindowForUI(),
          extension_, user_gesture(), std::move(params->filter),
          base::BindOnce(
              &DocumentScanGetScannerListFunction::OnScannerListReceived,
              this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DocumentScanGetScannerListFunction::OnScannerListReceived(
    api::document_scan::GetScannerListResponse response) {
  Respond(ArgumentList(
      api::document_scan::GetScannerList::Results::Create(response)));
}

DocumentScanOpenScannerFunction::DocumentScanOpenScannerFunction() = default;
DocumentScanOpenScannerFunction::~DocumentScanOpenScannerFunction() = default;

ExtensionFunction::ResponseAction DocumentScanOpenScannerFunction::Run() {
  auto params = api::document_scan::OpenScanner::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  DocumentScanAPIHandler::Get(browser_context())
      ->OpenScanner(
          extension_, std::move(params->scanner_id),
          base::BindOnce(&DocumentScanOpenScannerFunction::OnResponseReceived,
                         this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DocumentScanOpenScannerFunction::OnResponseReceived(
    api::document_scan::OpenScannerResponse response) {
  Respond(
      ArgumentList(api::document_scan::OpenScanner::Results::Create(response)));
}

DocumentScanGetOptionGroupsFunction::DocumentScanGetOptionGroupsFunction() =
    default;
DocumentScanGetOptionGroupsFunction::~DocumentScanGetOptionGroupsFunction() =
    default;

ExtensionFunction::ResponseAction DocumentScanGetOptionGroupsFunction::Run() {
  auto params = api::document_scan::GetOptionGroups::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  DocumentScanAPIHandler::Get(browser_context())
      ->GetOptionGroups(
          extension_, std::move(params->scanner_handle),
          base::BindOnce(
              &DocumentScanGetOptionGroupsFunction::OnResponseReceived, this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DocumentScanGetOptionGroupsFunction::OnResponseReceived(
    api::document_scan::GetOptionGroupsResponse response) {
  Respond(ArgumentList(
      api::document_scan::GetOptionGroups::Results::Create(response)));
}

DocumentScanCloseScannerFunction::DocumentScanCloseScannerFunction() = default;
DocumentScanCloseScannerFunction::~DocumentScanCloseScannerFunction() = default;

ExtensionFunction::ResponseAction DocumentScanCloseScannerFunction::Run() {
  auto params = api::document_scan::CloseScanner::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  DocumentScanAPIHandler::Get(browser_context())
      ->CloseScanner(
          extension_, std::move(params->scanner_handle),
          base::BindOnce(&DocumentScanCloseScannerFunction::OnResponseReceived,
                         this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DocumentScanCloseScannerFunction::OnResponseReceived(
    api::document_scan::CloseScannerResponse response) {
  Respond(ArgumentList(
      api::document_scan::CloseScanner::Results::Create(response)));
}

DocumentScanSetOptionsFunction::DocumentScanSetOptionsFunction() = default;
DocumentScanSetOptionsFunction::~DocumentScanSetOptionsFunction() = default;

ExtensionFunction::ResponseAction DocumentScanSetOptionsFunction::Run() {
  auto params = api::document_scan::SetOptions::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  DocumentScanAPIHandler::Get(browser_context())
      ->SetOptions(
          extension_, std::move(params->scanner_handle),
          std::move(params->options),
          base::BindOnce(&DocumentScanSetOptionsFunction::OnResponseReceived,
                         this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DocumentScanSetOptionsFunction::OnResponseReceived(
    api::document_scan::SetOptionsResponse response) {
  Respond(
      ArgumentList(api::document_scan::SetOptions::Results::Create(response)));
}

DocumentScanStartScanFunction::DocumentScanStartScanFunction() = default;
DocumentScanStartScanFunction::~DocumentScanStartScanFunction() = default;

ExtensionFunction::ResponseAction DocumentScanStartScanFunction::Run() {
  auto params = api::document_scan::StartScan::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  DocumentScanAPIHandler::Get(browser_context())
      ->StartScan(
          ChromeExtensionFunctionDetails(this).GetNativeWindowForUI(),
          extension_, user_gesture(), std::move(params->scanner_handle),
          std::move(params->options),
          base::BindOnce(&DocumentScanStartScanFunction::OnResponseReceived,
                         this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DocumentScanStartScanFunction::OnResponseReceived(
    api::document_scan::StartScanResponse response) {
  Respond(
      ArgumentList(api::document_scan::StartScan::Results::Create(response)));
}

DocumentScanCancelScanFunction::DocumentScanCancelScanFunction() = default;
DocumentScanCancelScanFunction::~DocumentScanCancelScanFunction() = default;

ExtensionFunction::ResponseAction DocumentScanCancelScanFunction::Run() {
  auto params = api::document_scan::CancelScan::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  DocumentScanAPIHandler::Get(browser_context())
      ->CancelScan(
          extension_, std::move(params->job),
          base::BindOnce(&DocumentScanCancelScanFunction::OnResponseReceived,
                         this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DocumentScanCancelScanFunction::OnResponseReceived(
    api::document_scan::CancelScanResponse response) {
  Respond(
      ArgumentList(api::document_scan::CancelScan::Results::Create(response)));
}

DocumentScanReadScanDataFunction::DocumentScanReadScanDataFunction() = default;
DocumentScanReadScanDataFunction::~DocumentScanReadScanDataFunction() = default;

ExtensionFunction::ResponseAction DocumentScanReadScanDataFunction::Run() {
  auto params = api::document_scan::ReadScanData::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  DocumentScanAPIHandler::Get(browser_context())
      ->ReadScanData(
          extension_, std::move(params->job),
          base::BindOnce(&DocumentScanReadScanDataFunction::OnResponseReceived,
                         this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DocumentScanReadScanDataFunction::OnResponseReceived(
    api::document_scan::ReadScanDataResponse response) {
  Respond(ArgumentList(
      api::document_scan::ReadScanData::Results::Create(response)));
}

}  // namespace extensions
