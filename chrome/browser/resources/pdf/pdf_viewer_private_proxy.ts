// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/40825351): Move the other chrome.pdfViewerPrivate calls across
// the PDF UI under this proxy.
// `chrome.pdfViewerPrivate.isAllowedLocalFileAccess` is currently located in
// `chrome/browser/resources/pdf/navigator.ts`.
interface PdfViewerPrivateProxy {
  setPdfDocumentTitle(title: string): void;
}

export class PdfViewerPrivateProxyImpl implements PdfViewerPrivateProxy {
  setPdfDocumentTitle(title: string): void {
    chrome.pdfViewerPrivate.setPdfDocumentTitle(title);
  }

  static getInstance(): PdfViewerPrivateProxy {
    return instance || (instance = new PdfViewerPrivateProxyImpl());
  }
}

let instance: PdfViewerPrivateProxy|null = null;
