// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

import {NativeLayerCros, NativeLayerCrosImpl, PrintServer, PrintServersConfig} from '../native_layer_cros.js';

export class PrintServerStore extends EventTarget {
  /**
   * A data store that stores print servers and dispatches events when the
   * data store changes.
   * @param {function(string, !Function):void} addListenerCallback Function
   *     to call to add Web UI listeners in PrintServerStore constructor.
   */
  constructor(addListenerCallback) {
    super();

    /**
     * Used to fetch print servers.
     * @private {!NativeLayerCros}
     */
    this.nativeLayer_ = NativeLayerCrosImpl.getInstance();

    /**
     * All available print servers mapped by name.
     * @private {!Map<string, !Array<!PrintServer>>}
     */
    this.printServersByName_ = new Map();

    /**
     * Whether in single print server fetching mode.
     * @private {boolean}
     */
    this.isSingleServerFetchingMode_ = false;

    addListenerCallback(
        'print-servers-config-changed',
        printServersConfig =>
            this.onPrintServersConfigChanged_(printServersConfig));
    addListenerCallback(
        'server-printers-loading',
        isLoading => this.onServerPrintersLoading_(isLoading));
  }

  /**
   * Selects the print server(s) with the corresponding name.
   * @param {string} printServerName Name of the print server(s).
   */
  choosePrintServers(printServerName) {
    const printServers = this.printServersByName_.get(printServerName);
    this.nativeLayer_.choosePrintServers(
        printServers ? printServers.map(printServer => printServer.id) : []);
  }

  /**
   * Gets the currently available print servers and fetching mode.
   * @return {!Promise<!PrintServersConfig>} The print servers configuration.
   */
  getPrintServersConfig() {
    return this.nativeLayer_.getPrintServersConfig();
  }

  /**
   * Called when new print servers and fetching mode are available.
   * @param {!PrintServersConfig} printServersConfig The print servers
   *     configuration.
   */
  onPrintServersConfigChanged_(printServersConfig) {
    this.isSingleServerFetchingMode_ =
        printServersConfig.isSingleServerFetchingMode;
    this.printServersByName_ = new Map();
    for (const printServer of printServersConfig.printServers) {
      if (this.printServersByName_.has(printServer.name)) {
        this.printServersByName_.get(printServer.name).push(printServer);
      } else {
        this.printServersByName_.set(printServer.name, [printServer]);
      }
    }
    const eventData = {
      printServerNames: Array.from(this.printServersByName_.keys()),
      isSingleServerFetchingMode: this.isSingleServerFetchingMode_
    };
    this.dispatchEvent(new CustomEvent(
        PrintServerStore.EventType.PRINT_SERVERS_CHANGED, {detail: eventData}));
  }

  /**
   * Called when print server printers loading status has changed.
   * @param {boolean} isLoading Whether server printers are loading
   */
  onServerPrintersLoading_(isLoading) {
    this.dispatchEvent(new CustomEvent(
        PrintServerStore.EventType.SERVER_PRINTERS_LOADING,
        {detail: isLoading}));
  }
}

/**
 * Event types dispatched by the print server store.
 * @enum {string}
 */
PrintServerStore.EventType = {
  PRINT_SERVERS_CHANGED: 'PrintServerStore.PRINT_SERVERS_CHANGED',
  SERVER_PRINTERS_LOADING: 'PrintServerStore.SERVER_PRINTERS_LOADING',
};
