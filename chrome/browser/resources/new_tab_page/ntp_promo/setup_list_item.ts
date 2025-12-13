// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';
import './ntp_promo_icons.html.js';

import type {CrIconElement} from '//resources/cr_elements/cr_icon/cr_icon.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './setup_list_item.css.js';
import {getHtml} from './setup_list_item.html.js';

export interface SetupListItemElement {
  $: {
    actionIcon: CrIconElement,
    backing: HTMLElement,
    bodyIcon: HTMLElement,
    bodyText: HTMLElement,
  };
}

/**
 * Entry in an NTP Setup List. Represents a single promotion to show.
 */
export class SetupListItemElement extends CrLitElement {
  static get is() {
    return 'setup-list-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      actionButtonText: {type: String, reflect: true, useDefault: true},
      bodyIconName: {type: String, reflect: true, useDefault: true},
      bodyText: {type: String, reflect: true, useDefault: true},
      completed: {type: Boolean, reflect: true, useDefault: true},
      promoId: {type: String, reflect: true, useDefault: true},
    };
  }

  accessor actionButtonText: string = '';
  accessor bodyIconName: string = '';
  accessor bodyText: string = '';
  accessor completed: boolean = false;
  accessor promoId: string = '';

  protected onClick_() {
    const event = new CustomEvent(
        'ntp-promo-click',
        {composed: true, bubbles: true, detail: this.promoId});
    this.dispatchEvent(event);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'setup-list-item': SetupListItemElement;
  }
}

customElements.define(SetupListItemElement.is, SetupListItemElement);
