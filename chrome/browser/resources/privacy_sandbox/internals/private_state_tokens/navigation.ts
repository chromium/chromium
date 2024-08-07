// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './metadata.js';
import './list_container.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './navigation.html.js';
import {ItemsToRender, nullMetadataObj} from './types.js';
import type {ListItem, Metadata} from './types.js';

export class PrivateStateTokensNavigationElement extends CrLitElement {
  static get is() {
    return 'private-state-tokens-navigation';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      data: {type: Array},
      itemToRender: {type: String},
    };
  }

  data: ListItem[] = [];
  protected metadata_: Metadata = nullMetadataObj;
  protected itemToRender: ItemsToRender = ItemsToRender.ISSUER_LIST;
}

declare global {
  interface HTMLElementTagNameMap {
    'private-state-tokens-navigation': PrivateStateTokensNavigationElement;
  }
}

customElements.define(
    PrivateStateTokensNavigationElement.is,
    PrivateStateTokensNavigationElement);
