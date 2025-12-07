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
  iconPath: string;
  id: number;
  isManaged: boolean;
  isOmniboxExtension: boolean;
  isPrepopulated: boolean;
  isStarterPack: boolean;
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

/**
 * The location from which the search engine choice was made.
 *
 * Must be kept in sync with the ChoiceMadeLocation enum in
 * //components/search_engines/choice_made_location.h
 */
export enum ChoiceMadeLocation {
  // `chrome://settings/search`
  SEARCH_SETTINGS = 0,
  // `chrome://settings/searchEngines`
  SEARCH_ENGINE_SETTINGS = 1,
  // The search engine choice dialog for existing users or the profile picker
  // for new users. This value should not be used in settings.
  CHOICE_SCREEN = 2,
  // Some other source, not matching some requirements that the full search
  // engine choice surfaces are compatible with. Might be used for example when
  // automatically changing default search engine via an extension, or some
  // enterprise policy.
  OTHER = 3,
}

export interface SearchEnginesBrowserProxy {
  setDefaultSearchEngine(
      modelIndex: number, choiceMadeLocation: ChoiceMadeLocation,
      saveGuestChoice: boolean|null): void;

  setIsActiveSearchEngine(modelIndex: number, isActive: boolean): void;

  removeSearchEngine(modelIndex: number): void;

  searchEngineEditStarted(modelIndex: number): void;

  searchEngineEditCancelled(): void;

  searchEngineEditCompleted(
      searchEngine: string, keyword: string, queryUrl: string): void;

  getSearchEnginesList(): Promise<SearchEnginesInfo>;

  getSaveGuestChoice(): Promise<boolean|null>;

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
  setDefaultSearchEngine(
      modelIndex: number, choiceMadeLocation: ChoiceMadeLocation,
      saveGuestChoice?: boolean|null) {
    chrome.send(
        'setDefaultSearchEngine',
        [modelIndex, choiceMadeLocation, saveGuestChoice]);
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

  getSaveGuestChoice() {
    return sendWithPromise('getSaveGuestChoice');
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
