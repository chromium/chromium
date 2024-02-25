// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_LOG_ENTRY_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_LOG_ENTRY_H_

#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chromeos/printing/uri.h"
#include "url/gurl.h"

namespace ash::printing::oauth2 {

// Builds a single log entry that can be added to chrome://device-log.
// Created log entries have prefix "oauth ".
// Parameters:
//  * `message` - free-text message, is omitted from the output when empty;
//  * `method` - the name of the method called, use __func__ here;
//  * `auth_server` - the URL of the Authorization Server (if applicable);
//  * `status` - the result returned by the method or std::nullopt if
//               the method is still being executed;
//  * `ipp_endpoint` - the URL of the IPP Endpoint (if applicable).
std::string LogEntry(std::string_view message,
                     std::string_view method,
                     const GURL& auth_server,
                     std::optional<StatusCode> status = std::nullopt,
                     const chromeos::Uri& ipp_endpoint = chromeos::Uri());

}  // namespace ash::printing::oauth2

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_LOG_ENTRY_H_
