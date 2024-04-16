// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export type PdfOcrPrefCallback = chrome.pdfViewerPrivate.PdfOcrPrefCallback;

// TODO(crbug.com/40825351): Move the other chrome.pdfViewerPrivate calls across
// the PDF UI under this proxy.
// `chrome.pdfViewerPrivate.isAllowedLocalFileAccess` is currently located in
// `chrome/browser/resources/pdf/navigator.ts`.
interface PdfViewerPrivateProxy {
  isPdfOcrAlwaysActive(): Promise<boolean>;
  setPdfDocumentTitle(title: string): void;
  setPdfOcrPref(value: boolean): Promise<boolean>;
  addPdfOcrPrefChangedListener(listener: PdfOcrPrefCallback): void;
  removePdfOcrPrefChangedListener(listener: PdfOcrPrefCallback): void;
}

export class PdfViewerPrivateProxyImpl implements PdfViewerPrivateProxy {
  isPdfOcrAlwaysActive(): Promise<boolean> {
    return new Promise(resolve => {
      chrome.pdfViewerPrivate.isPdfOcrAlwaysActive(result => resolve(result));
    });
  }

  setPdfDocumentTitle(title: string): void {
    chrome.pdfViewerPrivate.setPdfDocumentTitle(title);
  }

  setPdfOcrPref(value: boolean): Promise<boolean> {
    return new Promise(resolve => {
      chrome.pdfViewerPrivate.setPdfOcrPref(value, result => resolve(result));
    });
  }

  addPdfOcrPrefChangedListener(listener: PdfOcrPrefCallback): void {
    chrome.pdfViewerPrivate.onPdfOcrPrefChanged.addListener(listener);
  }

  removePdfOcrPrefChangedListener(listener: PdfOcrPrefCallback): void {
    chrome.pdfViewerPrivate.onPdfOcrPrefChanged.removeListener(listener);
  }

  static getInstance(): PdfViewerPrivateProxy {
    return instance || (instance = new PdfViewerPrivateProxyImpl());
  }
}

let instance: PdfViewerPrivateProxy|null = null;
