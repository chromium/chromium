// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Responsible for loading scripts into the inject context.
 */

export class InjectedScriptLoader {
  /** @private */
  constructor() {
    /** @private {!Object<string,string>} */
    this.code_ = {};
    /** @private {!Object<function()>} */
    this.resolveXhr_ = {};
  }

  /**
   * Inject the content scripts into already existing tabs.
   * @param {!Array<!Tab>} tabs The tabs where ChromeVox scripts should be
   *     injected.
   */
  static async injectContentScript(tabs) {
    if (!InjectedScriptLoader.instance) {
      InjectedScriptLoader.instance = new InjectedScriptLoader();
    }
    await InjectedScriptLoader.instance.fetchCode_(contentScriptFiles);
    await InjectedScriptLoader.instance.executeCodeInAllTabs_(tabs);
  }

  /**
   * Loads a dictionary of file contents for Javascript files.
   * @param {Array<string>} files A list of file names.
   * @private
   */
  async fetchCode_(files) {
    return Promise.all(files.map(file => this.loadScriptAsCode_(file)));
  }

  /**
   * @param {string} fileName
   * @private
   */
  async loadScriptAsCode_(fileName) {
    if (this.code_[fileName]) {
      return this.code_[fileName];
    }

    const loaded = new Promise(resolve => this.resolveXhr_[fileName] = resolve);
    // Load the script by fetching its source and running 'eval' on it
    // directly, with a magic comment that makes Chrome treat it like it
    // loaded normally. Wait until it's fetched before loading the
    // next script.
    const xhr = new XMLHttpRequest();
    xhr.onreadystatechange = () => this.xhrMaybeReady_(xhr, fileName);
    const url = chrome.extension.getURL(fileName) + '?' + new Date().getTime();
    xhr.open('GET', url);
    xhr.send(null);
    return loaded;
  }

  /**
   * @param {XMLHttpRequest} xhr
   * @param {string} fileName
   * @private
   */
  xhrMaybeReady_(xhr, fileName) {
    if (xhr.readyState === 4) {
      let scriptText = xhr.responseText;
      // Add a magic comment to the bottom of the file so that
      // Chrome knows the name of the script in the JavaScript debugger.
      const debugSrc = fileName.replace('closure/../', '');
      // The 'chromevox' id is only used in the DevTools instead of a long
      // extension id.
      scriptText +=
          '\n//# sourceURL= chrome-extension://chromevox/' + debugSrc + '\n';
      this.code_[fileName] = scriptText;
      this.markAsLoaded_(fileName);
    }
  }

  /**
   * @param {string} fileName
   * @private
   */
  markAsLoaded_(fileName) {
    if (!this.resolveXhr_[fileName]) {
      return;
    }
    const callback = this.resolveXhr_[fileName];
    delete this.resolveXhr_[fileName];
    callback();
  }

  /**
   * @param {!Array<!Tab>} tabs
   * @private
   */
  async executeCodeInAllTabs_(tabs) {
    for (const tab of tabs) {
      // Inject the ChromeVox content script code into the tab.
      await Promise.all(this.code_.map(script => this.execute_(script, tab)));
    }
  }

  /**
   * @param {string} code
   * @param {!Tab} tab
   * @private
   */
  async execute_(code, tab) {
    await new Promise(
        resolve => chrome.tabs.executeScript(
            tab.id, {code, allFrames: true}, resolve));
    if (chrome.extension.lastError) {
      console.error('Could not inject into tab', tab);
    }
  }
}

/** @type {InjectedScriptLoader} */
InjectedScriptLoader.instance;

// Local to module.

const contentScriptFiles =
    chrome.runtime.getManifest()['content_scripts'][0]['js'];
