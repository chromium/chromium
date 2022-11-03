// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CupsPrinterInfo} from './cups_printers_browser_proxy.js';

export interface PrinterListEntry {
  printerInfo: CupsPrinterInfo;
  printerType: number;
}

/**
 * These values correspond to the different types of printers available. Refer
 * to cups_printer_management.md for more information about the different
 * categories of printers.
 *
 * The types are numbered in desired sorting order for display.
 */
export enum PrinterType {
  SAVED = 0,
  PRINTSERVER = 1,
  AUTOMATIC = 2,
  DISCOVERED = 3,
  ENTERPRISE = 4,
}
