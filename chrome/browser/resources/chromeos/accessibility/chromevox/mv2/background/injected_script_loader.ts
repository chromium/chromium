// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Responsible for loading scripts into the inject context.
 */

export class InjectedScriptLoader {

  private code_: { [key: string]: string } = {};
  private constructor() { }

  static injectContentScriptForGoogleDocs(): void {
    // Build a regexp to match all allowed urls.
    let matches = [];
    try {
      matches = chrome.runtime.getManifest()['content_scripts'][0]['matches'];
    } catch (e) {
      throw new Error(
        'Unable to find content script matches entry in manifest.');
    }

    // Build one large regexp.
    const docsRe = new RegExp(matches.join('|'));

    // Inject the content script into all running tabs allowed by the
    // manifest. This block is still necessary because the extension system
    // doesn't re-inject content scripts into already running tabs.
    chrome.windows.getAll({ 'populate': true }, (windows: chrome.windows.Window[]) => {
      for (let i = 0; i < windows.length; i++) {
        // TODO(b/314203187): Determine if not null assertion is acceptable.
        const tabs = windows[i].tabs!.filter(tab => docsRe.test(tab.url!));
        InjectedScriptLoader.injectContentScript_(tabs);
      }
    });
  }

  /**
   * Loads a dictionary of file contents for Javascript files.
   * @param files A list of file names.
   * @private
   */
  private async fetchCode_(files: string[]): Promise<void> {
    Promise.all(files.map(file => this.loadScriptAsCode_(file)));
  }

  private async loadScriptAsCode_(fileName: string): Promise<void> {
    if (this.code_[fileName]) {
      return;
    }

    // Load the script by fetching its source and running 'eval' on it
    // directly, with a magic comment that makes Chrome treat it like it
    // loaded normally. Wait until it's fetched before loading the
    // next script.
    const url = chrome.extension.getURL(fileName) + '?' + new Date().getTime();
    const response = await fetch(url);
    if (response.ok) {
      let scriptText = await response.text();
      // Add a magic comment to the bottom of the file so that
      // Chrome knows the name of the script in the JavaScript debugger.
      const debugSrc = fileName.replace('closure/../', '');
      // The 'chromevox' id is only used in the DevTools instead of a long
      // extension id.
      scriptText +=
        '\n//# sourceURL= chrome-extension://chromevox/' + debugSrc + '\n';
      this.code_[fileName] = scriptText;
    } else {
      // Cause the promise created by this async function to reject.
      throw new Error(`${response.status}: ${response.statusText}`);
    }
  }

  private async executeCodeInAllTabs_(tabs: chrome.tabs.Tab[]): Promise<void> {
    for (const tab of tabs) {
      // Inject the ChromeVox content script code into the tab.
      await Promise.all(
        Object.values(this.code_).map(script => this.execute_(script, tab)));
    }
  }

  /**
   * Inject the content scripts into already existing tabs.
   * @param tabs The tabs where ChromeVox scripts should be
   *     injected.
   * @private
   */
  private static async injectContentScript_(tabs: chrome.tabs.Tab[]): Promise<void> {
    if (!InjectedScriptLoader.instance) {
      InjectedScriptLoader.instance = new InjectedScriptLoader();
    }
    await InjectedScriptLoader.instance.fetchCode_(contentScriptFiles);
    await InjectedScriptLoader.instance.executeCodeInAllTabs_(tabs);
  }

  private async execute_(code: string, tab: chrome.tabs.Tab): Promise<void> {
    await new Promise(
      resolve => chrome.tabs.executeScript(
        tab.id, { code, allFrames: true }, resolve));
    if (chrome.runtime.lastError) {
      console.error('Could not inject into tab', tab);
    }
  }
}

export namespace InjectedScriptLoader {
  export let instance: InjectedScriptLoader;
}

// Local to module.

const contentScriptFiles =
  chrome.runtime.getManifest()['content_scripts'][0]['js'];
