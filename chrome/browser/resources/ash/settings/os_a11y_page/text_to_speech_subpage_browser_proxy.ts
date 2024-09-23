// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface TextToSpeechSubpageBrowserProxy {
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

  showChromeVoxTutorial(): void {
    chrome.send('showChromeVoxTutorial');
  }
}
