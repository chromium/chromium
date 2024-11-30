// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_spinner_style.css.js';
import '/strings.m.js';
import './searchbox_shared_style.css.js';

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './searchbox_ghost_loader.html.js';
import {BrowserProxyImpl} from './searchbox_ghost_loader_browser_proxy.js';
import type {BrowserProxy} from './searchbox_ghost_loader_browser_proxy.js';

// Displays a loading preview while waiting on autocomplete to return matches.
export class SearchboxGhostLoaderElement extends PolymerElement {
  static get is() {
    return 'cr-searchbox-ghost-loader';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showErrorState: {
        type: Boolean,
        reflectToAttribute: true,
        notify: true,
      },
      showContextualSearchboxLoadingState: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('showContextualSearchboxLoadingState'),
        reflectToAttribute: true,
      },
    };
  }

  // Whether the autocomplete stop timer has triggered. If it has, we should
  // hide the ghost loader. We also show the error text in this case.
  private showErrorState: boolean;
  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();
  private listenerIds: number[];

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.browserProxy.callbackRouter;
    this.listenerIds = [
      callbackRouter.showErrorState.addListener(() => {
        this.showErrorState = true;
      }),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds.forEach(
        id => assert(this.browserProxy.callbackRouter.removeListener(id)));
    this.listenerIds = [];
  }

  showErrorStateForTesting() {
    this.showErrorState = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox-ghost-loader': SearchboxGhostLoaderElement;
  }
}

customElements.define(
    SearchboxGhostLoaderElement.is, SearchboxGhostLoaderElement);
