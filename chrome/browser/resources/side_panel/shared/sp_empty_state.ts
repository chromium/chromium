// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Shared styles for showing an empty state for a side panel UI.
 */

import '//resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sp_empty_state.html.js';

export class SpEmptyStateElement extends PolymerElement {
  static get is() {
    return 'sp-empty-state';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      body: String,
      darkImagePath: String,
      heading: String,
      imagePath: String,
    };
  }

  body: string;
  darkImagePath: string;
  heading: string;
  imagePath: string;
}

declare global {
  interface HTMLElementTagNameMap {
    'sp-empty-state': SpEmptyStateElement;
  }
}

customElements.define(SpEmptyStateElement.is, SpEmptyStateElement);
