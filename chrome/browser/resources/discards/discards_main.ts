// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import './database_tab.js';
import './discards_tab.js';
import './graph_tab.js';

import {CrRouter} from 'chrome://resources/js/cr_router.js';
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

      tabs: {
        type: Array,
        value: () => ['Discards', 'Database', 'Graph'],
      },
    };
  }

  selected: number;
  tabs: string[];

  override ready() {
    super.ready();
    const router = CrRouter.getInstance();
    this.pathChanged_(router.getPath());
    router.addEventListener(
        'cr-router-path-changed',
        (e: Event) => this.pathChanged_((e as CustomEvent<string>).detail));
  }

  /** Updates the location hash on selection change. */
  private selectedChanged_(newValue: number, oldValue?: number) {
    if (oldValue !== undefined) {
      // The first tab is special-cased to the empty path.
      const defaultTab = this.tabs[0].toLowerCase();
      const tabName = this.tabs[newValue].toLowerCase();
      CrRouter.getInstance().setPath(
          '/' + (tabName === defaultTab ? '' : tabName));
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
  private pathChanged_(newValue: string) {
    this.selected = this.selectedFromPath_(newValue.substr(1));
  }
}

customElements.define(DiscardsMainElement.is, DiscardsMainElement);
