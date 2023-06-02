// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_tabs/cr_tabs.js';
import '//resources/polymer/v3_0/iron-pages/iron-pages.js';
import '../demo.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_tabs_demo.html.js';

class CrTabsDemoElement extends PolymerElement {
  static get is() {
    return 'cr-tabs-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedTabIndex_: Number,
      tabNames_: Array,
    };
  }

  private selectedTabIndex_ = 0;
  private tabNames_: string[] = ['Tab 1', 'Tab 2', 'Tab 3'];

  private onAddClick_() {
    this.push('tabNames_', 'Added');
  }

  private onAddAt1Click_() {
    this.splice('tabNames_', 1, 0, 'Added at 1');
  }

  private onSelectAt1Click_() {
    this.selectedTabIndex_ = 1;
  }
}

export const tagName = CrTabsDemoElement.is;

customElements.define(CrTabsDemoElement.is, CrTabsDemoElement);
