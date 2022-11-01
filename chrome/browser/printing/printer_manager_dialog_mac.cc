// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printer_manager_dialog.h"

#include "base/mac/mac_util.h"

namespace printing {

void PrinterManagerDialog::ShowPrinterManagerDialog() {
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kPrintersScanners);
}

}  // namespace printing
