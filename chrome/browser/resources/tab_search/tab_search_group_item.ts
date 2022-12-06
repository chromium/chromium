// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/mwb_shared_icons.html.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.css.js';

import {MouseHoverableMixin} from 'chrome://resources/cr_elements/mouse_hoverable_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ariaLabel, TabGroupData} from './tab_data.js';
import {colorName} from './tab_group_color_helper.js';
import {getTemplate} from './tab_search_group_item.html.js';
import {highlightText} from './tab_search_utils.js';

const TabSearchGroupItemBase = MouseHoverableMixin(PolymerElement);

export interface TabSearchGroupItem {
  $: {
    primaryText: HTMLElement,
  };
}

export class TabSearchGroupItem extends TabSearchGroupItemBase {
  static get is() {
    return 'tab-search-group-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      id: String,
      index: Number,

      data: {
        type: Object,
        observer: 'dataChanged_',
      },
    };
  }

  index: string;
  data: TabGroupData;

  private ariaLabelForText_(tabGroupData: TabGroupData): string {
    return ariaLabel(tabGroupData);
  }

  private dataChanged_(data: TabGroupData) {
    highlightText(
        this.$.primaryText, data.tabGroup!.title,
        data.highlightRanges['tabGroup.title']);

    this.style.setProperty(
        '--group-dot-color',
        `var(--tab-group-color-${colorName(data.tabGroup!.color)})`);
  }

  private tabCountText_(tabCount: number): string {
    return loadTimeData.getStringF(
        tabCount === 1 ? 'oneTab' : 'tabCount', tabCount);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-search-group-item': TabSearchGroupItem;
  }
}

customElements.define(TabSearchGroupItem.is, TabSearchGroupItem);
