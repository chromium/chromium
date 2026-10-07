// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/threads_rail.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

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
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-left-hand-side': LeftHandSideElement;
  }
}

customElements.define(LeftHandSideElement.is, LeftHandSideElement);
