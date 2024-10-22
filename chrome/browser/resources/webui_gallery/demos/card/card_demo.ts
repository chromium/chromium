// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './card_demo.css.js';
import {getHtml} from './card_demo.html.js';

export class CardDemoElement extends CrLitElement {
  static get is() {
    return 'card-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      expanded_: {type: Boolean},
    };
  }

  protected expanded_: boolean = false;

  protected onExternalLinkClick_() {
    window.open('https://chromium.org');
  }

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
  }
}

export const tagName = CardDemoElement.is;

customElements.define(CardDemoElement.is, CardDemoElement);
