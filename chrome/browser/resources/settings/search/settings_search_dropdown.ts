// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-search-dropdown' shows the results of a Settings search below the
 * toolbar search field (behind the enableSettingsSearch2026 flag).
 */

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SearchResult} from '../settings_search.mojom-webui.js';

import {getCss} from './settings_search_dropdown.css.js';
import {getHtml} from './settings_search_dropdown.html.js';
import {SettingsSearchManagerImpl} from './settings_search_manager.js';

export interface SettingsSearchDropdownElement {
  $: {
    dropdown: HTMLElement,
  };
}

export class SettingsSearchDropdownElement extends CrLitElement {
  static get is() {
    return 'settings-search-dropdown';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      query: {type: String},
      results_: {type: Array},
    };
  }

  accessor query: string = '';
  protected accessor results_: SearchResult[] = [];

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    // Skip the initial update from `undefined`, so that no search runs while
    // the element is being initialized.
    if (changedProperties.has('query') &&
        changedProperties.get('query') !== undefined) {
      this.onQueryChanged_();
    }
  }

  /** Requests results for `query` and shows them. */
  private async onQueryChanged_(): Promise<void> {
    const request =
        await SettingsSearchManagerImpl.getInstance().search(this.query);
    if (request.canceled) {
      // A newer query replaced this request, and `results_` is updated when
      // that query's results arrive. Keep showing the current results until
      // then.
      return;
    }
    this.results_ = request.results;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-dropdown': SettingsSearchDropdownElement;
  }
}

customElements.define(
    SettingsSearchDropdownElement.is, SettingsSearchDropdownElement);
