// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';

import {BrowserBridge} from './browser_bridge.js';
import {DivView} from './view.js';

/** @type {?SharedDictionaryView} */
let instance = null;

/**
 * This view displays information about Shared Dictionary:
 */
export class SharedDictionaryView extends DivView {
  constructor() {
    super(SharedDictionaryView.MAIN_BOX_ID);

    this.browserBridge_ = BrowserBridge.getInstance();

    this.outputElement = $(SharedDictionaryView.OUTPUT_ID);

    const loadDictionary = async () => {
      this.outputElement.innerHTML = window.trustedTypes.emptyHTML;
      const result = await this.browserBridge_.getSharedDictionaryUsageInfo();
      if (result.length === 0) {
        this.outputElement.appendChild(document.createTextNode('no data'));
        return;
      }
      const resultDiv = document.createElement('div');
      for (const item of result) {
        const isolationKeyDiv = document.createElement('div');
        isolationKeyDiv.appendChild(document.createTextNode(
            `Isolation key : {frame_origin: ${item.frame_origin}, ` +
            `top_frame_site: ${item.top_frame_site}}`));
        resultDiv.appendChild(isolationKeyDiv);

        const diskUsageDiv = document.createElement('div');
        diskUsageDiv.appendChild(document.createTextNode(
            `Total usage: ${item.total_size_bytes} bytes`));
        resultDiv.appendChild(diskUsageDiv);

        const buttonDiv = document.createElement('div');
        const clearButton = document.createElement('button');
        clearButton.innerText = 'Clear';
        clearButton.classList.add(
            'clear-shared-dictionary-button-for-isolation');
        clearButton.addEventListener('click', async () => {
          await this.browserBridge_
              .sendClearSharedDictionaryCacheForIsolationKey(
                  item.frame_origin, item.top_frame_site);
          loadDictionary();
        });
        buttonDiv.appendChild(clearButton);
        resultDiv.appendChild(buttonDiv);

        const dictsPre = document.createElement('pre');
        const dicts = await this.browserBridge_.getSharedDictionaryInfo(
            item.frame_origin, item.top_frame_site);
        dictsPre.appendChild(
            document.createTextNode(JSON.stringify(dicts, undefined, 2)));
        resultDiv.appendChild(dictsPre);
      }
      this.outputElement.appendChild(resultDiv);
    };

    $(SharedDictionaryView.CLEAR_ALL_BUTTON_ID).onclick = async () => {
      await this.browserBridge_.sendClearSharedDictionary();
      loadDictionary();
    };
    $(SharedDictionaryView.RELOAD_BUTTON_ID).onclick = loadDictionary;
    loadDictionary();
  }

  static getInstance() {
    return instance || (instance = new SharedDictionaryView());
  }
}

SharedDictionaryView.TAB_ID = 'tab-handle-shared-dictionary';
SharedDictionaryView.TAB_NAME = 'Shared Dictionaries';
SharedDictionaryView.TAB_HASH = '#sharedDictionary';

// IDs for special HTML elements in index.html
SharedDictionaryView.MAIN_BOX_ID = 'shared-dictionary-view-tab-content';

SharedDictionaryView.CLEAR_ALL_BUTTON_ID = 'shared-dictionary-view-clear-all';
SharedDictionaryView.RELOAD_BUTTON_ID = 'shared-dictionary-reload';
SharedDictionaryView.OUTPUT_ID = 'shared-dictionary-output';
