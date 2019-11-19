// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_DETECTOR_H_

#include <vector>

#include "base/callback.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {

// Interface for automatic printer detection.  This API allows for
// incremental discovery of printers and notification when discovery
// is complete.
//
// All of the interface calls in this class must be called from the
// same sequence, but do not have to be on any specific thread.
//
// The usual usage of this interface by a class that wants to maintain
// an up-to-date list of the printers detected is this:
//
// auto detector_ = PrinterDetectorImplementation::Create();
// detector_->RegisterPrintersFoundCallback(cb);
// printers_ = detector_->GetPrinters();
//
class CHROMEOS_EXPORT PrinterDetector {
 public:
  // The result of a detection.
  struct DetectedPrinter {
    // Printer information
    Printer printer;

    // Additional metadata used to find a driver.
    PrinterSearchData ppd_search_data;
  };

  using OnPrintersFoundCallback = base::RepeatingCallback<void(
      const std::vector<DetectedPrinter>& printers)>;

  virtual ~PrinterDetector() = default;

  virtual void RegisterPrintersFoundCallback(OnPrintersFoundCallback cb) = 0;

  // Get the current list of known printers.
  virtual std::vector<DetectedPrinter> GetPrinters() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_DETECTOR_H_
