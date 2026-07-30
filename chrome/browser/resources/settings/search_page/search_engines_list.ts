// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engines-list' is a component for showing a
 * list of search engines.
 */
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import './search_engine_entry.js';

import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SearchEngine} from './search_engines_browser_proxy.js';
import {getCss} from './search_engines_list.css.js';
import {getHtml} from './search_engines_list.html.js';

export type SearchEnginesListElement = SettingsSearchEnginesListElement;

export class SettingsSearchEnginesListElement extends CrLitElement {
  static get is() {
    return 'settings-search-engines-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      engines: {type: Array},
      showShortcut: {type: Boolean},
      showQueryUrl: {type: Boolean},
      collapseList: {type: Boolean},
      visibleEnginesSize: {type: Number},
      visibleEngines_: {type: Array},
      collapsedEngines_: {type: Array},
      expandListText: {type: String},
      fixedHeight: {type: Boolean, reflect: true},
      enginesListExpanded_: {type: Boolean},
    };
  }

  accessor engines: SearchEngine[] = [];
  accessor showShortcut: boolean = false;
  accessor showQueryUrl: boolean = false;
  accessor collapseList: boolean = false;
  accessor visibleEnginesSize: number = 5;
  accessor expandListText: string = '';
  accessor fixedHeight: boolean = false;
  protected accessor enginesListExpanded_: boolean = false;
  protected accessor visibleEngines_: SearchEngine[] = [];
  protected accessor collapsedEngines_: SearchEngine[] = [];

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('engines') ||
        changedProperties.has('visibleEnginesSize')) {
      this.visibleEngines_ = this.engines.slice(0, this.visibleEnginesSize);
      this.collapsedEngines_ = this.engines.slice(this.visibleEnginesSize);
    }
  }

  protected onEnginesListExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.enginesListExpanded_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-engines-list': SettingsSearchEnginesListElement;
  }
}

customElements.define(
    SettingsSearchEnginesListElement.is, SettingsSearchEnginesListElement);
