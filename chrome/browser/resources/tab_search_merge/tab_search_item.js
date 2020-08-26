// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class TabSearchItem extends PolymerElement {
  static get is() {
    return 'tab-search-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!tabSearch.mojom.Tab} */
      data: Object,
    };
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemCancel_(e) {
    this.dispatchEvent(new CustomEvent('close'));
    e.stopPropagation();
  }

  /**
   * @param {string} url
   * @return {string}
   * @private
   */
  urlHostname_(url) {
    return new URL(url).hostname;
  }
}

customElements.define(TabSearchItem.is, TabSearchItem);
