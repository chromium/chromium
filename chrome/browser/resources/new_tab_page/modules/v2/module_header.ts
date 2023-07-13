// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nMixin} from '../../i18n_setup.js';

import {getTemplate} from './module_header.html.js';

/** Element that displays a header inside a module.  */
export class ModuleHeaderElementV2 extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-module-header-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      headerText: String,
      moreActionsText: String,
    };
  }

  headerText: string;
  moreActionsText: string;

  private onMenuButtonClick_() {
    this.dispatchEvent(new Event('menu-button-click', {bubbles: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-module-header-v2': ModuleHeaderElementV2;
  }
}

customElements.define(ModuleHeaderElementV2.is, ModuleHeaderElementV2);
