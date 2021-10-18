// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Minimal TypeScript definitions for Print Preview.
// TODO(rbpotter): Remove this file once pdf_scripting_api.js is migrated to
// TypeScript.

export interface PDFPlugin extends HTMLIFrameElement {
  darkModeChanged(darkMode: boolean): void;
  hideToolbar(): void;
  loadPreviewPage(url: string, index: number): void;
  resetPrintPreviewMode(
      url: string, color: boolean, pages: number[], modifiable: boolean): void;
  scrollPosition(x: number, y: number): void;
  sendKeyEvent(e: KeyboardEvent): void;
  setKeyEventCallback(callback: (e: KeyboardEvent) => void): void;
  setLoadCompleteCallback(callback: (success: boolean) => void): void;
  setViewportChangedCallback(
      callback:
          (pageX: number, pageY: number, pageWidth: number,
           viewportWidth: number, viewportHeight: number) => void): void;
}

export function PDFCreateOutOfProcessPlugin(
    src: string, baseUrl: string): PDFPlugin;
