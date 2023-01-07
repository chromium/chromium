// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINTER_EVENT_TRACKER_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINTER_EVENT_TRACKER_H_

#include <vector>

#include "base/synchronization/lock.h"
#include "chrome/browser/ash/printing/printer_detector.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/metrics_proto/printer_event.pb.h"

namespace chromeos {
class Printer;
}  // namespace chromeos

namespace ash {

// Aggregates printer events for logging.  This class is thread-safe.
class PrinterEventTracker : public KeyedService {
 public:
  enum SetupMode {
    // Catch-all for unrecognized mode strings.
    kUnknownMode,
    // Device configured by the user.
    kUser,
    // Configuration detected by the system.
    kAutomatic
  };

  PrinterEventTracker();

  PrinterEventTracker(const PrinterEventTracker&) = delete;
  PrinterEventTracker& operator=(const PrinterEventTracker&) = delete;

  ~PrinterEventTracker() override;

  // If |logging| is true, logging is enabled. If |logging| is false, logging is
  // disabled and the Record* functions are nops.
  void set_logging(bool logging);

  // Store a succesful USB printer installation. |mode| indicates if
  // the PPD was selected automatically or chosen by the user.
  void RecordUsbPrinterInstalled(
      const PrinterDetector::DetectedPrinter& printer,
      SetupMode mode);

  // Store a succesful network printer installation. |mode| indicates if
  // the PPD was selected automatically or chosen by the user.
  void RecordIppPrinterInstalled(const chromeos::Printer& printer,
                                 SetupMode mode);

  // Record an abandoned setup.
  void RecordSetupAbandoned(const chromeos::Printer& printer);

  // Record an abandoned setup for a USB printer.
  void RecordUsbSetupAbandoned(const PrinterDetector::DetectedPrinter& printer);

  // Store a printer removal.
  void RecordPrinterRemoved(const chromeos::Printer& printer);

  // Writes stored events to |events|.
  void FlushPrinterEvents(std::vector<metrics::PrinterEventProto>* events);

 private:
  // Records logs if true.  Discards logs if false.
  bool logging_ = false;
  std::vector<metrics::PrinterEventProto> events_;
  base::Lock lock_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINTER_EVENT_TRACKER_H_
