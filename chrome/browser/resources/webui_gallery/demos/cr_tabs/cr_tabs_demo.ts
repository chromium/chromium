// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_page_selector/cr_page_selector.js';
import '//resources/cr_elements/cr_tabs/cr_tabs.js';

import type {CrTabsElement} from '//resources/cr_elements/cr_tabs/cr_tabs.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_tabs_demo.css.js';
import {getHtml} from './cr_tabs_demo.html.js';

export interface CrTabsDemoElement {
  $: {
    tabs: CrTabsElement,
  };
}

export class CrTabsDemoElement extends CrLitElement {
  static get is() {
    return 'cr-tabs-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedTabIndex_: {type: Number},
      tabNames_: {type: Array},
    };
  }

  protected accessor selectedTabIndex_: number = 0;
  protected accessor tabNames_: string[] = ['Tab 1', 'Tab 2', 'Tab 3'];

  protected onAddClick_() {
    this.tabNames_.push('Added');
    this.tabNames_ = this.tabNames_.slice();
  }

  protected onAddAt1Click_() {
    this.tabNames_.splice(1, 0, 'Added at 1');
    this.tabNames_ = this.tabNames_.slice();
  }

  protected onSelectAt1Click_() {
    this.selectedTabIndex_ = 1;
  }

  protected onSelectedTabIndexChanged_(e: CustomEvent<{value: number}>) {
    this.selectedTabIndex_ = e.detail.value;
  }
}

export const tagName = CrTabsDemoElement.is;

customElements.define(CrTabsDemoElement.is, CrTabsDemoElement);
