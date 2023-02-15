// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';

/**
 *  These values must be kept in sync with the Reason enum in
 *  /chromeos/printing/cups_printer_status.h
 */
export enum PrinterStatusReason {
  UNKNOWN_REASON = 0,
  DEVICE_ERROR = 1,
  DOOR_OPEN = 2,
  LOW_ON_INK = 3,
  LOW_ON_PAPER = 4,
  NO_ERROR = 5,
  OUT_OF_INK = 6,
  OUT_OF_PAPER = 7,
  OUTPUT_ALMOST_FULL = 8,
  OUTPUT_FULL = 9,
  PAPER_JAM = 10,
  PAUSED = 11,
  PRINTER_QUEUE_FULL = 12,
  PRINTER_UNREACHABLE = 13,
  STOPPED = 14,
  TRAY_MISSING = 15,
}

/**
 *  These values must be kept in sync with the Severity enum in
 *  /chromeos/printing/cups_printer_status.h
 */
export enum PrinterStatusSeverity {
  UNKNOWN_SEVERITY = 0,
  REPORT = 1,
  WARNING = 2,
  ERROR = 3,
}

/**
 * Enumeration giving a local Chrome OS printer 3 different state possibilities
 * depending on its current status.
 */
export enum PrinterState {
  GOOD = 0,
  ERROR = 1,
  UNKNOWN = 2,
}

export enum PrintAttemptOutcome {
  CANCELLED_PRINT_BUTTON_DISABLED = 0,
  CANCELLED_NO_PRINTERS_AVAILABLE = 1,
  CANCELLED_OTHER_PRINTERS_AVAILABLE = 2,
  CANCELLED_PRINTER_ERROR_STATUS = 3,
  CANCELLED_PRINTER_GOOD_STATUS = 4,
  CANCELLED_PRINTER_UNKNOWN_STATUS = 5,
  PDF_PRINT_ATTEMPTED = 6,
  PRINT_JOB_SUCCESS_INITIAL_PRINTER = 7,
  PRINT_JOB_SUCCESS_MANUALLY_SELECTED_PRINTER = 8,
  PRINT_JOB_FAIL_INITIAL_PRINTER = 9,
  PRINT_JOB_FAIL_MANUALLY_SELECTED_PRINTER = 10,
}

interface StatusReasonEntry {
  reason: PrinterStatusReason;
  severity: PrinterStatusSeverity;
}

/**
 * A container for the results of a printer status query. A printer status query
 * can return multiple error reasons. |timestamp| is set at the time of status
 * creation.
 */
export interface PrinterStatus {
  printerId: string;
  statusReasons: StatusReasonEntry[];
  timestamp: number;
}

export const ERROR_STRING_KEY_MAP: Map<PrinterStatusReason, string> = new Map([
  [PrinterStatusReason.DEVICE_ERROR, 'printerStatusDeviceError'],
  [PrinterStatusReason.DOOR_OPEN, 'printerStatusDoorOpen'],
  [PrinterStatusReason.LOW_ON_INK, 'printerStatusLowOnInk'],
  [PrinterStatusReason.LOW_ON_PAPER, 'printerStatusLowOnPaper'],
  [PrinterStatusReason.OUT_OF_INK, 'printerStatusOutOfInk'],
  [PrinterStatusReason.OUT_OF_PAPER, 'printerStatusOutOfPaper'],
  [PrinterStatusReason.OUTPUT_ALMOST_FULL, 'printerStatusOutputAlmostFull'],
  [PrinterStatusReason.OUTPUT_FULL, 'printerStatusOutputFull'],
  [PrinterStatusReason.PAPER_JAM, 'printerStatusPaperJam'],
  [PrinterStatusReason.PAUSED, 'printerStatusPaused'],
  [PrinterStatusReason.PRINTER_QUEUE_FULL, 'printerStatusPrinterQueueFull'],
  [PrinterStatusReason.PRINTER_UNREACHABLE, 'printerStatusPrinterUnreachable'],
  [PrinterStatusReason.STOPPED, 'printerStatusStopped'],
  [PrinterStatusReason.TRAY_MISSING, 'printerStatusTrayMissing'],
]);

/**
 * A |printerStatus| can have multiple status reasons so this function's
 * responsibility is to determine which status reason is most relevant to
 * surface to the user. Any status reason with a severity of WARNING or ERROR
 * will get highest precedence since this usually means the printer is in a
 * bad state. If there does not exist an error status reason with a high enough
 * severity, then return NO_ERROR.
 * @return Status reason extracted from |printerStatus|.
 */
export function getStatusReasonFromPrinterStatus(printerStatus: PrinterStatus):
    PrinterStatusReason {
  if (!printerStatus.printerId) {
    // TODO(crbug.com/1027400): Remove console.warn once bug is confirmed fix.
    console.warn('Received printer status missing printer id');
    return PrinterStatusReason.UNKNOWN_REASON;
  }
  let statusReason = PrinterStatusReason.NO_ERROR;
  for (const printerStatusReason of printerStatus.statusReasons) {
    const reason = printerStatusReason.reason;
    const severity = printerStatusReason.severity;
    if (severity !== PrinterStatusSeverity.ERROR &&
        severity !== PrinterStatusSeverity.WARNING) {
      continue;
    }

    // Always prioritize an ERROR severity status, unless it's for unknown
    // reasons.
    if (reason !== PrinterStatusReason.UNKNOWN_REASON &&
        severity === PrinterStatusSeverity.ERROR) {
      return reason;
    }

    if (reason !== PrinterStatusReason.UNKNOWN_REASON ||
        statusReason === PrinterStatusReason.NO_ERROR) {
      statusReason = reason;
    }
  }
  return statusReason;
}

export function computePrinterState(
    printerStatusReason: (PrinterStatusReason|null)): PrinterState {
  if (printerStatusReason === null ||
      printerStatusReason === PrinterStatusReason.UNKNOWN_REASON) {
    return PrinterState.UNKNOWN;
  }
  if (printerStatusReason === PrinterStatusReason.NO_ERROR) {
    return PrinterState.GOOD;
  }
  return PrinterState.ERROR;
}

export function getPrinterStatusIcon(
    printerStatusReason: PrinterStatusReason|null, isEnterprisePrinter: boolean,
    prefersDarkColorScheme: boolean): string {
  const printerTypePrefix = isEnterprisePrinter ?
      'print-preview:business-printer-status-' :
      'print-preview:printer-status-';
  const darkModeSuffix = prefersDarkColorScheme ? '-dark' : '';
  switch (computePrinterState(printerStatusReason)) {
    case PrinterState.GOOD:
      return `${printerTypePrefix}green${darkModeSuffix}`;
    case PrinterState.ERROR:
      return `${printerTypePrefix}red${darkModeSuffix}`;
    case PrinterState.UNKNOWN:
      return `${printerTypePrefix}grey${darkModeSuffix}`;
    default:
      assertNotReached();
  }
}
