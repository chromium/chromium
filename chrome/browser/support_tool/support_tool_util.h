// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_UTIL_H_
#define CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_UTIL_H_

#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/support_tool_handler.h"

class Profile;

// Data collector types that can work on every platform.
static constexpr support_tool::DataCollectorType kDataCollectors[] = {
    support_tool::CHROME_INTERNAL, support_tool::CRASH_IDS,
    support_tool::MEMORY_DETAILS, support_tool::POLICIES};

// Data collector types can only work on Chrome OS Ash.
static constexpr support_tool::DataCollectorType kDataCollectorsChromeosAsh[] =
    {support_tool::CHROMEOS_UI_HIERARCHY,
     support_tool::CHROMEOS_COMMAND_LINE,
     support_tool::CHROMEOS_DEVICE_EVENT,
     support_tool::CHROMEOS_IWL_WIFI_DUMP,
     support_tool::CHROMEOS_TOUCH_EVENTS,
     support_tool::CHROMEOS_CROS_API,
     support_tool::CHROMEOS_LACROS,
     support_tool::CHROMEOS_DBUS,
     support_tool::CHROMEOS_NETWORK_ROUTES,
     support_tool::CHROMEOS_SHILL,
     support_tool::CHROMEOS_SYSTEM_STATE,
     support_tool::CHROMEOS_SYSTEM_LOGS,
     support_tool::CHROMEOS_CHROME_USER_LOGS};

// Data collector types that can only work on if IS_CHROMEOS_WITH_HW_DETAILS
// flag is turned on. IS_CHROMEOS_WITH_HW_DETAILS flag will be turned on for
// Chrome OS Flex devices.
static constexpr support_tool::DataCollectorType
    kDataCollectorsChromeosHwDetails[] = {support_tool::CHROMEOS_REVEN};

// Returns SupportToolHandler that is created for collecting logs from the
// given information. Adds the corresponding DataCollectors that were listed in
// `included_data_collectors` to the returned SupportToolHandler.
std::unique_ptr<SupportToolHandler> GetSupportToolHandler(
    std::string case_id,
    std::string email_address,
    std::string issue_description,
    Profile* profile,
    std::set<support_tool::DataCollectorType> included_data_collectors);

#endif  // CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_UTIL_H_
