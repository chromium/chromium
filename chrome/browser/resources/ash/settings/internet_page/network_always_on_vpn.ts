// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and modifying always-on VPN
 * settings.
 */
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import 'chrome://resources/ash/common/network/network_shared.css.js';

import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {AlwaysOnVpnMode} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {cast} from '../assert_extras.js';

import {getTemplate} from './network_always_on_vpn.html.js';

interface VpnServiceOption {
  name: string;
  value: string;
  selected: boolean;
}

const NetworkAlwaysOnVpnElementBase = I18nMixin(PolymerElement);

export class NetworkAlwaysOnVpnElement extends NetworkAlwaysOnVpnElementBase {
  static get is() {
    return 'network-always-on-vpn' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * List of all always-on VPN compatible network states.
       */
      networks: Array,

      /**
       * Always-on VPN operating mode.
       */
      mode: {
        type: Number,
        notify: true,
      },

      /**
       * Always-on VPN service automatically started on login.
       */
      service: {
        type: String,
        notify: true,
      },
    };
  }

  mode: AlwaysOnVpnMode|undefined;
  networks: OncMojo.NetworkStateProperties[];
  service: string|undefined;

  /**
   * Tells whether the always-on VPN main toggle is disabled or not. The toggle
   * is disabled when there's no compatible VPN networks available.
   */
  private shouldDisableAlwaysOnVpn_(): boolean {
    return this.networks.length === 0;
  }

  /**
   * Computes the visibility of always-on VPN networks list and lockdown toggle.
   * These settings are visible when always-on VPN is enabled.
   */
  private shouldShowAlwaysOnVpnOptions_(): boolean {
    return !this.shouldDisableAlwaysOnVpn_() &&
        this.mode !== AlwaysOnVpnMode.kOff;
  }

  /**
   * Computes the checked value for the always-on VPN enabled toggle.
   */
  private computeAlwaysOnVpnEnabled_(): boolean {
    return !this.shouldDisableAlwaysOnVpn_() &&
        this.mode !== AlwaysOnVpnMode.kOff;
  }

  /**
   * Handles a state change on always-on VPN enable toggle.
   */
  private onAlwaysOnEnableChanged_(event: Event): void {
    const toggleEl = cast(event.target, CrToggleElement);
    if (!toggleEl.checked) {
      this.mode = AlwaysOnVpnMode.kOff;
      return;
    }
    this.mode = AlwaysOnVpnMode.kBestEffort;
  }

  /**
   * Deduces the lockdown state from the always-on VPN mode.
   */
  private computeAlwaysOnVpnLockdown_(): boolean {
    return this.mode === AlwaysOnVpnMode.kStrict;
  }

  /**
   * Handles a lockdown toggle state change. It reflects the change on the
   * current always-on VPN mode.
   */
  private onAlwaysOnVpnLockdownChanged_(event: Event): void {
    if (this.mode === AlwaysOnVpnMode.kOff) {
      // The event should not be fired when always-on VPN is disabled (the
      // enable toggle is disabled).
      return;
    }
    const toggleEl = cast(event.target, CrToggleElement);
    this.mode = toggleEl.checked ? AlwaysOnVpnMode.kStrict :
                                   AlwaysOnVpnMode.kBestEffort;
  }

  /**
   * Returns a list of options for a settings-dropdown-menu from a network
   * state list.
   * @return {!Array<{name: string, value: string, selected: boolean}>}
   * @private
   */
  private getAlwaysOnVpnListOptions_(): VpnServiceOption[] {
    const options: VpnServiceOption[] = [];
    const currentService = this.service;
    let serviceIsInList = false;

    if (!this.networks) {
      return options;
    }

    // Build a list of options for the service dropdown menu.
    this.networks.forEach(state => {
      options.push({
        name: state.name,
        value: state.guid,
        selected: currentService === state.guid,
      });
      serviceIsInList = serviceIsInList || (currentService === state.guid);
    });

    // If the current always-on VPN service is not in the VPN network list, it
    // needs a placeholder.
    if (!serviceIsInList) {
      options.unshift({
        name: '',
        value: '',
        selected: true,
      });
    }

    return options;
  }

  private onAlwaysOnVpnServiceChanged_(event: Event): void {
    const selectEl = cast(event.target, HTMLSelectElement);
    this.service = selectEl.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkAlwaysOnVpnElement.is]: NetworkAlwaysOnVpnElement;
  }
}

customElements.define(NetworkAlwaysOnVpnElement.is, NetworkAlwaysOnVpnElement);
