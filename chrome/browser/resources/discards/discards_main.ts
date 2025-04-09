// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import './database_tab.js';
import './discards_tab.js';
import './graph_tab.js';

import {CrRouter} from 'chrome://resources/js/cr_router.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './discards_main.css.js';
import {getHtml} from './discards_main.html.js';

export class DiscardsMainElement extends CrLitElement {
  static get is() {
    return 'discards-main';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selected: {type: Number},
      tabs: {type: Array},
    };
  }

  protected accessor selected: number = 0;
  protected accessor tabs: string[] = ['Discards', 'Database', 'Graph'];

  override firstUpdated() {
    const router = CrRouter.getInstance();
    this.pathChanged_(router.getPath());
    router.addEventListener(
        'cr-router-path-changed',
        (e: Event) => this.pathChanged_((e as CustomEvent<string>).detail));
  }

  /** Updates the location hash on selection change. */
  protected onSelectedChanged_(e: CustomEvent<{value: number}>) {
    const newValue = e.detail.value;
    if (newValue === this.selected) {
      return;
    }

    this.selected = newValue;

    // The first tab is special-cased to the empty path.
    const defaultTab = this.tabs[0].toLowerCase();
    const tabName = this.tabs[newValue].toLowerCase();
    CrRouter.getInstance().setPath(
        '/' + (tabName === defaultTab ? '' : tabName));
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
