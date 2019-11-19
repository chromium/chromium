// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_INFO_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_INFO_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "printing/printer_query_result_chromeos.h"

namespace chromeos {

// Callback for basic printer information.  |success| indicates if the request
// succeeded at all.  |make| represents the printer manufacturer.  |model| is
// the printer model.  |make_and_model| is the raw printer-make-and-model value
// from the printer. |autoconf| indicates if we think we can compute the
// printer capabilites without a PPD.
using PrinterInfoCallback =
    base::OnceCallback<void(::printing::PrinterQueryResult result,
                            const std::string& make,
                            const std::string& model,
                            const std::string& make_and_model,
                            const std::vector<std::string>& document_formats,
                            bool autoconf)>;

// Dispatch an IPP request to |host| on |port| for |path| to obtain
// basic printer information.
void QueryIppPrinter(const std::string& host,
                     const int port,
                     const std::string& path,
                     bool encrypted,
                     PrinterInfoCallback callback);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_INFO_H_
