// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://performance-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://performance-side-panel.top-chrome/shared/sp_shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './battery_saver_card.html.js';

export interface BatterySaverCardElement {
  $: {};
}

export class BatterySaverCardElement extends PolymerElement {
  static get is() {
    return 'battery-saver-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'battery-saver-card': BatterySaverCardElement;
  }
}
customElements.define(BatterySaverCardElement.is, BatterySaverCardElement);
