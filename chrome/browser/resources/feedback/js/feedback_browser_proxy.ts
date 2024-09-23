// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

declare global {
  type GetUserMediaError = Error&{constraintName: string};

  interface Navigator {
    webkitGetUserMedia(
        params: any, callback: (stream?: MediaStream) => void,
        errorCallback: (error: GetUserMediaError) => void): void;
  }
}

export interface FeedbackBrowserProxy {
  getSystemInformation(): Promise<chrome.feedbackPrivate.LogsMapEntry[]>;
  getUserEmail(): Promise<string>;
  getDialogArguments(): string;
  getUserMedia(params: any): Promise<MediaStream|undefined>;

  sendFeedback(
      feedback: chrome.feedbackPrivate.FeedbackInfo, loadSystemInfo?: boolean,
      formOpenTime?: number):
      Promise<chrome.feedbackPrivate.SendFeedbackResult>;

  // Send a message to show the WebDialog
  showDialog(): void;

  // Send a message to close the WebDialog
  closeDialog(): void;

  showSystemInfo(): void;
  showMetrics(): void;
  showAutofillMetadataInfo(autofillMetadata: string): void;
}

export class FeedbackBrowserProxyImpl implements FeedbackBrowserProxy {
  getSystemInformation(): Promise<chrome.feedbackPrivate.LogsMapEntry[]> {
    return new Promise(
        resolve => chrome.feedbackPrivate.getSystemInformation(resolve));
  }

  getUserEmail(): Promise<string> {
    return new Promise(resolve => chrome.feedbackPrivate.getUserEmail(resolve));
  }

  getDialogArguments() {
    return chrome.getVariableValue('dialogArguments');
  }

  getUserMedia(params: any): Promise<MediaStream|undefined> {
    return new Promise((resolve, reject) => {
      navigator.webkitGetUserMedia(
          params, stream => resolve(stream), error => reject(error));
    });
  }

  sendFeedback(
      feedback: chrome.feedbackPrivate.FeedbackInfo, loadSystemInfo?: boolean,
      formOpenTime?: number) {
    return chrome.feedbackPrivate.sendFeedback(
        feedback, loadSystemInfo, formOpenTime);
  }

  showDialog() {
    chrome.send('showDialog');
  }

  closeDialog() {
    chrome.send('dialogClose');
  }

  showSystemInfo() {
    chrome.send('showSystemInfo');
  }

  showMetrics() {
    chrome.send('showMetrics');
  }

  showAutofillMetadataInfo(autofillMetadata: string) {
    chrome.send('showAutofillMetadataInfo', [autofillMetadata]);
  }

  static getInstance(): FeedbackBrowserProxy {
    return instance || (instance = new FeedbackBrowserProxyImpl());
  }

  static setInstance(obj: FeedbackBrowserProxy) {
    instance = obj;
  }
}

let instance: FeedbackBrowserProxy|null = null;
