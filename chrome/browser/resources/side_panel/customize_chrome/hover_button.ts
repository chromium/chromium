// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './hover_button.html.js';

export class HoverButtonElement extends PolymerElement {
  static get is() {
    return 'customize-chrome-hover-button';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      label: {
        reflectToAttribute: true,
        type: String,
      },

      labelDescription: {
        reflectToAttribute: true,
        type: String,
      },
    };
  }

  label: string;
  labelDescription: string|null = null;

  constructor() {
    super();
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
  }

  private onKeyDown_(e: KeyboardEvent) {
    if (e.key !== ' ' && e.key !== 'Enter') {
      return;
    }

    e.preventDefault();
    e.stopPropagation();
    this.click();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-hover-button': HoverButtonElement;
  }
}

customElements.define(HoverButtonElement.is, HoverButtonElement);
