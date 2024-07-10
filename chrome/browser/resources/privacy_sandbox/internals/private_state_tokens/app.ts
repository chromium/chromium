// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import './toolbar.js';
import './list_item.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {dummyListItemData} from './test_data.js';
import type {ListItem} from './test_data.js';

export class PrivateStateTokensAppElement extends CrLitElement {
  static get is() {
    return 'private-state-tokens-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pageTitle_: {type: String},
      narrow_: {type: Boolean},
      data_: {type: Array},
    };
  }

  protected pageTitle_: string = 'Private State Tokens';
  protected narrow_: boolean = true;
  protected data_: ListItem[] = dummyListItemData;
}

declare global {
  interface HTMLElementTagNameMap {
    'private-state-tokens-app': PrivateStateTokensAppElement;
  }
}

customElements.define(PrivateStateTokensAppElement.is, PrivateStateTokensAppElement);
