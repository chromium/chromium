// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_test_utils.h"

namespace extensions {

crosapi::mojom::ScannerInfoPtr CreateTestScannerInfo() {
  auto scanner = crosapi::mojom::ScannerInfo::New();
  scanner->id = "test:scanner:1234";
  scanner->display_name = "GoogleTest Scanner";
  scanner->manufacturer = "GoogleTest";
  scanner->model = "Scanner";
  return scanner;
}

crosapi::mojom::ScannerOptionPtr CreateTestScannerOption(
    const std::string& name,
    int32_t val) {
  auto option = crosapi::mojom::ScannerOption::New();
  option->name = name;
  option->title = name + " title";
  option->description = name + " description";
  option->value = crosapi::mojom::OptionValue::NewIntValue(val);
  option->isActive = true;
  return option;
}

}  // namespace extensions
