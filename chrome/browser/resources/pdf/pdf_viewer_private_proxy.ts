// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="enable_pdf_save_to_drive">
import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';

type SaveRequestType = chrome.pdfViewerPrivate.SaveRequestType;
type SaveToDriveProgress = chrome.pdfViewerPrivate.SaveToDriveProgress;
// </if> enable_pdf_save_to_drive

// <if expr="enable_pdf_ink2">
type GetTextInfoResult = chrome.pdfViewerPrivate.GetTextInfoResult;
// </if>

// TODO(crbug.com/40825351): Move the other chrome.pdfViewerPrivate calls across
// the PDF UI under this proxy.
// `chrome.pdfViewerPrivate.isAllowedLocalFileAccess` is currently located in
// `chrome/browser/resources/pdf/navigator.ts`.
export interface PdfViewerPrivateProxy {
  // <if expr="enable_pdf_ink2">
  getTextInfo(textarea: HTMLTextAreaElement, knownFontIds: number[]):
      Promise<GetTextInfoResult>;
  // </if>
  // <if expr="enable_pdf_save_to_drive">
  onSaveToDriveProgress:
      ChromeEvent<(url: string, progress: SaveToDriveProgress) => void>;

  saveToDrive(saveRequestType?: SaveRequestType): void;
  // </if>
  glicSummarize(): void;
  setPdfDocumentTitle(title: string): void;
}

export class PdfViewerPrivateProxyImpl implements PdfViewerPrivateProxy {
  // <if expr="enable_pdf_ink2">
  getTextInfo(textarea: HTMLTextAreaElement, knownFontIds: number[]):
      Promise<GetTextInfoResult> {
    return chrome.pdfViewerPrivate.getTextInfo(textarea, knownFontIds);
  }
  // </if>

  // <if expr="enable_pdf_save_to_drive">
  onSaveToDriveProgress:
      ChromeEvent<(url: string, progress: SaveToDriveProgress) => void> =
          chrome.pdfViewerPrivate.onSaveToDriveProgress;

  saveToDrive(saveRequestType?: SaveRequestType): void {
    chrome.pdfViewerPrivate.saveToDrive(saveRequestType);
  }
  // </if>

  glicSummarize(): void {
    chrome.pdfViewerPrivate.glicSummarize();
  }

  setPdfDocumentTitle(title: string): void {
    chrome.pdfViewerPrivate.setPdfDocumentTitle(title);
  }

  static getInstance(): PdfViewerPrivateProxy {
    return instance || (instance = new PdfViewerPrivateProxyImpl());
  }

  static setInstance(obj: PdfViewerPrivateProxy) {
    instance = obj;
  }
}

let instance: PdfViewerPrivateProxy|null = null;
