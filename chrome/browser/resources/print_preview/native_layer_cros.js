// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

import {Cdd} from './data/cdd.js';
import {DestinationPolicies} from './data/destination_policies.js';
import {ProvisionalDestinationInfo} from './data/local_parsers.js';
import {PrinterStatus, PrinterStatusReason} from './data/printer_status_cros.js';

/**
 * @typedef {{
 *   printerId: string,
 *   success: boolean,
 *   capabilities: !Cdd,
 *   policies: (DestinationPolicies | undefined),
 * }}
 */
export let PrinterSetupResponse;

/**
 * @typedef {{
 *   id: string,
 *   name: string,
 * }}
 */
export let PrintServer;

/**
 * @typedef {{
 *   printServers: !Array<!PrintServer>,
 *   isSingleServerFetchingMode: boolean,
 * }}
 */
export let PrintServersConfig;

/**
 * An interface to the Chrome OS platform specific part of the native Chromium
 * printing system layer.
 * @interface
 */
export class NativeLayerCros {
  /**
   * Requests access token for cloud print requests for DEVICE origin.
   * @return {!Promise<string>}
   */
  getAccessToken() {}

  /**
   * Requests the destination's end user license information. Returns a promise
   * that will be resolved with the destination's EULA URL if obtained
   * successfully.
   * @param {!string} destinationId ID of the destination.
   * @return {!Promise<string>}
   */
  getEulaUrl(destinationId) {}

  /**
   * Requests Chrome to resolve provisional extension destination by granting
   * the provider extension access to the printer.
   * @param {string} provisionalDestinationId
   * @return {!Promise<!ProvisionalDestinationInfo>}
   */
  grantExtensionPrinterAccess(provisionalDestinationId) {}

  /**
   * Requests that Chrome perform printer setup for the given printer.
   * @param {string} printerId
   * @return {!Promise<!PrinterSetupResponse>}
   */
  setupPrinter(printerId) {}

  /**
   * Sends a request to the printer with id |printerId| for its current status.
   * @param {string} printerId
   * @return {!Promise<?PrinterStatus>}
   */
  requestPrinterStatusUpdate(printerId) {}

  /**
   * Records the histogram to capture the printer status of the current
   * destination and whether the user chose to print or cancel.
   * @param {?PrinterStatusReason} statusReason Current destination printer
   * status
   * @param {boolean} didUserAttemptPrint True if user printed, false if user
   * canceled.
   */
  recordPrinterStatusHistogram(statusReason, didUserAttemptPrint) {}

  /**
   * Selects all print servers with ids in |printServerIds| to query for their
   * printers.
   * @param {!Array<string>} printServerIds
   */
  choosePrintServers(printServerIds) {}

  /**
   * Gets the available print servers and whether we are in single server
   * fetching mode.
   * @return {!Promise<!PrintServersConfig>}
   */
  getPrintServersConfig() {}
}

/** @implements {NativeLayerCros} */
export class NativeLayerCrosImpl {
  /** @override */
  getAccessToken() {
    return sendWithPromise('getAccessToken');
  }

  /** @override */
  getEulaUrl(destinationId) {
    return sendWithPromise('getEulaUrl', destinationId);
  }

  /** @override */
  grantExtensionPrinterAccess(provisionalDestinationId) {
    return sendWithPromise(
        'grantExtensionPrinterAccess', provisionalDestinationId);
  }

  /** @override */
  setupPrinter(printerId) {
    return sendWithPromise('setupPrinter', printerId);
  }

  /** @override */
  requestPrinterStatusUpdate(printerId) {
    return sendWithPromise('requestPrinterStatus', printerId);
  }

  /** @override */
  recordPrinterStatusHistogram(statusReason, didUserAttemptPrint) {
    if (!statusReason) {
      return;
    }

    let histogram;
    switch (statusReason) {
      case (PrinterStatusReason.NO_ERROR):
        histogram = 'PrintPreview.PrinterStatus.AttemptedPrintWithGoodStatus';
        break;
      case (PrinterStatusReason.UNKNOWN_REASON):
        histogram =
            'PrintPreview.PrinterStatus.AttemptedPrintWithUnknownStatus';
        break;
      default:
        histogram = 'PrintPreview.PrinterStatus.AttemptedPrintWithErrorStatus';
        break;
    }
    chrome.send(
        'metricsHandler:recordBooleanHistogram',
        [histogram, didUserAttemptPrint]);
  }

  /** @override */
  choosePrintServers(printServerIds) {
    chrome.send('choosePrintServers', [printServerIds]);
  }

  /** @override */
  getPrintServersConfig() {
    return sendWithPromise('getPrintServersConfig');
  }
}

addSingletonGetter(NativeLayerCrosImpl);
