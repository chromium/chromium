// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
let SearchEngine;

/**
 * @typedef {{
 *   defaults: !Array<!SearchEngine>,
 *   others: !Array<!SearchEngine>,
 *   extensions: !Array<!SearchEngine>
 * }}
 */
let SearchEnginesInfo;

cr.define('settings', function() {
  /** @interface */
  class SearchEnginesBrowserProxy {
    /** @param {number} modelIndex */
    setDefaultSearchEngine(modelIndex) {}

    /** @param {number} modelIndex */
    removeSearchEngine(modelIndex) {}

    /** @param {number} modelIndex */
    searchEngineEditStarted(modelIndex) {}

    searchEngineEditCancelled() {}

    /**
     * @param {string} searchEngine
     * @param {string} keyword
     * @param {string} queryUrl
     */
    searchEngineEditCompleted(searchEngine, keyword, queryUrl) {}

    /** @return {!Promise<!SearchEnginesInfo>} */
    getSearchEnginesList() {}

    /**
     * @param {string} fieldName
     * @param {string} fieldValue
     * @return {!Promise<boolean>}
     */
    validateSearchEngineInput(fieldName, fieldValue) {}
  }

  /**
   * @implements {settings.SearchEnginesBrowserProxy}
   */
  class SearchEnginesBrowserProxyImpl {
    /** @override */
    setDefaultSearchEngine(modelIndex) {
      chrome.send('setDefaultSearchEngine', [modelIndex]);
    }

    /** @override */
    removeSearchEngine(modelIndex) {
      chrome.send('removeSearchEngine', [modelIndex]);
    }

    /** @override */
    searchEngineEditStarted(modelIndex) {
      chrome.send('searchEngineEditStarted', [modelIndex]);
    }

    /** @override */
    searchEngineEditCancelled() {
      chrome.send('searchEngineEditCancelled');
    }

    /** @override */
    searchEngineEditCompleted(searchEngine, keyword, queryUrl) {
      chrome.send('searchEngineEditCompleted', [
        searchEngine,
        keyword,
        queryUrl,
      ]);
    }

    /** @override */
    getSearchEnginesList() {
      return cr.sendWithPromise('getSearchEnginesList');
    }

    /** @override */
    validateSearchEngineInput(fieldName, fieldValue) {
      return cr.sendWithPromise(
          'validateSearchEngineInput', fieldName, fieldValue);
    }
  }

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(SearchEnginesBrowserProxyImpl);

  return {
    SearchEnginesBrowserProxy: SearchEnginesBrowserProxy,
    SearchEnginesBrowserProxyImpl: SearchEnginesBrowserProxyImpl,
  };
});
