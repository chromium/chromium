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

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
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

const WALLETABLE_PASS_DETECTION_OPTED_IN_PREF_KEY =
    'synthetic.settings.autofill.walletable_pass_detection_opted_in';

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
      ineligibleUser_: {type: Boolean},
    };
  }

  protected accessor ineligibleUser_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();

    // Does not correspond to an actual pref - this is faked to allow
    // writing it into a GAIA-id keyed dictionary of opt-ins.
    Promise
        .all([
          EntityDataManagerProxyImpl.getInstance()
              .getWalletablePassDetectionOptInStatus(),
          PrefService.getInstance().whenInitialized(),
        ])
        .then(([optedIn]) => {
          PrefService.getInstance().setPrefValue(
              WALLETABLE_PASS_DETECTION_OPTED_IN_PREF_KEY, optedIn);
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
      PrefService.getInstance().setPrefValue(
          WALLETABLE_PASS_DETECTION_OPTED_IN_PREF_KEY, false);
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
