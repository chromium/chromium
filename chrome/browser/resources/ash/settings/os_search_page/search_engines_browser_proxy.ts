// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Manage search engines" section
 * to interact with the browser.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * @see chrome/browser/ui/webui/settings/search_engines_handler.cc
 */
export interface SearchEngine {
  canBeDefault: boolean;
  canBeEdited: boolean;
  canBeRemoved: boolean;
  default: boolean;
  displayName: string;
  extension?: {
    id: string,
    name: string,
    canBeDisabled: boolean,
    icon: string,
  };
  iconURL?: string;
  id: number;
  isOmniboxExtension: boolean;
  keyword: string;
  name: string;
  url: string;
  urlLocked: boolean;
}

export interface SearchEnginesInfo {
  defaults: SearchEngine[];
  actives: SearchEngine[];
  others: SearchEngine[];
  extensions: SearchEngine[];
}

export interface CategorizedTemplateUrls {
  activeSiteShortcuts: SearchEngine[];
  inactiveSiteShortcuts: SearchEngine[];
  activeFeatureShortcuts: SearchEngine[];
  inactiveFeatureShortcuts: SearchEngine[];
  [key: string]: SearchEngine[];
}

export interface SearchEnginesBrowserProxy {
  getCategorizedTemplateUrls(): Promise<CategorizedTemplateUrls>;
  getSearchEnginesList(): Promise<SearchEnginesInfo>;
  openBrowserSearchSettings(): void;
}

let instance: SearchEnginesBrowserProxy|null = null;

export class SearchEnginesBrowserProxyImpl implements
    SearchEnginesBrowserProxy {
  static getInstance(): SearchEnginesBrowserProxy {
    return instance || (instance = new SearchEnginesBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: SearchEnginesBrowserProxy): void {
    instance = obj;
  }

  getCategorizedTemplateUrls(): Promise<CategorizedTemplateUrls> {
    return sendWithPromise('getCategorizedTemplateUrls');
  }

  getSearchEnginesList(): Promise<SearchEnginesInfo> {
    return sendWithPromise('getSearchEnginesList');
  }

  openBrowserSearchSettings(): void {
    chrome.send('openBrowserSearchSettings');
  }
}
