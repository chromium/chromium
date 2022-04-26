// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

export type DataCollectorItem = {
  name: string,
  isIncluded: boolean,
  protoEnum: number,
};

export type IssueDetails = {
  caseId: string,
  emailAddress: string,
  issueDescription: string,
};

export type PIIDataItem = {
  piiTypeDescription: string,
  piiType: number,
  detectedData: string,
  count: number,
  keep: boolean,
  expandDetails: boolean,
};

export type StartDataCollectionResult = {
  success: boolean,
  errorMessage: string,
};

export type UrlGenerationResult = {
  success: boolean,
  url: string,
  errorMessage: string,
};

export interface BrowserProxy {
  /**
   * Gets the list of email addresses that are logged in from C++ side.
   */
  getEmailAddresses(): Promise<string[]>;

  getDataCollectors(): Promise<DataCollectorItem[]>;

  getAllDataCollectors(): Promise<DataCollectorItem[]>;

  startDataCollection(
      issueDetails: IssueDetails, selectedDataCollectors: DataCollectorItem[]):
      Promise<StartDataCollectionResult>;

  cancelDataCollection(): void;

  startDataExport(piiItems: PIIDataItem[]): void;

  showExportedDataInFolder(): void;

  generateCustomizedURL(caseId: string, dataCollectors: DataCollectorItem[]):
      Promise<UrlGenerationResult>;
}

export class BrowserProxyImpl implements BrowserProxy {
  getEmailAddresses() {
    return sendWithPromise('getEmailAddresses');
  }

  getDataCollectors() {
    return sendWithPromise('getDataCollectors');
  }

  getAllDataCollectors() {
    return sendWithPromise('getAllDataCollectors');
  }

  startDataCollection(
      issueDetails: IssueDetails, dataCollectors: DataCollectorItem[]) {
    return sendWithPromise('startDataCollection', issueDetails, dataCollectors);
  }

  cancelDataCollection() {
    chrome.send('cancelDataCollection');
  }

  startDataExport(piiItems: PIIDataItem[]) {
    chrome.send('startDataExport', [piiItems]);
  }

  showExportedDataInFolder() {
    chrome.send('showExportedDataInFolder');
  }

  generateCustomizedURL(caseId: string, dataCollectors: DataCollectorItem[]) {
    return sendWithPromise('generateCustomizedURL', caseId, dataCollectors);
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
let instance: BrowserProxy|null = null;