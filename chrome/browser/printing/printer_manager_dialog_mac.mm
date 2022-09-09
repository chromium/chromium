// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printer_manager_dialog.h"

#import <AppKit/AppKit.h>

namespace printing {

static NSString* kPrintAndFaxPrefPane =
    @"/System/Library/PreferencePanes/PrintAndFax.prefPane";

void PrinterManagerDialog::ShowPrinterManagerDialog(Profile* profile) {
  [[NSWorkspace sharedWorkspace] openFile:kPrintAndFaxPrefPane];
}

}  // namespace printing
