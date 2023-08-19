// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface TextToSpeechSubpageBrowserProxy {
  /**
   * Request whether ScreenAIInstallState changed. Result is returned by the
   * 'pdf-ocr-state-changed' WebUI listener event.
   */
  pdfOcrSectionReady(): void;

  /**
   * Opens the options page for Chrome Vox.
   */
  showChromeVoxSettings(): void;

  /**
   * Opens the options page for select to speak.
   */
  showSelectToSpeakSettings(): void;

  /**
   * Opens the ChromeVox tutorial.
   */
  showChromeVoxTutorial(): void;
}

let instance: TextToSpeechSubpageBrowserProxy|null = null;

export class TextToSpeechSubpageBrowserProxyImpl implements
    TextToSpeechSubpageBrowserProxy {
  static getInstance(): TextToSpeechSubpageBrowserProxy {
    return instance || (instance = new TextToSpeechSubpageBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: TextToSpeechSubpageBrowserProxy): void {
    instance = obj;
  }

  pdfOcrSectionReady(): void {
    chrome.send('pdfOcrSectionReady');
  }

  showChromeVoxSettings(): void {
    chrome.send('showChromeVoxSettings');
  }

  showSelectToSpeakSettings(): void {
    chrome.send('showSelectToSpeakSettings');
  }

  showChromeVoxTutorial(): void {
    chrome.send('showChromeVoxTutorial');
  }
}
