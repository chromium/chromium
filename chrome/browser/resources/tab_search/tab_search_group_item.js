// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/mwb_shared_icons.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';

import {MouseHoverableMixin, MouseHoverableMixinInterface} from 'chrome://resources/cr_elements/mouse_hoverable_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ariaLabel, TabGroupData} from './tab_data.js';
import {colorName} from './tab_group_color_helper.js';
import {highlightText} from './tab_search_utils.js';

/**
 * @constructor
 * @extends PolymerElement
 * @implements {MouseHoverableMixinInterface}
 * @appliesMixin MouseHoverableMixin
 */
const TabSearchGroupItemBase = MouseHoverableMixin(PolymerElement);

/** @polymer */
export class TabSearchGroupItem extends TabSearchGroupItemBase {
  static get is() {
    return 'tab-search-group-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {number} */
      id: Number,

      /** @type {number} */
      index: Number,

      /** @type {!TabGroupData} */
      data: {
        type: Object,
        observer: 'dataChanged_',
      },
    };
  }

  /**
   * @param {!TabGroupData} tabGroupData
   * @return {string}
   * @private
   */
  ariaLabelForText_(tabGroupData) {
    return ariaLabel(tabGroupData);
  }

  /**
   * @param {!TabGroupData} data
   * @private
   */
  dataChanged_(data) {
    highlightText(
        /** @type {!HTMLElement} */ (this.$.primaryText), data.tabGroup.title,
        data.highlightRanges['tabGroup.title']);

    this.style.setProperty(
        '--group-dot-color',
        `var(--tab-group-color-${colorName(data.tabGroup.color)})`);
  }

  /**
   * @return {string}
   * @private
   */
  tabCountText_(tabCount) {
    return loadTimeData.getStringF(
        tabCount == 1 ? 'oneTab' : 'tabCount', tabCount);
  }
}

customElements.define(TabSearchGroupItem.is, TabSearchGroupItem);
