// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './button_label.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './hover_button.css.js';
import {getHtml} from './hover_button.html.js';

export interface HoverButtonElement {
  $: {
    hoverButton: HTMLDivElement,
  };
}

export class HoverButtonElement extends CrLitElement {
  static get is() {
    return 'customize-chrome-hover-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      ariaButtonLabel: {type: String},
      label: {type: String},
      labelDescription: {type: String},
    };
  }

  ariaButtonLabel: string|null = null;
  label: string = '';
  labelDescription: string|null = null;

  constructor() {
    super();
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
  }

  override focus() {
    this.$.hoverButton.focus();
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
