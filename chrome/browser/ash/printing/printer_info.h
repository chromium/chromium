// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINTER_INFO_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINTER_INFO_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "printing/printer_query_result.h"

namespace chromeos {
struct PrinterAuthenticationInfo;
}  // namespace chromeos

namespace printing {
struct PrinterStatus;
}  // namespace printing

namespace ash {

// Callback for basic printer information. |result| indicates if the request
// succeeded at all. |status| contains current status. |make_and_model| is
// the raw printer-make-and-model value from the printer. |autoconf| indicates
// if we think we can compute the printer capabilities without a PPD.
// |auth_info| holds the information about authentication required by the
// printer.
using PrinterInfoCallback = base::OnceCallback<void(
    ::printing::PrinterQueryResult result,
    const ::printing::PrinterStatus& status,
    const std::string& make_and_model,
    const std::vector<std::string>& document_formats,
    bool autoconf,
    const chromeos::PrinterAuthenticationInfo& auth_info)>;

// Dispatch an IPP request to |host| on |port| for |path| to obtain
// basic printer information.
void QueryIppPrinter(const std::string& host,
                     const int port,
                     const std::string& path,
                     bool encrypted,
                     PrinterInfoCallback callback);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINTER_INFO_H_
