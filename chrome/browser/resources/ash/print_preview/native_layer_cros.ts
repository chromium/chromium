// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

import type {Cdd} from './data/cdd.js';
import type {ExtensionDestinationInfo, LocalDestinationInfo} from './data/local_parsers.js';
import type {PrintAttemptOutcome, PrinterStatus} from './data/printer_status_cros.js';

export interface PrinterSetupResponse {
  printerId: string;
  capabilities: Cdd;
}

export interface PrintServer {
  id: string;
  name: string;
}

export interface PrintServersConfig {
  printServers: PrintServer[];
  isSingleServerFetchingMode: boolean;
}

/**
 * An interface to the Chrome OS platform specific part of the native Chromium
 * printing system layer.
 */
export interface NativeLayerCros {
  /**
   * Requests the destination's end user license information. Returns a promise
   * that will be resolved with the destination's EULA URL if obtained
   * successfully.
   * @param destinationId ID of the destination.
   */
  getEulaUrl(destinationId: string): Promise<string>;

  /**
   * Requests Chrome to resolve provisional extension destination by granting
   * the provider extension access to the printer.
   * @param provisionalDestinationId
   */
  grantExtensionPrinterAccess(provisionalDestinationId: string):
      Promise<ExtensionDestinationInfo>;

  /**
   * Requests that Chrome perform printer setup for the given printer.
   */
  setupPrinter(printerId: string): Promise<PrinterSetupResponse>;

  /**
   * Sends a request to the printer with id |printerId| for its current status.
   */
  requestPrinterStatusUpdate(printerId: string): Promise<PrinterStatus>;

  /**
   * Selects all print servers with ids in |printServerIds| to query for their
   * printers.
   */
  choosePrintServers(printServerIds: string[]): void;

  /**
   * Gets the available print servers and whether we are in single server
   * fetching mode.
   */
  getPrintServersConfig(): Promise<PrintServersConfig>;

  /**
   * Records the `PrintPreview.PrintAttemptOutcome` histogram for capturing
   * the result from opening Print Preview.
   */
  recordPrintAttemptOutcome(printAttemptOutcome: PrintAttemptOutcome): void;

  /**
   * Returns whether or not the manage printers button should be displayed for
   * the given print preview initiator.
   */
  getShowManagePrinters(): Promise<boolean>;

  /**
   * Observes the LocalPrinterObserver then returns the current list of local
   * printers.
   */
  observeLocalPrinters(): Promise<LocalDestinationInfo[]>;
}

export class NativeLayerCrosImpl implements NativeLayerCros {
  getEulaUrl(destinationId: string) {
    return sendWithPromise('getEulaUrl', destinationId);
  }

  grantExtensionPrinterAccess(provisionalDestinationId: string) {
    return sendWithPromise(
        'grantExtensionPrinterAccess', provisionalDestinationId);
  }

  setupPrinter(printerId: string) {
    return sendWithPromise('setupPrinter', printerId);
  }

  requestPrinterStatusUpdate(printerId: string) {
    return sendWithPromise('requestPrinterStatus', printerId);
  }

  choosePrintServers(printServerIds: string[]) {
    chrome.send('choosePrintServers', [printServerIds]);
  }

  getPrintServersConfig() {
    return sendWithPromise('getPrintServersConfig');
  }

  recordPrintAttemptOutcome(printAttemptOutcome: PrintAttemptOutcome) {
    chrome.send('recordPrintAttemptOutcome', [printAttemptOutcome]);
  }

  getShowManagePrinters() {
    return sendWithPromise('getShowManagePrinters');
  }

  observeLocalPrinters() {
    return sendWithPromise('observeLocalPrinters');
  }

  static getInstance(): NativeLayerCros {
    return instance || (instance = new NativeLayerCrosImpl());
  }

  static setInstance(obj: NativeLayerCros) {
    instance = obj;
  }
}

let instance: NativeLayerCros|null = null;
