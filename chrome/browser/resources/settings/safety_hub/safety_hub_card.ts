// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-hub-card' is used by the top cards in Safety Hub settings
 * page.
 */
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../settings_shared.css.js';

import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CardInfo} from './safety_hub_browser_proxy.js';
import {CardState} from './safety_hub_browser_proxy.js';
import {getTemplate} from './safety_hub_card.html.js';

export interface SettingsSafetyHubCardElement {
  $: {
    icon: CrIconElement,
  };
}

export class SettingsSafetyHubCardElement extends PolymerElement {
  static get is() {
    return 'settings-safety-hub-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // The object to hold Card Info.
      data: Object,
    };
  }

  data: CardInfo;

  // Returns the icon for the card state.
  private getStatusIcon(state: CardState): string {
    switch (state) {
      case CardState.WARNING:
      case CardState.WEAK:
        return 'cr:error';
      case CardState.INFO:
        return 'cr:info';
      case CardState.SAFE:
        return 'cr:check-circle';
      default:
        assertNotReached();
    }
  }

  // Returns the color class for the icon to paint it.
  private getColorClass(state: CardState): string {
    switch (state) {
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
