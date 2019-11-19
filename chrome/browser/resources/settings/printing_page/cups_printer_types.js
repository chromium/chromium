// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   printerInfo: !CupsPrinterInfo,
 *   printerType: number,
 * }}
 */
let PrinterListEntry;

/**
 * @enum {number}
 * These values correspond to the different types of printers available. Refer
 * to cups_printer_management.md for more information about the different
 * categories of printers.
 *
 * The types are numbered in desired sorting order for display.
 */
const PrinterType = {
  SAVED: 0,
  AUTOMATIC: 1,
  DISCOVERED: 2,
};