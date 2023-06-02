// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar.js';
import '../demo.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_toolbar_demo.html.js';

class CrToolbarDemoElement extends PolymerElement {
  static get is() {
    return 'cr-toolbar-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      alwaysShowLogo_: Boolean,
      clearLabel_: String,
      log_: Array,
      menuLabel_: String,
      narrow_: Boolean,
      narrowThreshold_: Number,
      pageName_: String,
      searchPrompt_: String,
      showMenu_: Boolean,
      showSearch_: Boolean,
      showSlottedContent_: Boolean,
    };
  }

  private alwaysShowLogo_: boolean = true;
  private clearLabel_: string = 'Clear search';
  private log_: string[] = [];
  private menuLabel_: string = 'Menu';
  private narrow_: boolean;
  private narrowThreshold_: number = 1000;
  private pageName_: string = 'Demo';
  private searchPrompt_: string = 'Search through some content';
  private showMenu_: boolean = true;
  private showSearch_: boolean = true;
  private showSlottedContent_: boolean = false;

  private onMenuClick_() {
    this.push('log_', 'Menu tapped.');
  }

  private onSearchChanged_(e: CustomEvent<string>) {
    if (e.detail) {
      this.push('log_', `Search term changed: ${e.detail}`);
    } else {
      this.push('log_', 'Search cleared.');
    }
  }
}

export const tagName = CrToolbarDemoElement.is;

customElements.define(CrToolbarDemoElement.is, CrToolbarDemoElement);
