// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_UTIL_H_
#define CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_UTIL_H_

#include <set>
#include <vector>

#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/support_tool_handler.h"

class Profile;

// Returns SupportToolHandler that is created for collecting logs from the
// given information. Adds the corresponding DataCollectors that were listed in
// `included_data_collectors` to the returned SupportToolHandler.
std::unique_ptr<SupportToolHandler> GetSupportToolHandler(
    std::string case_id,
    std::string email_address,
    std::string issue_description,
    Profile* profile,
    std::set<support_tool::DataCollectorType> included_data_collectors);

std::vector<support_tool::DataCollectorType> GetAllDataCollectors();

std::vector<support_tool::DataCollectorType>
GetAllAvailableDataCollectorsOnDevice();

#endif  // CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_UTIL_H_
