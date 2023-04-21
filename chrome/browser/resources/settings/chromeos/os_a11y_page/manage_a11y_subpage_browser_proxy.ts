// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface ManageA11ySubpageBrowserProxy {
  /**
   * Opens the options page for Chrome Vox.
   */
  showChromeVoxSettings(): void;

  /**
   * Opens the options page for select to speak.
   */
  showSelectToSpeakSettings(): void;

  /**
   * Sets the startup sound to enabled.
   */
  setStartupSoundEnabled(enabled: boolean): void;

  /**
   * Records the value of the show shelf navigation button.
   */
  recordSelectedShowShelfNavigationButtonValue(enabled: boolean): void;

  /**
   * Requests whether startup sound and tablet mode are enabled. Result
   * is returned by the 'initial-data-ready' WebUI listener event.
   */
  manageA11yPageReady(): void;

  /**
   * Opens the ChromeVox tutorial.
   */
  showChromeVoxTutorial(): void;
}

let instance: ManageA11ySubpageBrowserProxy|null = null;

export class ManageA11ySubpageBrowserProxyImpl implements
    ManageA11ySubpageBrowserProxy {
  static getInstance(): ManageA11ySubpageBrowserProxy {
    return instance || (instance = new ManageA11ySubpageBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: ManageA11ySubpageBrowserProxy): void {
    instance = obj;
  }

  showChromeVoxSettings(): void {
    chrome.send('showChromeVoxSettings');
  }

  showSelectToSpeakSettings(): void {
    chrome.send('showSelectToSpeakSettings');
  }

  setStartupSoundEnabled(enabled: boolean): void {
    chrome.send('setStartupSoundEnabled', [enabled]);
  }

  recordSelectedShowShelfNavigationButtonValue(enabled: boolean): void {
    chrome.send('recordSelectedShowShelfNavigationButtonValue', [enabled]);
  }

  manageA11yPageReady(): void {
    chrome.send('manageA11yPageReady');
  }

  showChromeVoxTutorial(): void {
    chrome.send('showChromeVoxTutorial');
  }
}
