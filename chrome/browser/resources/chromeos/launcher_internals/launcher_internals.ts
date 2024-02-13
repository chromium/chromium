// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './results_table.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {getTemplate} from './launcher_internals.html.js';
import {PageCallbackRouter, Result} from './launcher_internals.mojom-webui.js';
import {LauncherResultsTableElement} from './results_table.js';

interface LauncherInternalsElement {
  $: {
    'searchResults': LauncherResultsTableElement,
    'recentFiles': LauncherResultsTableElement,
    'recentApps': LauncherResultsTableElement,
  };
}

class LauncherInternalsElement extends PolymerElement {
  static get is() {
    return 'launcher-internals';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {query: String, keywords: [String]};
  }

  private query: string;
  private keywords: string[];
  private listenerIds: number[];
  private router: PageCallbackRouter;

  constructor() {
    super();
    this.query = '';
    this.keywords = [];
    this.listenerIds = [];
    this.router = BrowserProxy.getInstance().callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.listenerIds.push(
        this.router.updateResults.addListener(this.updateResults.bind(this)));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds.forEach(id => this.router.removeListener(id));
  }

  private updateResults(query: string, keywords: string[], results: Result[]) {
    // Split the results array into its three display surfaces.
    const recentFiles: Result[] = [];
    const recentApps: Result[] = [];
    const searchResults: Result[] = [];

    results.forEach(result => {
      switch (result.displayType) {
        case 'Continue':
          recentFiles.push(result);
          break;
        case 'RecentApps':
          recentApps.push(result);
          break;
        default:
          searchResults.push(result);
          break;
      }
    });

    if (recentFiles.length > 0) {
      this.$.recentFiles.clearResults();
      this.$.recentFiles.addResults(recentFiles);
    }

    if (recentApps.length > 0) {
      this.$.recentApps.clearResults();
      this.$.recentApps.addResults(recentApps);
    }

    if (searchResults.length > 0) {
      if (this.query !== query) {
        // Only reset search results if the query changes.
        this.$.searchResults.clearResults();
        this.query = query;
      }

      if (this.keywords !== keywords) {
        this.keywords = keywords;
      }

      this.$.searchResults.addResults(searchResults);
    }
  }
}

customElements.define(LauncherInternalsElement.is, LauncherInternalsElement);
