// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getTabGroupColorVar} from './tab_group_color_helper.js';
import {getCss} from './tab_group_dot.css.js';
import {getHtml} from './tab_group_dot.html.js';
import {Color} from './tab_group_types.mojom-webui.js';

export enum TabGroupDotSize {
  SMALL = 'small',
  LARGE = 'large',
}

export class TabGroupDotElement extends CrLitElement {
  static get is() {
    return 'tab-group-dot';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      color: {type: Number},
      size: {
        type: String,
        reflect: true,
      },
      tabGroupColorRefresh_: {type: Boolean},
    };
  }

  accessor color: Color = Color.kGrey;
  accessor size: TabGroupDotSize = TabGroupDotSize.SMALL;
  accessor tabGroupColorRefresh_: boolean =
      loadTimeData.valueExists('useTabGroupColorRefresh') ?
      loadTimeData.getBoolean('useTabGroupColorRefresh') :
      false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('color') ||
        changedProperties.has('tabGroupColorRefresh_')) {
      this.style.setProperty(
          '--group-dot-color',
          getTabGroupColorVar(this.color, this.tabGroupColorRefresh_));
    }

    if (changedProperties.has('size')) {
      assert(Object.values(TabGroupDotSize).includes(this.size));
    }
  }

  protected getViewBox_(): string {
    return this.size === TabGroupDotSize.LARGE ? '-10 -10 20 20' :
                                                 '-5 -5 10 10';
  }

  protected getRadius_(): number {
    return this.size === TabGroupDotSize.LARGE ? 8 : 4;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-group-dot': TabGroupDotElement;
  }
}

customElements.define(TabGroupDotElement.is, TabGroupDotElement);
