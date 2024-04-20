// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Shared styles for showing an empty state for a side panel UI.
 */

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './sp_empty_state.css.js';
import {getHtml} from './sp_empty_state.html.js';

export class SpEmptyStateElement extends CrLitElement {
  static get is() {
    return 'sp-empty-state';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      body: {type: String},
      darkImagePath: {type: String},
      heading: {type: String},
      imagePath: {type: String},
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
