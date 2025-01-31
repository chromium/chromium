// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NativeLayerCros, PrintServer, PrintServersConfig} from '../native_layer_cros.js';
import {NativeLayerCrosImpl} from '../native_layer_cros.js';

import type {DestinationStore} from './destination_store.js';

export class PrintServerStore extends EventTarget {
  /**
   * Used to fetch print servers.
   */
  private nativeLayerCros_: NativeLayerCros = NativeLayerCrosImpl.getInstance();

  /**
   * All available print servers mapped by name.
   */
  private printServersByName_: Map<string, PrintServer[]> = new Map();

  /**
   * Whether in single print server fetching mode.
   */
  private isSingleServerFetchingMode_: boolean = false;

  /**
   * Used to reload local printers.
   */
  private destinationStore_: DestinationStore|null = null;

  /**
   * A data store that stores print servers and dispatches events when the
   * data store changes.
   * @param addListenerCallback Function to call to add Web UI listeners in
   *     PrintServerStore constructor.
   */
  constructor(
      addListenerCallback:
          (eventName: string, listener: (p: any) => void) => void) {
    super();

    addListenerCallback(
        'print-servers-config-changed',
        (printServersConfig: PrintServersConfig) =>
            this.onPrintServersConfigChanged_(printServersConfig));
    addListenerCallback(
        'server-printers-loading',
        (isLoading: boolean) => this.onServerPrintersLoading_(isLoading));
  }

  /**
   * Selects the print server(s) with the corresponding name.
   * @param printServerName Name of the print server(s).
   */
  choosePrintServers(printServerName: string) {
    const printServers = this.printServersByName_.get(printServerName);
    this.nativeLayerCros_.choosePrintServers(
        printServers ? printServers.map(printServer => printServer.id) : []);
  }

  /**
   * Gets the currently available print servers and fetching mode.
   * @return The print servers configuration.
   */
  async getPrintServersConfig(): Promise<PrintServersConfig> {
    const printServersConfig =
        await this.nativeLayerCros_.getPrintServersConfig();
    this.updatePrintServersConfig_(printServersConfig);
    return printServersConfig;
  }

  setDestinationStore(destinationStore: DestinationStore) {
    this.destinationStore_ = destinationStore;
  }

  /**
   * Called when new print servers and fetching mode are available.
   */
  private onPrintServersConfigChanged_(printServersConfig: PrintServersConfig) {
    this.updatePrintServersConfig_(printServersConfig);
    const eventData = {
      printServerNames: Array.from(this.printServersByName_.keys()),
      isSingleServerFetchingMode: this.isSingleServerFetchingMode_,
    };
    this.dispatchEvent(new CustomEvent(
        PrintServerStoreEventType.PRINT_SERVERS_CHANGED, {detail: eventData}));
  }

  /**
   * Updates the print servers configuration when new print servers and fetching
   * mode are available.
   */
  private updatePrintServersConfig_(printServersConfig: PrintServersConfig) {
    this.isSingleServerFetchingMode_ =
        printServersConfig.isSingleServerFetchingMode;
    this.printServersByName_ = new Map();
    for (const printServer of printServersConfig.printServers) {
      if (this.printServersByName_.has(printServer.name)) {
        this.printServersByName_.get(printServer.name)!.push(printServer);
      } else {
        this.printServersByName_.set(printServer.name, [printServer]);
      }
    }
  }

  /**
   * Called when print server printers loading status has changed.
   * @param isLoading Whether server printers are loading
   */
  private async onServerPrintersLoading_(isLoading: boolean) {
    if (!isLoading && this.destinationStore_) {
      await this.destinationStore_.reloadLocalPrinters();
    }
    this.dispatchEvent(new CustomEvent(
        PrintServerStoreEventType.SERVER_PRINTERS_LOADING,
        {detail: isLoading}));
  }
}

/**
 * Event types dispatched by the print server store.
 */
export enum PrintServerStoreEventType {
  PRINT_SERVERS_CHANGED = 'PrintServerStore.PRINT_SERVERS_CHANGED',
  SERVER_PRINTERS_LOADING = 'PrintServerStore.SERVER_PRINTERS_LOADING',
}
