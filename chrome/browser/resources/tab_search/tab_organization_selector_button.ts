// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './tab_organization_selector_button.css.js';
import {getHtml} from './tab_organization_selector_button.html.js';

export class TabOrganizationSelectorButtonElement extends CrLitElement {
  static get is() {
    return 'tab-organization-selector-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      bottom: {type: Boolean, reflect: true},
      disabled: {type: Boolean, reflect: true},
      heading: {type: String},
      icon: {type: String},
      subheading: {type: String},
      top: {type: Boolean, reflect: true},
    };
  }

  bottom: boolean = false;
  disabled: boolean = false;
  heading: string = '';
  icon: string = 'cr:error';
  subheading: string = '';
  top: boolean = false;
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-selector-button': TabOrganizationSelectorButtonElement;
  }
}

customElements.define(
    TabOrganizationSelectorButtonElement.is,
    TabOrganizationSelectorButtonElement);
