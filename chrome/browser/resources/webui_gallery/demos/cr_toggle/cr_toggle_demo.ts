// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_toggle/cr_toggle.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from '../demo.css.js';

import {getHtml} from './cr_toggle_demo.html.js';

export class CrToggleDemoElement extends CrLitElement {
  static get is() {
    return 'cr-toggle-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      checked_: {type: Boolean},
    };
  }

  protected checked_?: boolean;

  protected onCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.checked_ = e.detail.value;
  }
}

export const tagName = CrToggleDemoElement.is;

customElements.define(CrToggleDemoElement.is, CrToggleDemoElement);
