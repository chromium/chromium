// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assertNotReached} from 'chrome://resources/js/assert.m.js';

/**
 *  These values must be kept in sync with the Reason enum in
 *  /chromeos/printing/cups_printer_status.h
 *  @enum {number}
 */
export const PrinterStatusReason = {
  DEVICE_ERROR: 0,
  DOOR_OPEN: 1,
  LOW_ON_INK: 2,
  LOW_ON_PAPER: 3,
  NO_ERROR: 4,
  OUT_OF_INK: 5,
  OUT_OF_PAPER: 6,
  OUTPUT_ALMOST_FULL: 7,
  OUTPUT_FULL: 8,
  PAPER_JAM: 9,
  PAUSED: 10,
  PRINTER_QUEUE_FULL: 11,
  PRINTER_UNREACHABLE: 12,
  STOPPED: 13,
  TRAY_MISSING: 14,
  UNKNOWN_REASON: 15,
};

/**
 *  These values must be kept in sync with the Severity enum in
 *  /chromeos/printing/cups_printer_status.h
 *  @enum {number}
 */
export const PrinterStatusSeverity = {
  UNKNOWN_SEVERITY: 0,
  REPORT: 1,
  WARNING: 2,
  ERROR: 3,
};

/**
 * Enumeration giving a local Chrome OS printer 3 different state possibilities
 * depending on its current status.
 * @enum {number}
 */
export const PrinterState = {
  GOOD: 0,
  ERROR: 1,
  UNKNOWN: 2,
};

/**
 * A container for the results of a printer status query. A printer status query
 * can return multiple error reasons. |timestamp| is set at the time of status
 * creation.
 *
 * @typedef {{
 *   printerId: string,
 *   statusReasons: !Array<{
 *     reason: PrinterStatusReason,
 *     severity: PrinterStatusSeverity,
 *   }>,
 *   timestamp: number,
 * }}
 */
export let PrinterStatus;

/** @const {!Map<!PrinterStatusReason, string>} */
export const ERROR_STRING_KEY_MAP = new Map([
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
 * @param {!PrinterStatus} printerStatus
 * @return {!PrinterStatusReason} Status reason extracted from |printerStatus|.
 */
export function getStatusReasonFromPrinterStatus(printerStatus) {
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

/**
 * @param {?PrinterStatusReason} printerStatusReason
 * @return {number}
 */
export function computePrinterState(printerStatusReason) {
  if (!printerStatusReason ||
      printerStatusReason === PrinterStatusReason.UNKNOWN_REASON) {
    return PrinterState.UNKNOWN;
  }
  if (printerStatusReason === PrinterStatusReason.NO_ERROR) {
    return PrinterState.GOOD;
  }
  return PrinterState.ERROR;
}

/**
 * @param {?PrinterStatusReason} printerStatusReason
 * @param {boolean} isEnterprisePrinter
 * @return {string}
 */
export function getPrinterStatusIcon(printerStatusReason, isEnterprisePrinter) {
  switch (computePrinterState(printerStatusReason)) {
    case PrinterState.GOOD:
      return isEnterprisePrinter ?
          'print-preview:business-printer-status-green' :
          'print-preview:printer-status-green';
    case PrinterState.ERROR:
      return isEnterprisePrinter ? 'print-preview:business-printer-status-red' :
                                   'print-preview:printer-status-red';
    case PrinterState.UNKNOWN:
      return isEnterprisePrinter ?
          'print-preview:business-printer-status-grey' :
          'print-preview:printer-status-grey';
    default:
      assertNotReached();
  }
}
