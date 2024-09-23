// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './check_mark_wrapper.css.js';
import {getHtml} from './check_mark_wrapper.html.js';

export interface CheckMarkWrapperElement {
  $: {
    circle: HTMLElement,
  };
}

export class CheckMarkWrapperElement extends CrLitElement {
  static get is() {
    return 'customize-chrome-check-mark-wrapper';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      checked: {
        type: Boolean,
        reflect: true,
      },

      checkmarkBorderHidden: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  checked: boolean = false;
  checkmarkBorderHidden: boolean = false;
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-check-mark-wrapper': CheckMarkWrapperElement;
  }
}

customElements.define(CheckMarkWrapperElement.is, CheckMarkWrapperElement);
