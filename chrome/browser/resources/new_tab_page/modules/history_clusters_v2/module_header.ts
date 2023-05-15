// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nMixin} from '../../i18n_setup.js';

import {getTemplate} from './module_header.html.js';

export interface ModuleHeaderElementV2 {
  $: {
    actionMenu: CrActionMenuElement,
  };
}

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
      dismissText: String,
      disableText: String,
    };
  }

  dismissText: string;
  disableText: string;

  private onInfoButtonClick_() {
    this.$.actionMenu.close();
    this.dispatchEvent(
        new Event('info-button-click', {bubbles: true, composed: true}));
  }

  private onMenuButtonClick_(e: Event) {
    this.$.actionMenu.showAt(e.target as HTMLElement);
  }

  private onDismissButtonClick_() {
    this.$.actionMenu.close();
    this.dispatchEvent(new Event('dismiss-button-click', {bubbles: true}));
  }

  private onDisableButtonClick_() {
    this.$.actionMenu.close();
    this.dispatchEvent(new Event('disable-button-click', {bubbles: true}));
  }

  private onCustomizeButtonClick_() {
    this.$.actionMenu.close();
    this.dispatchEvent(
        new Event('customize-module', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-module-header-v2': ModuleHeaderElementV2;
  }
}

customElements.define(ModuleHeaderElementV2.is, ModuleHeaderElementV2);
