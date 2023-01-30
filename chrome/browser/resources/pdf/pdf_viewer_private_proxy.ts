// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1302465): Move the other chrome.pdfViewerPrivate calls across
// the PDF UI under this proxy.
// `chrome.pdfViewerPrivate.isAllowedLocalFileAccess` is currently located in
// `chrome/browser/resources/pdf/navigator.ts`.
interface PdfViewerPrivateProxy {
  isPdfOcrAlwaysActive(): Promise<boolean>;
  setPdfOcrPref(value: boolean): Promise<boolean>;
}

export class PdfViewerPrivateProxyImpl implements PdfViewerPrivateProxy {
  isPdfOcrAlwaysActive(): Promise<boolean> {
    return new Promise(resolve => {
      chrome.pdfViewerPrivate.isPdfOcrAlwaysActive(result => resolve(result));
    });
  }

  setPdfOcrPref(value: boolean): Promise<boolean> {
    return new Promise(resolve => {
      chrome.pdfViewerPrivate.setPdfOcrPref(value, result => resolve(result));
    });
  }

  static getInstance(): PdfViewerPrivateProxy {
    return instance || (instance = new PdfViewerPrivateProxyImpl());
  }
}

let instance: PdfViewerPrivateProxy|null = null;
