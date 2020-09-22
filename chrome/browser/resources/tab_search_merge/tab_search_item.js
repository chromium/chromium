// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {getFaviconForPageURL} from 'chrome://resources/js/icon.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import './strings.js';

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
  onItemClose_(e) {
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

  /**
   * @param isDefaultFavicon {boolean}
   * @param url {string}
   * @return {string}
   * @private
   */
  faviconUrl_(isDefaultFavicon, url) {
    return getFaviconForPageURL(isDefaultFavicon ? undefined : url, false);
  }

  ariaLabel_(title) {
    return `${loadTimeData.getString('closeTab')} ${title}`;
  }
}

customElements.define(TabSearchItem.is, TabSearchItem);
