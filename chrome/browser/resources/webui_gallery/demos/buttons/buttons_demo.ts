// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './buttons_demo.css.js';
import {getHtml} from './buttons_demo.html.js';

export class ButtonsDemoElement extends CrLitElement {
  static get is() {
    return 'buttons-demo';
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

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
  }
}

export const tagName = ButtonsDemoElement.is;

customElements.define(ButtonsDemoElement.is, ButtonsDemoElement);
