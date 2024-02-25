// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_test_utils.h"

#include "base/strings/stringprintf.h"

namespace extensions {

crosapi::mojom::ScannerInfoPtr CreateTestScannerInfo() {
  auto scanner = crosapi::mojom::ScannerInfo::New();
  scanner->id = "scanneridabc123";
  scanner->display_name = "GoogleTest Scanner";
  scanner->manufacturer = "GoogleTest";
  scanner->model = "Scanner";
  scanner->protocol_type = "Mopria";
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

std::vector<api::document_scan::OptionSetting> CreateTestOptionSettingList(
    size_t num,
    api::document_scan::OptionType type) {
  std::vector<api::document_scan::OptionSetting> settings;
  settings.reserve(num);

  // Loop from 1 to create 1-based option names.
  for (size_t i = 1; i <= num; i++) {
    api::document_scan::OptionSetting setting;
    setting.name = base::StringPrintf("option%zu", i);
    setting.type = type;
    settings.emplace_back(std::move(setting));
  }

  return settings;
}

}  // namespace extensions
