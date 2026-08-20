// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for accessing and updating visual presentation, layout, and theme
// settings (such as fonts, line and letter spacing, color themes, line focus,
// and presentation states) in Read Anything.
export interface VisualBrowserProxy {
  getInSidePanelPresentationState(): number;
  getInImmersiveOverlayPresentationState(): number;

  togglePresentation(): void;
}

export class VisualBrowserProxyImpl implements VisualBrowserProxy {
  getInSidePanelPresentationState(): number {
    return chrome.readingMode.inSidePanelPresentationState;
  }

  getInImmersiveOverlayPresentationState(): number {
    return chrome.readingMode.inImmersiveOverlayPresentationState;
  }

  togglePresentation(): void {
    chrome.readingMode.togglePresentation();
  }

  static getInstance(): VisualBrowserProxy {
    return instance || (instance = new VisualBrowserProxyImpl());
  }

  static setInstance(obj: VisualBrowserProxy) {
    instance = obj;
  }
}

let instance: VisualBrowserProxy|null = null;
