// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './zero_state_overlay.css.js';
import {getHtml} from './zero_state_overlay.html.js';


export class ZeroStateOverlayElement extends CrLitElement {
  static get is() {
    return 'zero-state-overlay';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isFirstLoad: {
        type: Boolean,
        reflect: true,
      },
      isSidePanel: {
        type: Boolean,
        reflect: true,
      },
    };
  }
  accessor isFirstLoad: boolean = false;
  accessor isSidePanel: boolean = false;
  protected friendlyZeroStateSubtitle: string =
      loadTimeData.getString('friendlyZeroStateSubtitle');
  protected friendlyZeroStateTitle: string =
      loadTimeData.getString('friendlyZeroStateTitle');
}
declare global {
  interface HtmlElementTagNameMap {
    'zero-state-overlay': ZeroStateOverlayElement;
  }
}
customElements.define(ZeroStateOverlayElement.is, ZeroStateOverlayElement);
