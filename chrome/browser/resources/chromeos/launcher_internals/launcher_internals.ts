// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './results_table.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {PageCallbackRouter, Result} from './launcher_internals.mojom-webui.js';
import {LauncherResultsTableElement} from './results_table.js';

interface LauncherInternalsElement {
  $: {
    'zeroStateResults': LauncherResultsTableElement,
    'searchResults': LauncherResultsTableElement,
  };
}

class LauncherInternalsElement extends PolymerElement {
  static get is() {
    return 'launcher-internals';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      query: String,
    };
  }

  private query: string;
  private listenerIds: Array<number>;
  private router: PageCallbackRouter;

  constructor() {
    super();
    this.query = '';
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

  private updateResults(query: string, results: Array<Result>) {
    if (query === '') {
      this.$.zeroStateResults.clearResults();
      this.$.zeroStateResults.addResults(results);
      return;
    }

    if (this.query != query) {
      // Reset the results table whenever the query changes.
      this.$.searchResults.clearResults();
      this.query = query;
    }
    this.$.searchResults.addResults(results);
  }
}

customElements.define(LauncherInternalsElement.is, LauncherInternalsElement);
