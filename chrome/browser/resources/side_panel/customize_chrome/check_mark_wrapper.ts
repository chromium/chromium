// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './check_mark_wrapper.html.js';

export interface CheckMarkWrapperElement {
  $: {
    svg: Element,
  };
}

export class CheckMarkWrapperElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-check-mark-wrapper';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      checked: {
        type: Boolean,
        reflectToAttribute: true,
      },
      checkmarkBorderHidden: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  checked: boolean;
  checkmarkBorderHidden: boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-check-mark-wrapper': CheckMarkWrapperElement;
  }
}

customElements.define(CheckMarkWrapperElement.is, CheckMarkWrapperElement);
