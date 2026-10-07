// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../privacy_icons.html.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';

// </if>

import {getCss as getCrIconsCss} from 'chrome://resources/cr_elements/cr_icons_lit.css.js';
import {getCss as getCrSharedStyleCss} from 'chrome://resources/cr_elements/cr_shared_style_lit.css.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {getCss as getSettingsColumnedSectionCss} from '../settings_columned_section_lit.css.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import {EntityDataManagerProxyImpl} from './entity_data_manager_proxy.js';
import {getHtml} from './walletable_pass_detection_toggle.html.js';

export interface SettingsWalletablePassDetectionToggleElement {
  $: {
    toggle: SettingsToggleButtonElement,
  };
}

export type WalletablePassDetectionToggleElement =
    SettingsWalletablePassDetectionToggleElement;

export class SettingsWalletablePassDetectionToggleElement extends CrLitElement {
  static get is() {
    return 'settings-walletable-pass-detection-toggle';
  }

  static override get styles() {
    return [
      getCrIconsCss(),
      getCrSharedStyleCss(),
      getSettingsColumnedSectionCss(),
      getSettingsSharedCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      walletablePassDetectionOptedIn_: {type: Object},
      ineligibleUser_: {type: Boolean},
    };
  }

  // Does not correspond to an actual pref - this is faked to allow
  // writing it into a GAIA-id keyed dictionary of opt-ins.
  protected accessor walletablePassDetectionOptedIn_:
      chrome.settingsPrivate.PrefObject<boolean> = {
    key: '',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: false,
  };
  protected accessor ineligibleUser_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();

    EntityDataManagerProxyImpl.getInstance()
        .getWalletablePassDetectionOptInStatus()
        .then(optedIn => {
          this.walletablePassDetectionOptedIn_ = {
            ...this.walletablePassDetectionOptedIn_,
            value: optedIn,
          };
        });
  }

  /**
   * Listener for `walletablePassDetectionPrefToggle` change event.
   */
  protected async onSettingsBooleanControlChange_(e: Event) {
    const toggle = e.target as SettingsToggleButtonElement;
    // `setWalletablePassDetectionOptInStatus` returns false when the user
    // tries to toggle the opt-in status when they're ineligible. This
    // shouldn't happen usually but in some cases it can happen.
    const eligibleUser = await EntityDataManagerProxyImpl.getInstance()
                        .setWalletablePassDetectionOptInStatus(toggle.checked);
    if (!eligibleUser) {
      this.walletablePassDetectionOptedIn_ = {
        ...this.walletablePassDetectionOptedIn_,
        value: false,
      };
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
