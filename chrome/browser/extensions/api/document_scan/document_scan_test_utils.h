// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_TEST_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_TEST_UTILS_H_

#include <string>
#include <vector>

#include "chrome/common/extensions/api/document_scan.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"

namespace extensions {

lorgnette::ScannerInfo CreateTestScannerInfo();
lorgnette::ScannerOption CreateTestScannerOption(std::string_view name,
                                                 std::int32_t val);
std::vector<api::document_scan::OptionSetting> CreateTestOptionSettingList(
    size_t num,
    api::document_scan::OptionType type);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_TEST_UTILS_H_
