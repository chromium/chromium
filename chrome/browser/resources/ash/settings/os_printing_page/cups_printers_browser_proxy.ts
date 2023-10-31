// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "CUPS printing" section to
 * interact with the browser. Used only on Chrome OS.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.js';

import {PrinterStatus} from './printer_status.js';

/**
 * Note: |printerPPDPath| refers to a PPD retrieved from the user at the
 * add-printer-manufacturer-model-dialog. |printerPpdReference| refers to either
 * information retrieved from the printer or resolved via ppd_provider.
 */
export interface CupsPrinterInfo {
  isManaged: boolean;
  ppdManufacturer: string;
  ppdModel: string;
  printerAddress: string;
  printerDescription: string;
  printerId: string;
  printerMakeAndModel: string;
  printerName: string;
  printerPPDPath: string;
  printerPpdReference: {
    userSuppliedPpdUrl: string,
    effectiveMakeAndModel: string,
    autoconf: boolean,
  };
  printerProtocol: string;
  printerQueue: string;
  printServerUri: string;
  printerStatus?: PrinterStatus;
}

export interface CupsPrintersList {
  printerList: CupsPrinterInfo[];
}

export interface ManufacturersInfo {
  success: boolean;
  manufacturers: string[];
}

export interface ModelsInfo {
  success: boolean;
  models: string[];
}

export interface PrinterMakeModel {
  makeAndModel: string;
  autoconf: boolean;
  ppdRefUserSuppliedPpdUrl: string;
  ppdRefEffectiveMakeAndModel: string;
  ppdReferenceResolved: boolean;
}


export interface PrinterPpdMakeModel {
  ppdManufacturer: string;
  ppdModel: string;
}

/**
 * These values must be kept in sync with the PrinterSetupResult enum in
 * chrome/browser/ash/printing/printer_configurer.h.
 */
export enum PrinterSetupResult {
  FATAL_ERROR = 0,
  SUCCESS = 1,
  PRINTER_UNREACHABLE = 2,
  DBUS_ERROR = 3,
  NATIVE_PRINTERS_NOT_ALLOWED = 4,
  INVALID_PRINTER_UPDATE = 5,
  COMPONENT_UNAVAILAVLE = 6,
  EDIT_SUCCESS = 7,
  PPD_TOO_LARGE = 10,
  INVALID_PPD = 11,
  PPD_NOT_FOUND = 12,
  PPD_UNRETRIEVABLE = 13,
  IO_ERROR = 14,
  MEMORY_ALLOCATION_ERROR = 15,
  BAD_URI = 16,
  MANUAL_SETUP_REQUIRED = 17,
  DBUS_NO_REPLY = 64,
  DBUS_TIMEOUT = 65,
}

/**
 * These values must be kept in sync with the PrintServerQueryResult enum in
 * /chrome/browser/ash/printing/server_printers_fetcher.h
 */
export enum PrintServerResult {
  NO_ERRORS = 0,
  INCORRECT_URL = 1,
  CONNECTION_ERROR = 2,
  HTTP_ERROR = 3,
  CANNOT_PARSE_IPP_RESPONSE = 4,
}


export interface QueryFailure {
  message: string;
}

export interface CupsPrintersBrowserProxy {
  getCupsSavedPrintersList(): Promise<CupsPrintersList>;

  getCupsEnterprisePrintersList(): Promise<CupsPrintersList>;

  updateCupsPrinter(printerId: string, printerName: string):
      Promise<PrinterSetupResult>;

  removeCupsPrinter(printerId: string, printerName: string): void;

  retrieveCupsPrinterPpd(printerId: string, printerName: string, eula: string):
      void;

  getCupsPrinterPpdPath(): Promise<string>;

  addCupsPrinter(newPrinter: CupsPrinterInfo): Promise<PrinterSetupResult>;

  reconfigureCupsPrinter(printer: CupsPrinterInfo): Promise<PrinterSetupResult>;

  startDiscoveringPrinters(): void;
  stopDiscoveringPrinters(): void;

  getCupsPrinterManufacturersList(): Promise<ManufacturersInfo>;

  getCupsPrinterModelsList(manufacturer: string): Promise<ModelsInfo>;

  getPrinterInfo(newPrinter: CupsPrinterInfo): Promise<PrinterMakeModel>;

  getPrinterPpdManufacturerAndModel(printerId: string):
      Promise<PrinterPpdMakeModel>;

  addDiscoveredPrinter(printerId: string): Promise<PrinterSetupResult>;

  /**
   * Report to the handler that setup was cancelled.
   */
  cancelPrinterSetUp(newPrinter: CupsPrinterInfo): void;

  /**
   * Returns the EULA URL of the printer. Returns an empty string if no EULA is
   * required.
   */
  getEulaUrl(ppdManufacturer: string, ppdModel: string): Promise<string>;

  /**
   * Attempts to query the |serverUrl| and retrieve printers from the url.
   */
  queryPrintServer(serverUrl: string): Promise<CupsPrintersList>;

  /**
   * Opens the print management app in its own window.
   */
  openPrintManagementApp(): void;

  /**
   * Opens the Scanning app in its own window.
   */
  openScanningApp(): void;

  /**
   * Sends a request to the printer with id |printerId| for its current status.
   */
  requestPrinterStatusUpdate(printerId: string): Promise<PrinterStatus>;
}

let instance: CupsPrintersBrowserProxy|null = null;

export class CupsPrintersBrowserProxyImpl implements CupsPrintersBrowserProxy {
  static getInstance(): CupsPrintersBrowserProxy {
    return instance || (instance = new CupsPrintersBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: CupsPrintersBrowserProxy): void {
    instance = obj;
  }

  getCupsSavedPrintersList(): Promise<CupsPrintersList> {
    return sendWithPromise('getCupsSavedPrintersList');
  }

  getCupsEnterprisePrintersList(): Promise<CupsPrintersList> {
    return sendWithPromise('getCupsEnterprisePrintersList');
  }

  updateCupsPrinter(printerId: string, printerName: string):
      Promise<PrinterSetupResult> {
    return sendWithPromise('updateCupsPrinter', printerId, printerName);
  }

  removeCupsPrinter(printerId: string, printerName: string): void {
    chrome.send('removeCupsPrinter', [printerId, printerName]);
  }

  retrieveCupsPrinterPpd(printerId: string, printerName: string, eula: string):
      void {
    chrome.send('retrieveCupsPrinterPpd', [printerId, printerName, eula]);
  }

  addCupsPrinter(newPrinter: CupsPrinterInfo): Promise<PrinterSetupResult> {
    return sendWithPromise('addCupsPrinter', newPrinter);
  }

  reconfigureCupsPrinter(printer: CupsPrinterInfo):
      Promise<PrinterSetupResult> {
    return sendWithPromise('reconfigureCupsPrinter', printer);
  }

  getCupsPrinterPpdPath(): Promise<string> {
    return sendWithPromise('selectPPDFile');
  }

  startDiscoveringPrinters(): void {
    chrome.send('startDiscoveringPrinters');
  }

  stopDiscoveringPrinters(): void {
    chrome.send('stopDiscoveringPrinters');
  }

  getCupsPrinterManufacturersList(): Promise<ManufacturersInfo> {
    return sendWithPromise('getCupsPrinterManufacturersList');
  }

  getCupsPrinterModelsList(manufacturer: string): Promise<ModelsInfo> {
    return sendWithPromise('getCupsPrinterModelsList', manufacturer);
  }

  getPrinterInfo(newPrinter: CupsPrinterInfo): Promise<PrinterMakeModel> {
    return sendWithPromise('getPrinterInfo', newPrinter);
  }

  getPrinterPpdManufacturerAndModel(printerId: string):
      Promise<PrinterPpdMakeModel> {
    return sendWithPromise('getPrinterPpdManufacturerAndModel', printerId);
  }

  addDiscoveredPrinter(printerId: string): Promise<PrinterSetupResult> {
    return sendWithPromise('addDiscoveredPrinter', printerId);
  }

  cancelPrinterSetUp(newPrinter: CupsPrinterInfo): void {
    chrome.send('cancelPrinterSetUp', [newPrinter]);
  }

  getEulaUrl(ppdManufacturer: string, ppdModel: string): Promise<string> {
    return sendWithPromise('getEulaUrl', ppdManufacturer, ppdModel);
  }

  queryPrintServer(serverUrl: string): Promise<CupsPrintersList> {
    return sendWithPromise('queryPrintServer', serverUrl);
  }

  openPrintManagementApp(): void {
    chrome.send('openPrintManagementApp');
  }

  openScanningApp(): void {
    chrome.send('openScanningApp');
  }

  requestPrinterStatusUpdate(printerId: string): Promise<PrinterStatus> {
    return sendWithPromise('requestPrinterStatus', printerId);
  }
}
