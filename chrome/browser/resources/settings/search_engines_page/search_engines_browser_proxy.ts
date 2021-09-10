// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * @fileoverview A helper object used from the "Manage search engines" section
 * to interact with the browser.
 */

/**
 * @see chrome/browser/ui/webui/settings/search_engines_handler.cc
 */
export type SearchEngine = {
  canBeDefault: boolean,
  canBeEdited: boolean,
  canBeRemoved: boolean,
  canBeActivated: boolean,
  canBeDeactivated: boolean,
  default: boolean,
  displayName: string,
  extension?: {id: string, name: string, canBeDisabled: boolean, icon: string},
  iconURL?: string,
  id: number,
  isOmniboxExtension: boolean,
  keyword: string,
  modelIndex: number,
  name: string,
  url: string,
  urlLocked: boolean,
};

export type SearchEnginesInfo = {
  defaults: Array<SearchEngine>,
  actives: Array<SearchEngine>,
  others: Array<SearchEngine>,
  extensions: Array<SearchEngine>,
  [key: string]: Array<SearchEngine>,
};

export interface SearchEnginesBrowserProxy {
  setDefaultSearchEngine(modelIndex: number): void;

  setIsActiveSearchEngine(modelIndex: number, isActive: boolean): void;

  removeSearchEngine(modelIndex: number): void;

  searchEngineEditStarted(modelIndex: number): void;

  searchEngineEditCancelled(): void;

  searchEngineEditCompleted(
      searchEngine: string, keyword: string, queryUrl: string): void;

  getSearchEnginesList(): Promise<SearchEnginesInfo>;

  validateSearchEngineInput(fieldName: string, fieldValue: string):
      Promise<boolean>
}

export class SearchEnginesBrowserProxyImpl implements
    SearchEnginesBrowserProxy {
  setDefaultSearchEngine(modelIndex: number) {
    chrome.send('setDefaultSearchEngine', [modelIndex]);
  }

  setIsActiveSearchEngine(modelIndex: number, isActive: boolean) {
    chrome.send('setIsActiveSearchEngine', [modelIndex, isActive]);
  }

  removeSearchEngine(modelIndex: number) {
    chrome.send('removeSearchEngine', [modelIndex]);
  }

  searchEngineEditStarted(modelIndex: number) {
    chrome.send('searchEngineEditStarted', [modelIndex]);
  }

  searchEngineEditCancelled() {
    chrome.send('searchEngineEditCancelled');
  }

  searchEngineEditCompleted(
      searchEngine: string, keyword: string, queryUrl: string) {
    chrome.send('searchEngineEditCompleted', [
      searchEngine,
      keyword,
      queryUrl,
    ]);
  }

  getSearchEnginesList() {
    return sendWithPromise('getSearchEnginesList');
  }

  validateSearchEngineInput(fieldName: string, fieldValue: string) {
    return sendWithPromise('validateSearchEngineInput', fieldName, fieldValue);
  }

  static getInstance(): SearchEnginesBrowserProxy {
    return instance || (instance = new SearchEnginesBrowserProxyImpl());
  }

  static setInstance(obj: SearchEnginesBrowserProxy) {
    instance = obj;
  }
}

let instance: SearchEnginesBrowserProxy|null = null;
