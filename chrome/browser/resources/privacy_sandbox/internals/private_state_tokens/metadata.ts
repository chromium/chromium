// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../strings.m.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/icons_lit.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './metadata.css.js';
import {getHtml} from './metadata.html.js';

export class PrivateStateTokensMetadataElement extends CrLitElement {
  static get is() {
    return 'private-state-tokens-metadata';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      issuerOrigin: {type: String},
      expiration: {type: String},
      purposes: {type: Array},
    };
  }

  issuerOrigin: string = '';
  expiration: string = '';
  purposes: string[] = [];

  protected onClick_() {
    window.history.pushState(
        {}, '', 'chrome://privacy-sandbox-internals/private-state-tokens');
    window.dispatchEvent(new CustomEvent('navigate-to-container'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'private-state-tokens-metadata': PrivateStateTokensMetadataElement;
  }
}

customElements.define(
    PrivateStateTokensMetadataElement.is, PrivateStateTokensMetadataElement);
