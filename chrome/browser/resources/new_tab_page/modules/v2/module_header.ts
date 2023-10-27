// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {assert} from 'chrome://resources/js/assert.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nMixin} from '../../i18n_setup.js';

import {getTemplate} from './module_header.html.js';

export interface MenuItem {
  action: string;
  icon: string;
  text: string;
}

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
      headerText: String,
      moreActionsText: String,
      menuItemGroups: Array,
    };
  }

  headerText: string;
  menuItemGroups: MenuItem[][];
  moreActionsText: string;

  showAt(e: Event) {
    this.$.actionMenu.showAt(e.target as HTMLElement);
  }

  private onButtonClick_(e: DomRepeatEvent<MenuItem>) {
    const {action} = e.model.item;
    assert(action);
    e.stopPropagation();
    this.$.actionMenu.close();
    if (action === 'customize-module') {
      this.dispatchEvent(
          new Event('customize-module', {bubbles: true, composed: true}));
    } else {
      this.dispatchEvent(new Event(
          `${action}-button-click`, {bubbles: true, composed: true}));
    }
  }

  private onMenuButtonClick_(e: Event) {
    e.stopPropagation();
    this.dispatchEvent(
        new Event('menu-button-click', {bubbles: true, composed: true}));
  }

  private showDivider_(index: number): boolean {
    return index === 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-module-header-v2': ModuleHeaderElementV2;
  }
}

customElements.define(ModuleHeaderElementV2.is, ModuleHeaderElementV2);
