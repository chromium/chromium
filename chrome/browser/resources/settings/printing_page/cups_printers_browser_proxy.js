// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "CUPS printing" section to
 * interact with the browser. Used only on Chrome OS.
 */

/**
 * @typedef {{
 *   ppdManufacturer: string,
 *   ppdModel: string,
 *   printerAddress: string,
 *   printerDescription: string,
 *   printerId: string,
 *   printerManufacturer: string,
 *   printerModel: string,
 *   printerMakeAndModel: string,
 *   printerName: string,
 *   printerPPDPath: string,
 *   printerPpdReference: {
 *     userSuppliedPpdUrl: string,
 *     effectiveMakeAndModel: string,
 *     autoconf: boolean,
 *   },
 *   printerPpdReferenceResolved: boolean,
 *   printerProtocol: string,
 *   printerQueue: string,
 *   printerStatus: string,
 * }}
 *
 * Note: |printerPPDPath| refers to a PPD retrieved from the user at the
 * add-printer-manufacturer-model-dialog. |printerPpdReference| refers to either
 * information retrieved from the printer or resolved via ppd_provider.
 */
let CupsPrinterInfo;

/**
 * @typedef {{
 *   printerList: !Array<!CupsPrinterInfo>,
 * }}
 */
let CupsPrintersList;

/**
 * @typedef {{
 *   success: boolean,
 *   manufacturers: Array<string>
 * }}
 */
let ManufacturersInfo;

/**
 * @typedef {{
 *   success: boolean,
 *   models: Array<string>
 * }}
 */
let ModelsInfo;

/**
 * @typedef {{
 *   manufacturer: string,
 *   model: string,
 *   makeAndModel: string,
 *   autoconf: boolean,
 *   ppdRefUserSuppliedPpdUrl: string,
 *   ppdRefEffectiveMakeAndModel: string,
 *   ppdReferenceResolved: boolean
 * }}
 */
let PrinterMakeModel;

/**
 * @typedef {{
 *   ppdManufacturer: string,
 *   ppdModel: string
 * }}
 */
let PrinterPpdMakeModel;

/**
 *  @enum {number}
 *  These values must be kept in sync with the PrinterSetupResult enum in
 *  chrome/browser/chromeos/printing/printer_configurer.h.
 */
const PrinterSetupResult = {
  FATAL_ERROR: 0,
  SUCCESS: 1,
  PRINTER_UNREACHABLE: 2,
  DBUS_ERROR: 3,
  NATIVE_PRINTERS_NOT_ALLOWED: 4,
  INVALID_PRINTER_UPDATE: 5,
  COMPONENT_UNAVAILAVLE: 6,
  EDIT_SUCCESS: 7,
  PPD_TOO_LARGE: 10,
  INVALID_PPD: 11,
  PPD_NOT_FOUND: 12,
  PPD_UNRETRIEVABLE: 13,
  DBUS_NO_REPLY: 64,
  DBUS_TIMEOUT: 65,
};

/**
 * @typedef {{
 *   message: string
 * }}
 */
let QueryFailure;

cr.define('settings', function() {
  /** @interface */
  class CupsPrintersBrowserProxy {
    /**
     * @return {!Promise<!CupsPrintersList>}
     */
    getCupsPrintersList() {}

    /**
     * @param {string} printerId
     * @param {string} printerName
     * @return {!Promise<!PrinterSetupResult>}
     */
    updateCupsPrinter(printerId, printerName) {}

    /**
     * @param {string} printerId
     * @param {string} printerName
     */
    removeCupsPrinter(printerId, printerName) {}

    /**
     * @return {!Promise<string>} The full path of the printer PPD file.
     */
    getCupsPrinterPPDPath() {}

    /**
     * @param {!CupsPrinterInfo} newPrinter
     * @return {!Promise<!PrinterSetupResult>}
     */
    addCupsPrinter(newPrinter) {}

    /**
     * @param {!CupsPrinterInfo} printer
     * @return {!Promise<!PrinterSetupResult>}
     */
    reconfigureCupsPrinter(printer) {}

    startDiscoveringPrinters() {}
    stopDiscoveringPrinters() {}

    /**
     * @return {!Promise<!ManufacturersInfo>}
     */
    getCupsPrinterManufacturersList() {}

    /**
     * @param {string} manufacturer
     * @return {!Promise<!ModelsInfo>}
     */
    getCupsPrinterModelsList(manufacturer) {}

    /**
     * @param {!CupsPrinterInfo} newPrinter
     * @return {!Promise<!PrinterMakeModel>}
     */
    getPrinterInfo(newPrinter) {}

    /**
     * @param {string} printerId
     * @return {!Promise<!PrinterPpdMakeModel>}
     */
    getPrinterPpdManufacturerAndModel(printerId) {}

    /**
     * @param{string} printerId
     * @return {!Promise<!PrinterSetupResult>}
     */
    addDiscoveredPrinter(printerId) {}

    /**
     * Report to the handler that setup was cancelled.
     * @param {!CupsPrinterInfo} newPrinter
     */
    cancelPrinterSetUp(newPrinter) {}

    /**
     * @param {string} ppdManufacturer
     * @param {string} ppdModel
     * @return {!Promise<string>} Returns the EULA URL of the printer. Returns
     * an empty string if no EULA is required.
     */
    getEulaUrl(ppdManufacturer, ppdModel) {}
  }

  /**
   * @implements {settings.CupsPrintersBrowserProxy}
   */
  class CupsPrintersBrowserProxyImpl {
    /** @override */
    getCupsPrintersList() {
      return cr.sendWithPromise('getCupsPrintersList');
    }

    /** @override */
    updateCupsPrinter(printerId, printerName) {
      return cr.sendWithPromise('updateCupsPrinter', printerId, printerName);
    }

    /** @override */
    removeCupsPrinter(printerId, printerName) {
      chrome.send('removeCupsPrinter', [printerId, printerName]);
    }

    /** @override */
    addCupsPrinter(newPrinter) {
      return cr.sendWithPromise('addCupsPrinter', newPrinter);
    }

    /** @override */
    reconfigureCupsPrinter(printer) {
      return cr.sendWithPromise('reconfigureCupsPrinter', printer);
    }

    /** @override */
    getCupsPrinterPPDPath() {
      return cr.sendWithPromise('selectPPDFile');
    }

    /** @override */
    startDiscoveringPrinters() {
      chrome.send('startDiscoveringPrinters');
    }

    /** @override */
    stopDiscoveringPrinters() {
      chrome.send('stopDiscoveringPrinters');
    }

    /** @override */
    getCupsPrinterManufacturersList() {
      return cr.sendWithPromise('getCupsPrinterManufacturersList');
    }

    /** @override */
    getCupsPrinterModelsList(manufacturer) {
      return cr.sendWithPromise('getCupsPrinterModelsList', manufacturer);
    }

    /** @override */
    getPrinterInfo(newPrinter) {
      return cr.sendWithPromise('getPrinterInfo', newPrinter);
    }

    /** @override */
    getPrinterPpdManufacturerAndModel(printerId) {
      return cr.sendWithPromise('getPrinterPpdManufacturerAndModel', printerId);
    }

    /** @override */
    addDiscoveredPrinter(printerId) {
      return cr.sendWithPromise('addDiscoveredPrinter', printerId);
    }

    /** @override */
    cancelPrinterSetUp(newPrinter) {
      chrome.send('cancelPrinterSetUp', [newPrinter]);
    }

    /** @override */
    getEulaUrl(ppdManufacturer, ppdModel) {
      return cr.sendWithPromise('getEulaUrl', ppdManufacturer, ppdModel);
    }
  }

  cr.addSingletonGetter(CupsPrintersBrowserProxyImpl);

  return {
    CupsPrintersBrowserProxy: CupsPrintersBrowserProxy,
    CupsPrintersBrowserProxyImpl: CupsPrintersBrowserProxyImpl,
  };
});
