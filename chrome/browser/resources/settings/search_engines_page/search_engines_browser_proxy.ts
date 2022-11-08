// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

/**
 * @fileoverview A helper object used from the "Manage search engines" section
 * to interact with the browser.
 */

/**
 * @see chrome/browser/ui/webui/settings/search_engines_handler.cc
 */
export interface SearchEngine {
  canBeDefault: boolean;
  canBeEdited: boolean;
  canBeRemoved: boolean;
  canBeActivated: boolean;
  canBeDeactivated: boolean;
  default: boolean;
  displayName: string;
  extension?: {id: string, name: string, canBeDisabled: boolean, icon: string};
  iconURL?: string;
  id: number;
  isOmniboxExtension: boolean;
  keyword: string;
  modelIndex: number;
  name: string;
  shouldConfirmDeletion: boolean;
  url: string;
  urlLocked: boolean;
}

export interface SearchEnginesInfo {
  defaults: SearchEngine[];
  actives: SearchEngine[];
  others: SearchEngine[];
  extensions: SearchEngine[];
  [key: string]: SearchEngine[];
}

/**
 * Contains all recorded interactions on the search engines settings page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SettingsSearchEnginesInteractions enum in
 * histograms/enums.xml
 */
export enum SearchEnginesInteractions {
  ACTIVATE = 0,
  DEACTIVATE = 1,
  KEYBOARD_SHORTCUT_TAB = 2,
  KEYBOARD_SHORTCUT_SPACE_OR_TAB = 3,

  // Leave this at the end.
  COUNT = 4,
}

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
      Promise<boolean>;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.SearchEngines.Interactions histogram
   */
  recordSearchEnginesPageHistogram(interaction: SearchEnginesInteractions):
      void;
}

export class SearchEnginesBrowserProxyImpl implements
    SearchEnginesBrowserProxy {
  setDefaultSearchEngine(modelIndex: number) {
    chrome.send('setDefaultSearchEngine', [modelIndex]);
  }

  setIsActiveSearchEngine(modelIndex: number, isActive: boolean) {
    chrome.send('setIsActiveSearchEngine', [modelIndex, isActive]);
    this.recordSearchEnginesPageHistogram(
        isActive ? SearchEnginesInteractions.ACTIVATE :
                   SearchEnginesInteractions.DEACTIVATE);
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

  recordSearchEnginesPageHistogram(interaction: SearchEnginesInteractions) {
    chrome.metricsPrivate.recordEnumerationValue(
        'Settings.SearchEngines.Interactions', interaction,
        SearchEnginesInteractions.COUNT);
  }

  static getInstance(): SearchEnginesBrowserProxy {
    return instance || (instance = new SearchEnginesBrowserProxyImpl());
  }

  static setInstance(obj: SearchEnginesBrowserProxy) {
    instance = obj;
  }
}

let instance: SearchEnginesBrowserProxy|null = null;
