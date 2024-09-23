// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface DataCollectorItem {
  name: string;
  isIncluded: boolean;
  protoEnum: number;
}

export interface IssueDetails {
  caseId: string;
  emailAddress: string;
  issueDescription: string;
}

export interface PiiDataItem {
  piiTypeDescription: string;
  piiType: number;
  detectedData: string[];
  count: number;
  keep: boolean;
  expandDetails: boolean;
}

export interface StartDataCollectionResult {
  success: boolean;
  errorMessage: string;
}

export interface SupportTokenGenerationResult {
  success: boolean;
  // It will be filled only if `success` is true.
  token: string;
  errorMessage: string;
}

export interface BrowserProxy {
  /**
   * Gets the list of email addresses that are logged in from C++ side.
   */
  getEmailAddresses(): Promise<string[]>;

  getDataCollectors(): Promise<DataCollectorItem[]>;

  getAllDataCollectors(): Promise<DataCollectorItem[]>;

  startDataCollection(
      issueDetails: IssueDetails, selectedDataCollectors: DataCollectorItem[],
      screenshotBase64: string): Promise<StartDataCollectionResult>;

  takeScreenshot(): void;

  cancelDataCollection(): void;

  startDataExport(piiItems: PiiDataItem[]): void;

  showExportedDataInFolder(): void;

  generateCustomizedUrl(caseId: string, dataCollectors: DataCollectorItem[]):
      Promise<SupportTokenGenerationResult>;

  generateSupportToken(dataCollectors: DataCollectorItem[]):
      Promise<SupportTokenGenerationResult>;
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

  takeScreenshot() {
    chrome.send('takeScreenshot');
  }

  startDataCollection(
      issueDetails: IssueDetails, dataCollectors: DataCollectorItem[],
      screenshotBase64: string) {
    return sendWithPromise(
        'startDataCollection', issueDetails, dataCollectors, screenshotBase64);
  }

  cancelDataCollection() {
    chrome.send('cancelDataCollection');
  }

  startDataExport(piiItems: PiiDataItem[]) {
    chrome.send('startDataExport', [piiItems]);
  }

  showExportedDataInFolder() {
    chrome.send('showExportedDataInFolder');
  }

  generateCustomizedUrl(caseId: string, dataCollectors: DataCollectorItem[]) {
    return sendWithPromise('generateCustomizedUrl', caseId, dataCollectors);
  }

  generateSupportToken(dataCollectors: DataCollectorItem[]) {
    return sendWithPromise('generateSupportToken', dataCollectors);
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