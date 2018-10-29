// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

/**
 * @typedef {{selectSaveAsPdfDestination: boolean,
 *            layoutSettings.portrait: boolean,
 *            pageRange: string,
 *            headersAndFooters: boolean,
 *            backgroundColorsAndImages: boolean,
 *            margins: number}}
 * @see chrome/browser/printing/print_preview_pdf_generated_browsertest.cc
 */
print_preview.PreviewSettings;

/**
 * @typedef {{
 *   deviceName: string,
 *   printerName: string,
 *   printerDescription: (string | undefined),
 *   cupsEnterprisePrinter: (boolean | undefined),
 *   printerOptions: (Object | undefined),
 *   policies: (print_preview.Policies | undefined),
 * }}
 */
print_preview.LocalDestinationInfo;

/**
 * @typedef {{
 *   isInKioskAutoPrintMode: boolean,
 *   isInAppKioskMode: boolean,
 *   thousandsDelimeter: string,
 *   decimalDelimeter: string,
 *   unitType: !print_preview.MeasurementSystemUnitType,
 *   previewModifiable: boolean,
 *   documentTitle: string,
 *   documentHasSelection: boolean,
 *   shouldPrintSelectionOnly: boolean,
 *   printerName: string,
 *   headerFooter: ?boolean,
 *   isHeaderFooterManaged: boolean,
 *   serializedAppStateStr: ?string,
 *   serializedDefaultDestinationSelectionRulesStr: ?string,
 * }}
 * @see corresponding field name definitions in print_preview_handler.cc
 */
print_preview.NativeInitialSettings;

/**
 * @typedef {{
 *   serviceName: string,
 *   name: string,
 *   hasLocalPrinting: boolean,
 *   isUnregistered: boolean,
 *   cloudID: string,
 * }}
 * @see PrintPreviewHandler::FillPrinterDescription in print_preview_handler.cc
 */
print_preview.PrivetPrinterDescription;

/**
 * @typedef {{
 *   printer:(print_preview.PrivetPrinterDescription |
 *            print_preview.LocalDestinationInfo |
 *            undefined),
 *   capabilities: !print_preview.Cdd,
 * }}
 */
print_preview.CapabilitiesResponse;

/**
 * @typedef {{
 *   printerId: string,
 *   success: boolean,
 *   capabilities: !print_preview.Cdd,
 *   policies: (print_preview.Policies | undefined),
 * }}
 */
print_preview.PrinterSetupResponse;

/**
 * @typedef {{
 *   extensionId: string,
 *   extensionName: string,
 *   id: string,
 *   name: string,
 *   description: (string|undefined),
 * }}
 */
print_preview.ProvisionalDestinationInfo;

/**
 * Printer types for capabilities and printer list requests.
 * Should match PrinterType in print_preview_handler.h
 * @enum {number}
 */
print_preview.PrinterType = {
  PRIVET_PRINTER: 0,
  EXTENSION_PRINTER: 1,
  PDF_PRINTER: 2,
  LOCAL_PRINTER: 3,
  CLOUD_PRINTER: 4
};

cr.define('print_preview', function() {
  'use strict';

  /**
   * An interface to the native Chromium printing system layer.
   */
  class NativeLayer {
    /**
     * Creates a new NativeLayer if the current instance is not set.
     * @return {!print_preview.NativeLayer} The singleton instance.
     */
    static getInstance() {
      if (currentInstance == null)
        currentInstance = new NativeLayer();
      return assert(currentInstance);
    }

    /**
     * @param {!print_preview.NativeLayer} instance The NativeLayer instance
     *     to set for print preview construction.
     */
    static setInstance(instance) {
      currentInstance = instance;
    }

    /**
     * Requests access token for cloud print requests.
     * @param {string} authType type of access token.
     * @return {!Promise<string>}
     */
    getAccessToken(authType) {
      return cr.sendWithPromise('getAccessToken', authType);
    }

    /**
     * Gets the initial settings to initialize the print preview with.
     * @return {!Promise<!print_preview.NativeInitialSettings>}
     */
    getInitialSettings() {
      return cr.sendWithPromise('getInitialSettings');
    }

    /**
     * Requests the system's print destinations. The promise will be resolved
     * when all destinations of that type have been retrieved. One or more
     * 'printers-added' events may be fired in response before resolution.
     * @param {!print_preview.PrinterType} type The type of destinations to
     *     request.
     * @return {!Promise}
     */
    getPrinters(type) {
      return cr.sendWithPromise('getPrinters', type);
    }

    /**
     * Requests the destination's printing capabilities. Returns a promise that
     * will be resolved with the capabilities if they are obtained successfully.
     * @param {string} destinationId ID of the destination.
     * @param {!print_preview.PrinterType} type The destination's printer type.
     * @return {!Promise<!print_preview.CapabilitiesResponse>}
     */
    getPrinterCapabilities(destinationId, type) {
      return cr.sendWithPromise(
          'getPrinterCapabilities', destinationId,
          destinationId ==
                  print_preview.Destination.GooglePromotedId.SAVE_AS_PDF ?
              print_preview.PrinterType.PDF_PRINTER :
              type);
    }

    /**
     * Requests Chrome to resolve provisional extension destination by granting
     * the provider extension access to the printer.
     * @param {string} provisionalDestinationId
     * @return {!Promise<!print_preview.ProvisionalDestinationInfo>}
     */
    grantExtensionPrinterAccess(provisionalDestinationId) {
      return cr.sendWithPromise('grantExtensionPrinterAccess',
                                provisionalDestinationId);
    }

    /**
     * Requests that Chrome peform printer setup for the given printer.
     * @param {string} printerId
     * @return {!Promise<!print_preview.PrinterSetupResponse>}
     */
    setupPrinter(printerId) {
      return cr.sendWithPromise('setupPrinter', printerId);
    }

    /**
     * Requests that a preview be generated. The following Web UI events may
     * be triggered in response:
     *   'print-preset-options',
     *   'page-count-ready',
     *   'page-layout-ready',
     *   'page-preview-ready'
     * @param {string} printTicket JSON print ticket for the request.
     * @return {!Promise<number>} Promise that resolves with the unique ID of
     *     the preview UI when the preview has been generated.
     */
    getPreview(printTicket) {
      return cr.sendWithPromise('getPreview', printTicket);
    }

    /**
     * Requests that the document be printed.
     * @param {string} printTicket The serialized print ticket for the print
     *     job.
     * @return {!Promise} Promise that will resolve when the print request is
     *     finished or rejected.
     */
    print(printTicket) {
      return cr.sendWithPromise('print', printTicket);
    }

    /** Requests that the current pending print request be cancelled. */
    cancelPendingPrintRequest() {
      chrome.send('cancelPendingPrintRequest');
    }

    /**
     * Sends the app state to be saved in the sticky settings.
     * @param {string} appStateStr JSON string of the app state to persist.
     */
    saveAppState(appStateStr) {
      chrome.send('saveAppState', [appStateStr]);
    }

    /** Shows the system's native printing dialog. */
    showSystemDialog() {
      assert(!cr.isWindows);
      chrome.send('showSystemDialog');
    }

    /**
     * Closes the print preview dialog.
     * If |isCancel| is true, also sends a message to Print Preview Handler in
     * order to update UMA statistics.
     * @param {boolean} isCancel whether this was called due to the user
     *     closing the dialog without printing.
     */
    dialogClose(isCancel) {
      if (isCancel)
        chrome.send('closePrintPreviewDialog');
      chrome.send('dialogClose');
    }

    /** Hide the print preview dialog and allow the native layer to close it. */
    hidePreview() {
      chrome.send('hidePreview');
    }

    /**
     * Opens the Google Cloud Print sign-in tab. The DESTINATIONS_RELOAD event
     *     will be dispatched in response.
     * @param {boolean} addAccount Whether to open an 'add a new account' or
     *     default sign in page.
     * @return {!Promise} Promise that resolves when the sign in tab has been
     *     closed and the destinations should be reloaded.
     */
    signIn(addAccount) {
      return cr.sendWithPromise('signIn', addAccount);
    }

    /**
     * Navigates the user to the Chrome printing setting page to manage local
     * printers and Google cloud printers.
     * TODO (rbpotter): Delete this when the old Print Preview page is removed.
     */
    managePrinters() {
      chrome.send('managePrinters');
    }

    /**
     * Forces browser to open a new tab with the given URL address.
     * TODO (rbpotter): Delete this when the old Print Preview page is removed.
     */
    forceOpenNewTab(url) {
      chrome.send('forceOpenNewTab', [url]);
    }

    /**
     * Sends a message to the test, letting it know that an
     * option has been set to a particular value and that the change has
     * finished modifying the preview area.
     */
    uiLoadedForTest() {
      chrome.send('UILoadedForTest');
    }

    /**
     * Notifies the test that the option it tried to change
     * had not been changed successfully.
     */
    uiFailedLoadingForTest() {
      chrome.send('UIFailedLoadingForTest');
    }

    /**
     * Notifies the metrics handler to record a histogram value.
     * @param {string} histogram The name of the histogram to record
     * @param {number} bucket The bucket to record
     * @param {number} maxBucket The maximum bucket value in the histogram.
     */
    recordInHistogram(histogram, bucket, maxBucket) {
      chrome.send(
          'metricsHandler:recordInHistogram', [histogram, bucket, maxBucket]);
    }
  }

  /** @private {?print_preview.NativeLayer} */
  let currentInstance = null;

  /**
   * Version of the serialized state of the print preview.
   * @type {number}
   * @const
   * @private
   */
  NativeLayer.SERIALIZED_STATE_VERSION_ = 1;

  // Export
  return {
    NativeLayer: NativeLayer
  };
});
