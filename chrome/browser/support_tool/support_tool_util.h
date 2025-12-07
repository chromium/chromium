// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_UTIL_H_
#define CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_UTIL_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/support_tool_handler.h"

class Profile;

// Returns SupportToolHandler that is created for collecting logs from the
// given information. Adds the corresponding DataCollectors that were listed in
// `included_data_collectors` to the returned SupportToolHandler. Callers can
// attach an optional `upload_id` to attach to the support packet.
std::unique_ptr<SupportToolHandler> GetSupportToolHandler(
    std::string case_id,
    std::string email_address,
    std::string issue_description,
    std::optional<std::string> upload_id,
    Profile* profile,
    std::set<support_tool::DataCollectorType> included_data_collectors);

std::vector<support_tool::DataCollectorType> GetAllDataCollectors();

std::vector<support_tool::DataCollectorType>
GetAllAvailableDataCollectorsOnDevice();

// Returns a filepath in `target_directory` to export the support packet into.
// The returned filename will be in format of
// <filename_prefix>_<case_id>_UTCYYYYMMDD_HHmm. `case_id` will not be included
// if it's empty.
base::FilePath GetFilepathToExport(base::FilePath target_directory,
                                   const std::string& filename_prefix,
                                   const std::string& case_id,
                                   base::Time timestamp);

// Returns the string representation of support tool errors.
std::string SupportToolErrorsToString(const std::set<SupportToolError>& errors);

#endif  // CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_UTIL_H_
