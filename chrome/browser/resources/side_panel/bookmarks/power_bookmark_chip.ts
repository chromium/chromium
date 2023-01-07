// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/mwb_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './power_bookmark_chip.html.js';

export interface PowerBookmarkChipElement {
  $: {
    chip: HTMLDivElement,
  };
}

export class PowerBookmarkChipElement extends PolymerElement {
  static get is() {
    return 'power-bookmark-chip';
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
    'power-bookmark-chip': PowerBookmarkChipElement;
  }
}

customElements.define(PowerBookmarkChipElement.is, PowerBookmarkChipElement);
