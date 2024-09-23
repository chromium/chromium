// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MouseHoverableMixinLit} from 'chrome://resources/cr_elements/mouse_hoverable_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {TabGroupData} from './tab_data.js';
import {colorName} from './tab_group_color_helper.js';
import {Color} from './tab_group_types.mojom-webui.js';
import {getCss} from './tab_search_group_item.css.js';
import {getHtml} from './tab_search_group_item.html.js';
import {highlightText} from './tab_search_utils.js';

const TabSearchGroupItemBase = MouseHoverableMixinLit(CrLitElement);

export interface TabSearchGroupItemElement {
  $: {
    primaryText: HTMLElement,
  };
}

export class TabSearchGroupItemElement extends TabSearchGroupItemBase {
  static get is() {
    return 'tab-search-group-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      data: {type: Object},
    };
  }

  data: TabGroupData = new TabGroupData({
    sessionId: -1,
    id: {high: 0n, low: 0n},
    color: Color.kGrey,
    title: '',
    tabCount: 1,
    lastActiveTime: {internalValue: 0n},
    lastActiveElapsedText: '',
  });

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('data')) {
      this.style.setProperty(
          '--group-dot-color',
          `var(--tab-group-color-${colorName(this.data.tabGroup.color)})`);
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('data')) {
      highlightText(
          this.$.primaryText, this.data.tabGroup.title,
          this.data.highlightRanges['tabGroup.title']);
    }
  }

  protected tabCountText_(): string {
    const tabCount = this.data.tabGroup.tabCount;
    return loadTimeData.getStringF(
        tabCount === 1 ? 'oneTab' : 'tabCount', tabCount);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-search-group-item': TabSearchGroupItemElement;
  }
}

customElements.define(TabSearchGroupItemElement.is, TabSearchGroupItemElement);
