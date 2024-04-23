// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface TetheringStatus {
  state?: string;
}

export interface NetworkUiBrowserProxy {
  addNetwork(type: string): void;

  getShillDeviceProperties(type: string): Promise<any[]>;

  getShillEthernetEap(): Promise<any[]>;

  getShillNetworkProperties(guid: string): Promise<any[]>;

  getFirstWifiNetworkProperties(): Promise<any[]>;

  importOnc(content: string): Promise<[string, boolean]>;

  openCellularActivationUi(): Promise<[boolean]>;

  resetEsimCache(): void;

  setShillDebugging(debugging: string): Promise<[string, boolean]>;

  showAddNewWifi(): void;

  showNetworkConfig(guid: string): void;

  showNetworkDetails(guid: string): void;

  storeLogs(options: Object): Promise<[string, boolean]>;

  getHostname(): Promise<string>;

  setHostname(hostname: string): void;

  disableActiveEsimProfile(): void;

  resetEuicc(): void;

  resetApnMigrator(): void;

  getTetheringCapabilities(): Promise<string>;

  getTetheringStatus(): Promise<TetheringStatus>;

  getTetheringConfig(): Promise<string>;

  setTetheringConfig(config: string): Promise<string>;

  checkTetheringReadiness(): Promise<string>;

  getWifiDirectCapabilities(): Promise<string>;

  getWifiDirectOwnerInfo(): Promise<string>;

  getWifiDirectClientInfo(): Promise<string>;
}

export class NetworkUiBrowserProxyImpl implements NetworkUiBrowserProxy {
  addNetwork(type: string) {
    chrome.send('addNetwork', [type]);
  }

  getShillDeviceProperties(type: string): Promise<any[]> {
    return sendWithPromise('getShillDeviceProperties', type);
  }

  getShillEthernetEap(): Promise<any[]> {
    return sendWithPromise('getShillEthernetEAP');
  }

  getShillNetworkProperties(guid: string): Promise<any[]> {
    return sendWithPromise('getShillNetworkProperties', guid);
  }

  getFirstWifiNetworkProperties(): Promise<any[]> {
    return sendWithPromise('getFirstWifiNetworkProperties');
  }

  importOnc(content: string): Promise<[string, boolean]> {
    return sendWithPromise('importONC', content);
  }

  openCellularActivationUi(): Promise<[boolean]> {
    return sendWithPromise('openCellularActivationUi');
  }

  resetEsimCache() {
    chrome.send('resetESimCache');
  }

  setShillDebugging(debugging: string): Promise<[string, boolean]> {
    return sendWithPromise('setShillDebugging', debugging);
  }

  showAddNewWifi() {
    chrome.send('showAddNewWifi');
  }

  showNetworkConfig(guid: string) {
    chrome.send('showNetworkConfig', [guid]);
  }

  showNetworkDetails(guid: string) {
    chrome.send('showNetworkDetails', [guid]);
  }

  storeLogs(options: Object): Promise<[string, boolean]> {
    return sendWithPromise('storeLogs', options);
  }

  getHostname(): Promise<string> {
    return sendWithPromise('getHostname');
  }

  setHostname(hostname: string) {
    chrome.send('setHostname', [hostname]);
  }

  disableActiveEsimProfile() {
    chrome.send('disableActiveESimProfile');
  }

  resetEuicc() {
    chrome.send('resetEuicc');
  }

  resetApnMigrator() {
    chrome.send('resetApnMigrator');
  }

  getTetheringCapabilities(): Promise<string> {
    return sendWithPromise('getTetheringCapabilities');
  }

  getTetheringStatus(): Promise<TetheringStatus> {
    return sendWithPromise('getTetheringStatus');
  }

  getTetheringConfig(): Promise<string> {
    return sendWithPromise('getTetheringConfig');
  }

  setTetheringConfig(config: string): Promise<string> {
    return sendWithPromise('setTetheringConfig', config);
  }

  checkTetheringReadiness(): Promise<string> {
    return sendWithPromise('checkTetheringReadiness');
  }

  getWifiDirectCapabilities(): Promise<string> {
    return sendWithPromise('getWifiDirectCapabilities');
  }

  getWifiDirectOwnerInfo(): Promise<string> {
    return sendWithPromise('getWifiDirectOwnerInfo');
  }

  getWifiDirectClientInfo(): Promise<string> {
    return sendWithPromise('getWifiDirectClientInfo');
  }

  static getInstance(): NetworkUiBrowserProxy {
    return instance || (instance = new NetworkUiBrowserProxyImpl());
  }
}

let instance: NetworkUiBrowserProxy|null = null;
