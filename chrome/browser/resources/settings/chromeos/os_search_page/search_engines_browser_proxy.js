// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * @fileoverview A helper object used from the "Manage search engines" section
 * to interact with the browser.
 */

/**
 * @typedef {{canBeDefault: boolean,
 *            canBeEdited: boolean,
 *            canBeRemoved: boolean,
 *            default: boolean,
 *            displayName: string,
 *            extension: ({id: string,
 *                         name: string,
 *                         canBeDisabled: boolean,
 *                         icon: string}|undefined),
 *            iconURL: (string|undefined),
 *            id: number,
 *            isOmniboxExtension: boolean,
 *            keyword: string,
 *            modelIndex: number,
 *            name: string,
 *            url: string,
 *            urlLocked: boolean}}
 * @see chrome/browser/ui/webui/settings/search_engine_manager_handler.cc
 */
export let SearchEngine;

/**
 * @typedef {{
 *   defaults: !Array<!SearchEngine>,
 *   actives: !Array<!SearchEngine>,
 *   others: !Array<!SearchEngine>,
 *   extensions: !Array<!SearchEngine>
 * }}
 */
export let SearchEnginesInfo;

/** @interface */
export class SearchEnginesBrowserProxy {
  /** @param {number} modelIndex */
  setDefaultSearchEngine(modelIndex) {}

  /** @return {!Promise<!SearchEnginesInfo>} */
  getSearchEnginesList() {}
}

/** @type {?SearchEnginesBrowserProxy} */
let instance = null;

/**
 * @implements {SearchEnginesBrowserProxy}
 */
export class SearchEnginesBrowserProxyImpl {
  /** @return {!SearchEnginesBrowserProxy} */
  static getInstance() {
    return instance || (instance = new SearchEnginesBrowserProxyImpl());
  }

  /** @param {!SearchEnginesBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  setDefaultSearchEngine(modelIndex) {
    chrome.send('setDefaultSearchEngine', [modelIndex]);
  }

  /** @override */
  getSearchEnginesList() {
    return sendWithPromise('getSearchEnginesList');
  }
}
