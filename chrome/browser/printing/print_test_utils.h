// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_TEST_UTILS_H_
#define CHROME_BROWSER_PRINTING_PRINT_TEST_UTILS_H_

#include "base/values.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"

namespace printing {

namespace mojom {
enum class PrinterType;
}

extern const char kDummyPrinterName[];
const int kTestPrinterDpi = 600;

// Creates a print ticket with some default values. Based on ticket creation in
// chrome/browser/resources/print_preview/native_layer.js.
base::Value::Dict GetPrintTicket(mojom::PrinterType type);

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_TEST_UTILS_H_
