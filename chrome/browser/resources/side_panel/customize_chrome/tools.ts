// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './tools.css.js';
import {getHtml} from './tools.html.js';

export interface ToolChipsElement {
  $: {
    showToggle: CrToggleElement,
  };
}

export class ToolChipsElement extends CrLitElement {
  static get is() {
    return 'customize-chrome-tools';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isChipsEnabled_: {type: Boolean},
    };
  }

  protected accessor isChipsEnabled_: boolean = false;

  constructor() {
    super();
  }

  // TODO(Bug: 450288644): Implement logic for setting tool chips visibility.
  private setChipsEnabled_(isEnabled: boolean) {
    this.isChipsEnabled_ = isEnabled;
  }

  protected onShowToggleChange_(e: CustomEvent<boolean>) {
    this.setChipsEnabled_(e.detail);
  }

  protected onShowToggleClick_() {
    this.setChipsEnabled_(!this.isChipsEnabled_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-tools': ToolChipsElement;
  }
}

customElements.define(ToolChipsElement.is, ToolChipsElement);
