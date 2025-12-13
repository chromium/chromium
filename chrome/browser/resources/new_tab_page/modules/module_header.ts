// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './module_header.css.js';
import {getHtml} from './module_header.html.js';

export interface MenuItem {
  action: string;
  icon: string;
  text: string;
}

export interface ModuleHeaderElement {
  $: {
    actionMenu: CrActionMenuElement,
    menuButton: HTMLElement,
    title: HTMLElement,
  };
}

/** Element that displays a header inside a module.  */
export class ModuleHeaderElement extends CrLitElement {
  static get is() {
    return 'ntp-module-header';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      headerText: {type: String},
      moreActionsText: {type: String},
      menuItems: {type: Array},
      hideCustomize: {type: Boolean},
    };
  }

  accessor headerText: string|null = null;
  accessor menuItems: MenuItem[] = [];
  accessor moreActionsText: string = '';
  accessor hideCustomize: boolean = false;

  protected onButtonClick_(e: Event) {
    const action = (e.currentTarget as HTMLElement).dataset['action'];
    assert(action);
    e.stopPropagation();
    this.$.actionMenu.close();
    if (action === 'customize-module') {
      this.dispatchEvent(
          new Event('customize-module', {bubbles: true, composed: true}));
    } else {
      this.dispatchEvent(
          new Event(`${action}-button-click`, {bubbles: true, composed: true}));
    }
  }

  protected onMenuButtonClick_(e: Event) {
    this.$.actionMenu.showAt(e.target as HTMLElement);
  }

  protected showDivider_(): boolean {
    return this.menuItems?.length > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-module-header': ModuleHeaderElement;
  }
}

customElements.define(ModuleHeaderElement.is, ModuleHeaderElement);
