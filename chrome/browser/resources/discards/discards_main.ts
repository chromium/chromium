// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './database_tab.js';
import './discards_tab.js';
import './graph_tab_template.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './discards_main.html.js';

class DiscardsMainElement extends PolymerElement {
  static get is() {
    return 'discards-main';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selected: {
        type: Number,
        value: 0,
        observer: 'selectedChanged_',
      },

      path: {
        type: String,
        value: '',
        observer: 'pathChanged_',
      },

      tabs: {
        type: Array,
        value: () => ['Discards', 'Database', 'Graph'],
      },
    };
  }

  selected: number;
  path: string;
  tabs: string[];

  /** Updates the location hash on selection change. */
  private selectedChanged_(newValue: number, oldValue?: number) {
    if (oldValue !== undefined) {
      // The first tab is special-cased to the empty path.
      const defaultTab = this.tabs[0].toLowerCase();
      const tabName = this.tabs[newValue].toLowerCase();
      this.path = '/' + (tabName === defaultTab ? '' : tabName);
    }
  }

  /**
   * Returns the index of the currently selected tab corresponding to the
   * path or zero if no match.
   */
  private selectedFromPath_(path: string): number {
    const index = this.tabs.findIndex(tab => path === tab.toLowerCase());
    return Math.max(index, 0);
  }

  /** Updates the selection property on path change. */
  private pathChanged_(newValue: string, _oldValue?: string) {
    this.selected = this.selectedFromPath_(newValue.substr(1));
  }
}

customElements.define(DiscardsMainElement.is, DiscardsMainElement);
