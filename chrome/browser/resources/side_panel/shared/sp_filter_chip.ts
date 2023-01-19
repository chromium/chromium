// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sp_filter_chip.html.js';

export class SpFilterChip extends PolymerElement {
  static get is() {
    return 'sp-filter-chip';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: Boolean,
      selected: Boolean,
    };
  }

  disabled: boolean;
  selected: boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    'sp-filter-chip': SpFilterChip;
  }
}

customElements.define(SpFilterChip.is, SpFilterChip);
