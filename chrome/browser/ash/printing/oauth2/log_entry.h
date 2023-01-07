// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_LOG_ENTRY_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_LOG_ENTRY_H_

#include <string>

#include "base/strings/string_piece.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chromeos/printing/uri.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash::printing::oauth2 {

// Builds a single log entry that can be added to chrome://device-log.
// Created log entries have prefix "oauth ".
// Parameters:
//  * `message` - free-text message, is omitted from the output when empty;
//  * `method` - the name of the method called, use __func__ here;
//  * `auth_server` - the URL of the Authorization Server (if applicable);
//  * `status` - the result returned by the method or absl::nullopt if
//               the method is still being executed;
//  * `ipp_endpoint` - the URL of the IPP Endpoint (if applicable).
std::string LogEntry(base::StringPiece message,
                     base::StringPiece method,
                     const GURL& auth_server,
                     absl::optional<StatusCode> status = absl::nullopt,
                     const chromeos::Uri& ipp_endpoint = chromeos::Uri());

}  // namespace ash::printing::oauth2

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_LOG_ENTRY_H_
