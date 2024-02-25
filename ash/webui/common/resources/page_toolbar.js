// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icons.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './page_toolbar.html.js';

/** @polymer */
export class PageToolbarElement extends PolymerElement {
  static get is() {
    return 'page-toolbar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      title: {
        type: String,
        value: '',
      },

      isNarrow: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      hasSearch: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  onMenuTap_() {
    this.dispatchEvent(
        new CustomEvent('menu-tap', {bubbles: true, composed: true}));
  }

  shouldHideTitle_() {
    // Hide the title when a search bar is present and the side nav is hidden.
    return this.isNarrow && this.hasSearch;
  }
}

customElements.define(PageToolbarElement.is, PageToolbarElement);
