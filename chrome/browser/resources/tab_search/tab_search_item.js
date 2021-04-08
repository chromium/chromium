// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/mwb_shared_icons.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './strings.m.js';

import {getFaviconForPageURL} from 'chrome://resources/js/icon.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {highlight} from 'chrome://resources/js/search_highlight_utils.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ariaLabel, TabData, TabItemType} from './tab_data.js';
import {Tab} from './tab_search.mojom-webui.js';

export class TabSearchItem extends PolymerElement {
  static get is() {
    return 'tab-search-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!TabData} */
      data: {
        type: Object,
        observer: 'dataChanged_',
      },

      /** @private {boolean} */
      buttonRipples_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('useRipples'),
      },

      /** @type {number} */
      index: Number,
    };
  }

  /**
   * @param {!TabItemType} type
   * @return {boolean} Whether a close action can be performed on the item.
   */
  isCloseable_(type) {
    return type === TabItemType.OPEN;
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
   * @param {!Tab} tab
   * @return {string}
   * @private
   */
  faviconUrl_(tab) {
    return tab.faviconUrl ?
        `url("${tab.faviconUrl}")` :
        getFaviconForPageURL(
            tab.isDefaultFavicon ? 'chrome://newtab' : tab.url, false);
  }

  /**
   * @private
   */
  dataChanged_() {
    this.highlightText_(
        /** @type {!HTMLElement} */ (this.$.primaryText), this.data.tab.title,
        this.data.titleHighlightRanges);
    this.highlightText_(
        /** @type {!HTMLElement} */ (this.$.secondaryText), this.data.hostname,
        this.data.hostnameHighlightRanges);

    // Show chrome:// if it's a chrome internal url
    let secondaryLabel = this.data.hostname;
    let protocol = '';
    try {
      protocol = new URL(this.data.tab.url).protocol;
    } catch (e) {
      // TODO(crbug.com/1186409): Remove this after we root cause the issue
      console.error(
          `Error parsing URL on Tab Search: url=${this.data.tab.url}`);
    }
    if (protocol === 'chrome:') {
      /** @type {!HTMLElement} */ (this.$.secondaryText)
          .prepend(document.createTextNode('chrome://'));
      secondaryLabel = `chrome://${secondaryLabel}`;
    }
  }

  /**
   *
   * @param {!HTMLElement} container
   * @param {string} text
   * @param {!Array<!{start:number, length:number}>|undefined} ranges
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

  /**
   * @param {!TabData} tabData
   * @return {string}
   * @private
   */
  ariaLabelForText_(tabData) {
    return ariaLabel(tabData);
  }

  /**
   * @param {string} title
   * @return {string}
   * @private
   */
  ariaLabelForButton_(title) {
    return `${loadTimeData.getString('closeTab')} ${title}`;
  }
}

customElements.define(TabSearchItem.is, TabSearchItem);
