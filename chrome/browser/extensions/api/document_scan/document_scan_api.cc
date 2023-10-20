// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_api.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/extensions/api/document_scan/document_scan_api_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  if (!user_gesture())
    return RespondNow(Error(kUserGestureRequiredError));

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
    absl::optional<api::document_scan::ScanResults> scan_results,
    absl::optional<std::string> error) {
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

}  // namespace extensions
