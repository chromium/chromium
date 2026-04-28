// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/fake_document_scan_ash.h"

#include <utility>

#include "base/check.h"
#include "base/notimplemented.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/document_scan/document_scan_test_utils.h"

namespace extensions {

FakeDocumentScanAsh::FakeDocumentScanAsh() = default;
FakeDocumentScanAsh::~FakeDocumentScanAsh() = default;

FakeDocumentScanAsh::OpenScannerState::OpenScannerState() = default;
FakeDocumentScanAsh::OpenScannerState::~OpenScannerState() = default;

FakeDocumentScanAsh::OpenScannerState::OpenScannerState(
    const std::string& client_id,
    const std::string& connection_string)
    : client_id(client_id), connection_string(connection_string) {}

void FakeDocumentScanAsh::CloseScanner(const std::string& scanner_handle) {
  open_scanners_.erase(scanner_handle);
}

void FakeDocumentScanAsh::SetOpenScannerResponse(
    const std::string& connection_string,
    crosapi::mojom::OpenScannerResponsePtr response) {
  open_responses_[connection_string] = std::move(response);
}

}  // namespace extensions
