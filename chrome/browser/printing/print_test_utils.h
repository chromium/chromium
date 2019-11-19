// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_TEST_UTILS_H_
#define CHROME_BROWSER_PRINTING_PRINT_TEST_UTILS_H_

#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"

namespace base {
class Value;
}

namespace printing {

extern const char kDummyPrinterName[];
const int kTestPrinterDpi = 600;

// Creates a print ticket with some default values. Based on ticket creation in
// chrome/browser/resources/print_preview/native_layer.js.
base::Value GetPrintTicket(PrinterType type);

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_TEST_UTILS_H_
