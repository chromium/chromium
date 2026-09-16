// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getTabGroupColorVar} from './tab_group_color_helper.js';
import {getCss} from './tab_group_dot.css.js';
import {getHtml} from './tab_group_dot.html.js';
import {Color} from './tab_group_types.mojom-webui.js';

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
      tabGroupColorRefresh_: {type: Boolean},
    };
  }

  accessor color: Color = Color.kGrey;
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
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-group-dot': TabGroupDotElement;
  }
}

customElements.define(TabGroupDotElement.is, TabGroupDotElement);
