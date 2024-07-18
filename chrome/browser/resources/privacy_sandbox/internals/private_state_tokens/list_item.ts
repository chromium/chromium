// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons_lit.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './list_item.css.js';
import {getHtml} from './list_item.html.js';
import type {Redemption} from './test_data.js';

export class PrivateStateTokensListItemElement extends CrLitElement {
  static get is() {
    return 'private-state-tokens-list-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      expanded: {type: Boolean},
      issuerOrigin: {type: String},
      numTokens: {type: Number},
      redemptions: {type: Array},
      index: {type: Number},
    };
  }

  expanded: boolean = false;
  issuerOrigin: string = '';
  numTokens: number = 0;
  redemptions: Redemption[] = [];
  index: number = -1;

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded = e.detail.value;
    this.dispatchEvent(new CustomEvent(
        'expanded-toggled',
        {detail: {id: this.issuerOrigin, expanded: this.expanded}}));
  }

  protected getNumTokensString_() {
    if (this.numTokens > 0) {
      return ` ${this.numTokens} token${
          this.redemptions.length > 0 ? 's,' : ''}`;
    }
    return '';
  }

  protected getRedemptionsString_() {
    if (this.redemptions.length === 0) {
      return '';
    }
    return `${this.redemptions.length} recent redemption${
        this.redemptions.length > 1 ? 's' : ''}`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'private-state-tokens-list-item': PrivateStateTokensListItemElement;
  }
}

customElements.define(
    PrivateStateTokensListItemElement.is, PrivateStateTokensListItemElement);
