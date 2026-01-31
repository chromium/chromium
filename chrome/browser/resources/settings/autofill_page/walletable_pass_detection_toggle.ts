// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/shared/settings/prefs/prefs.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_columned_section.css.js';
import '../settings_shared.css.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';

// </if>

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';

import {EntityDataManagerProxyImpl} from './entity_data_manager_proxy.js';
import {getTemplate} from './walletable_pass_detection_toggle.html.js';

export interface SettingsWalletablePassDetectionToggleElement {
  $: {
    toggle: SettingsToggleButtonElement,
  };
}

export class SettingsWalletablePassDetectionToggleElement extends
    PolymerElement {
  static get is() {
    return 'settings-walletable-pass-detection-toggle';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      walletablePassDetectionOptedIn_: {
        type: Object,
        value: () => ({
          // Does not correspond to an actual pref - this is faked to allow
          // writing it into a GAIA-id keyed dictionary of opt-ins.
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        }),
      },
      ineligibleUser_: {
        type: Boolean,
        value: false,
      },
    };
  }

  declare private walletablePassDetectionOptedIn_:
      chrome.settingsPrivate.PrefObject;
  declare private ineligibleUser_: boolean;

  override connectedCallback() {
    super.connectedCallback();

    EntityDataManagerProxyImpl.getInstance()
        .getWalletablePassDetectionOptInStatus()
        .then(optedIn => {
          this.set('walletablePassDetectionOptedIn_.value', optedIn);
        });
  }

  /**
   * Listener for `walletablePassDetectionPrefToggle` change event.
   */
  private async onChange_(e: Event) {
    const toggle = e.target as SettingsToggleButtonElement;
    // `setWalletablePassDetectionOptInStatus` returns false when the user
    // tries to toggle the opt-in status when they're ineligible. This
    // shouldn't happen usually but in some cases it can happen.
    const eligibleUser = await EntityDataManagerProxyImpl.getInstance()
                        .setWalletablePassDetectionOptInStatus(toggle.checked);
    if (!eligibleUser) {
      this.set('walletablePassDetectionOptedIn_.value', false);
      this.ineligibleUser_ = true;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-walletable-pass-detection-toggle':
        SettingsWalletablePassDetectionToggleElement;
  }
}

customElements.define(
    SettingsWalletablePassDetectionToggleElement.is,
    SettingsWalletablePassDetectionToggleElement);
