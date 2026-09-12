// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-hub-card' is used by the top cards in Safety Hub settings
 * page.
 */
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';

import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CardInfo} from './safety_hub_browser_proxy.js';
import {CardState} from './safety_hub_browser_proxy.js';
import {getCss} from './safety_hub_card.css.js';
import {getHtml} from './safety_hub_card.html.js';

export interface SettingsSafetyHubCardElement {
  $: {
    header: HTMLElement,
    icon: CrIconElement,
    subheader: HTMLElement,
  };
}

export class SettingsSafetyHubCardElement extends CrLitElement {
  static get is() {
    return 'settings-safety-hub-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // The object to hold Card Info.
      data: {type: Object},
    };
  }

  accessor data: CardInfo = {
    header: '',
    subheader: '',
    state: CardState.INFO,
  };

  // Returns the icon for the card state.
  protected getStatusIcon_(): string {
    switch (this.data.state) {
      case CardState.WARNING:
      case CardState.WEAK:
        return 'cr:error-filled';
      case CardState.INFO:
        return 'cr:info-filled';
      case CardState.SAFE:
        return 'cr:check-circle';
      default:
        assertNotReached();
    }
  }

  // Returns the color class for the icon to paint it.
  protected getColorClass_(): string {
    switch (this.data.state) {
      case CardState.WARNING:
        return 'red';
      case CardState.WEAK:
        return 'yellow';
      case CardState.INFO:
        return 'grey';
      case CardState.SAFE:
        return 'green';
      default:
        assertNotReached();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-hub-card': SettingsSafetyHubCardElement;
  }
}

customElements.define(
    SettingsSafetyHubCardElement.is, SettingsSafetyHubCardElement);
