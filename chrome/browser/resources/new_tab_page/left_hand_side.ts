// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/threads_rail.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from './i18n_setup.js';
import {getCss} from './left_hand_side.css.js';
import {getHtml} from './left_hand_side.html.js';

export class LeftHandSideElement extends CrLitElement {
  static get is() {
    return 'cr-left-hand-side';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      enableThreadsRailHybrid_: {type: Boolean},
      expandedLhsUrl_: {type: String},
    };
  }

  protected accessor enableThreadsRailHybrid_: boolean =
      loadTimeData.getBoolean('enableThreadsRailHybrid');
  protected accessor expandedLhsUrl_: string =
      loadTimeData.getString('expandedLhsUrl');
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-left-hand-side': LeftHandSideElement;
  }
}

customElements.define(LeftHandSideElement.is, LeftHandSideElement);
