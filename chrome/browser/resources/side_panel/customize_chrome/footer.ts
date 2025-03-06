// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {CustomizeChromeAction, recordCustomizeChromeAction} from './common.js';
import {getCss} from './footer.css.js';
import {getHtml} from './footer.html.js';

export interface FooterElement {
  $: {
    showToggle: CrToggleElement,
  };
}

export class FooterElement extends CrLitElement {
  static get is() {
    return 'customize-chrome-footer';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      show_: {type: Boolean},
    };
  }

  protected show_: boolean = false;

  private setShow_(show: boolean) {
    recordCustomizeChromeAction(
        CustomizeChromeAction.SHOW_FOOTER_TOGGLE_CLICKED);
    this.show_ = show;
  }

  protected onShowToggleChange_(e: CustomEvent<boolean>) {
    this.setShow_(e.detail);
  }

  // TODO(crbug.com/399179081): Add tests for this function once the pref is
  // added.
  protected onShowToggleClick_() {
    this.setShow_(!this.show_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-footer': FooterElement;
  }
}

customElements.define(FooterElement.is, FooterElement);
