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
import {highlight} from 'chrome://resources/js/search_highlight_utils.m.js';
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
      data: {
        type: Object,
        observer: 'dataChanged_',
      },
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
   * @param {boolean} isDefaultFavicon
   * @param {string} url
   * @return {string}
   * @private
   */
  faviconUrl_(isDefaultFavicon, url) {
    return getFaviconForPageURL(
        isDefaultFavicon ? 'chrome://newtab' : url, false);
  }

  /**
   * @suppress {checkTypes}
   * Suppress checking types because hostname, titleHighlightRanges and
   * hostnameHighlightRanges are added on the flight and not part of the
   * original mojom.tabSearch.Tab definition
   * @private
   */
  dataChanged_() {
    this.highlightText_(
        /** @type {!HTMLElement} */ (this.$.primaryText), this.data.title,
        this.data.titleHighlightRanges);
    this.highlightText_(
        /** @type {!HTMLElement} */ (this.$.secondaryText), this.data.hostname,
        this.data.hostnameHighlightRanges);

    // Show chrome:// if it's a chrome internal url
    if (new URL(this.data.url).protocol === 'chrome:') {
      /** @type {!HTMLElement} */ (this.$.secondaryText)
          .prepend(document.createTextNode('chrome://'));
    }
  }

  /**
   *
   * @param {!HTMLElement} container
   * @param {string} text
   * @param {?Array<!{start:number, length:number}>} ranges
   */
  highlightText_(container, text, ranges) {
    container.textContent = '';
    const node = document.createTextNode(text);
    container.appendChild(node);
    if (ranges) {
      const result = highlight(node, ranges, true);
      // Delete default highlight style.
      result.querySelectorAll('.search-highlight-hit').forEach(e => {
        e.style = '';
      });
    }
  }

  ariaLabel_(title) {
    return `${loadTimeData.getString('closeTab')} ${title}`;
  }
}

customElements.define(TabSearchItem.is, TabSearchItem);
