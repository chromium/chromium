// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_test_utils.h"

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"

namespace extensions {

lorgnette::ScannerInfo CreateTestScannerInfo() {
  lorgnette::ScannerInfo scanner;
  scanner.set_name("scanneridabc123");
  scanner.set_display_name("GoogleTest Scanner");
  scanner.set_manufacturer("GoogleTest");
  scanner.set_model("Scanner");
  scanner.set_protocol_type("Mopria");
  return scanner;
}

lorgnette::ScannerOption CreateTestScannerOption(std::string_view name,
                                                 int32_t val) {
  lorgnette::ScannerOption option;
  option.set_name(std::string(name));
  option.set_title(base::StrCat({name, " title"}));
  option.set_description(base::StrCat({name, " description"}));
  option.set_option_type(lorgnette::OptionType::TYPE_INT);
  option.mutable_int_value()->add_value(val);
  option.set_active(true);
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
