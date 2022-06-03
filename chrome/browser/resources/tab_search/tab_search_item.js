// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/mwb_shared_icons.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './strings.m.js';

import {MouseHoverableMixin, MouseHoverableMixinInterface} from 'chrome://resources/cr_elements/mouse_hoverable_mixin.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {get as deepGet, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ariaLabel, TabData, TabItemType} from './tab_data.js';
import {colorName} from './tab_group_color_helper.js';
import {Tab, TabGroup} from './tab_search.mojom-webui.js';
import {highlightText} from './tab_search_utils.js';

/**
 * @constructor
 * @extends PolymerElement
 * @implements {MouseHoverableMixinInterface}
 * @appliesMixin MouseHoverableMixin
 */
const TabSearchItemBase = MouseHoverableMixin(PolymerElement);

/** @polymer */
export class TabSearchItem extends TabSearchItemBase {
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
    return type === TabItemType.OPEN_TAB;
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
        `url("${tab.faviconUrl.url}")` :
        getFaviconForPageURL(
            tab.isDefaultFavicon ? 'chrome://newtab' : tab.url.url, false);
  }

  /**
   * Determines the display attribute value for the group SVG element.
   * @param {!TabData} tabData
   * @return {string}
   * @private
   */
  groupSvgDisplay_(tabData) {
    return tabData.tabGroup ? 'block' : 'none';
  }

  /**
   * @param {!TabData} tabData
   * @returns {boolean}
   * @private
   */
  hasTabGroupWithTitle_(tabData) {
    return !!(tabData.tabGroup && tabData.tabGroup.title);
  }

  /**
   * @param {!TabData} data
   * @private
   */
  dataChanged_(data) {
    [['tab.title', this.$.primaryText], ['hostname', this.$.secondaryText],
     ['tabGroup.title', this.$.groupTitle]]
        .forEach(([path, element]) => {
          if (element) {
            const highlightRanges =
                data.highlightRanges ? data.highlightRanges[path] : undefined;
            highlightText(
                /** @type {!HTMLElement} */ (element), deepGet(data, path),
                highlightRanges);
          }
        });

    // Show chrome:// if it's a chrome internal url
    let secondaryLabel = data.hostname;
    const protocol = new URL(data.tab.url.url).protocol;
    if (protocol === 'chrome:') {
      /** @type {!HTMLElement} */ (this.$.secondaryText)
          .prepend(document.createTextNode('chrome://'));
      secondaryLabel = `chrome://${secondaryLabel}`;
    }

    if (data.tabGroup) {
      this.style.setProperty(
          '--group-dot-color',
          `var(--tab-group-color-${colorName(data.tabGroup.color)})`);
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
