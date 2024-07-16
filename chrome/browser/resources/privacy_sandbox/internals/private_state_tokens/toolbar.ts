// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar.js';

import type {CrToolbarElement} from '//resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './toolbar.css.js';
import {getHtml} from './toolbar.html.js';

export interface PrivateStateTokensToolbarElement {
  $: {mainToolbar: CrToolbarElement};
}

export class PrivateStateTokensToolbarElement extends CrLitElement {
  static get is() {
    return 'private-state-tokens-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      autoFocus_: {type: Boolean},
      alwaysShowLogo_: {type: Boolean},
      clearLabel_: {type: String},
      menuLabel_: {type: String},
      narrow: {type: Boolean},
      narrowThreshold: {type: Number},
      pageName: {type: String},
      searchPrompt_: {type: String},
      showSearch_: {type: Boolean},
    };
  }

  protected autoFocus_: boolean = true;
  protected alwaysShowLogo_: boolean = true;
  protected clearLabel_: string = 'Clear search';
  protected menuLabel_: string = 'Menu';
  protected narrow: boolean = false;
  narrowThreshold: number = 0;
  protected pageName: string = '';
  protected searchPrompt_: string = 'Search site';

}

declare global {
  interface HTMLElementTagNameMap {
    'private-state-tokens-toolbar': PrivateStateTokensToolbarElement;
  }
}

customElements.define(
    PrivateStateTokensToolbarElement.is, PrivateStateTokensToolbarElement);
