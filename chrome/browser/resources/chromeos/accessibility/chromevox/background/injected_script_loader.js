// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Responsible for loading scripts into the inject context.
 */

export class InjectedScriptLoader {
  /**
   * Inject the content scripts into already existing tabs.
   * @param {!Array<!Tab>} tabs The tab where ChromeVox scripts should be
   *     injected.
   */
  static async injectContentScript(tabs) {
    const listOfFiles =
        chrome.runtime.getManifest()['content_scripts'][0]['js'];

    const loader = new InjectedScriptLoader();
    const code =
        await new Promise(resolve => loader.fetchCode_(listOfFiles, resolve));
    for (const tab of tabs) {
      // Inject the ChromeVox content script code into the tab.
      listOfFiles.forEach(file => loader.execute_(code[file], tab));
    }
  }

  /**
   * Loads a dictionary of file contents for Javascript files.
   * @param {Array<string>} files A list of file names.
   * @param {function(Object<string,string>)} done A function called when all
   *     the files have been loaded. Called with the code map as the first
   *     parameter.
   * @private
   */
  fetchCode_(files, done) {
    const code = {};
    let waiting = files.length;
    const startTime = new Date();
    const loadScriptAsCode = function(src) {
      // Load the script by fetching its source and running 'eval' on it
      // directly, with a magic comment that makes Chrome treat it like it
      // loaded normally. Wait until it's fetched before loading the
      // next script.
      const xhr = new XMLHttpRequest();
      const url = chrome.extension.getURL(src) + '?' + new Date().getTime();
      xhr.onreadystatechange = function() {
        if (xhr.readyState === 4) {
          let scriptText = xhr.responseText;
          // Add a magic comment to the bottom of the file so that
          // Chrome knows the name of the script in the JavaScript debugger.
          const debugSrc = src.replace('closure/../', '');
          // The 'chromevox' id is only used in the DevTools instead of a long
          // extension id.
          scriptText += '\n//# sourceURL= chrome-extension://chromevox/' +
              debugSrc + '\n';
          code[src] = scriptText;
          waiting--;
          if (waiting === 0) {
            done(code);
          }
        }
      };
      xhr.open('GET', url);
      xhr.send(null);
    };

    files.forEach(file => loadScriptAsCode(file));
  }

  /**
   * A helper function which executes code.
   * @param {string} code The code to execute.
   * @return {!Promise}
   * @private
   */
  async execute_(code, tab) {
    await new Promise(
        resolve => chrome.tabs.executeScript(
            tab.id, {code, 'allFrames': true}, resolve));
    if (!chrome.extension.lastError) {
      return;
    }
    console.error('Could not inject into tab', tab);
  }
}
