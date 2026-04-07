// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_LOCAL_PRINTER_UTILS_CHROMEOS_H_
#define CHROME_BROWSER_PRINTING_LOCAL_PRINTER_UTILS_CHROMEOS_H_

#include "chromeos/crosapi/mojom/local_printer.mojom.h"

namespace printing {

// Returns the platform-specific handle for the LocalPrinter interface.
crosapi::mojom::LocalPrinter* GetLocalPrinterInterface();

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_LOCAL_PRINTER_UTILS_CHROMEOS_H_
