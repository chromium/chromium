// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_UTIL_H_
#define CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_UTIL_H_

#include "chrome/browser/support_tool/support_tool_handler.h"

// Returns SupportToolHandler that is created for collecting logs from the
// given data modules. Adds all Chrome OS related DataCollectors to the
// SupportToolHandler if `chrome_os` is true. Adds all Chrome browser
// DataCollectors if `chrome_browser` is true.
std::unique_ptr<SupportToolHandler> GetSupportToolHandler(bool chrome_os,
                                                          bool chrome_browser);

#endif  // CHROME_BROWSER_SUPPORT_TOOL_SUPPORT_TOOL_UTIL_H_
