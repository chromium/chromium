// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'bruschetta-subpage' is the settings subpage for managing Bruschetta
 * (third-party VMs).
 */

import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Router, routes} from '../router.js';

import {getTemplate} from './bruschetta_subpage.html.js';
import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl} from './crostini_browser_proxy.js';

const BruschettaSubpageElementBase =
    DeepLinkingMixin(RouteOriginMixin(PrefsMixin(PolymerElement)));

export class BruschettaSubpageElement extends BruschettaSubpageElementBase {
  static get is() {
    return 'settings-bruschetta-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showBruschettaMicPermissionDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kBruschettaMicAccess,
        ]),
      },
    };
  }

  static get observers() {
    return [
      'onInstalledChanged_(prefs.bruschetta.installed.value)',
    ];
  }

  private browserProxy_: CrostiniBrowserProxy;
  private showBruschettaMicPermissionDialog_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.BRUSCHETTA_DETAILS;

    // For now we reuse the Crostini browser proxy, we're both part of
    // crostini_page. At some point we may want to split them apart (or make
    // something for all guest OSs).
    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.addFocusConfig(
        routes.BRUSCHETTA_SHARED_USB_DEVICES, '#bruschettaSharedUsbDevicesRow');
    this.addFocusConfig(
        routes.BRUSCHETTA_SHARED_PATHS, '#bruschettaSharedPathsRow');
  }

  private onSharedUsbDevicesClick_(): void {
    Router.getInstance().navigateTo(routes.BRUSCHETTA_SHARED_USB_DEVICES);
  }

  private onSharedPathsClick_(): void {
    Router.getInstance().navigateTo(routes.BRUSCHETTA_SHARED_PATHS);
  }

  private onRemoveClick_(): void {
    this.browserProxy_.requestBruschettaUninstallerView();
  }

  private onInstalledChanged_(installed: boolean): void {
    if (!installed &&
        Router.getInstance().currentRoute === routes.BRUSCHETTA_DETAILS) {
      Router.getInstance().navigateToPreviousRoute();
    }
  }

  private getMicToggle_(): SettingsToggleButtonElement {
    return castExists(
        this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#bruschetta-mic-permission-toggle'));
  }

  /**
   * If a change to the mic settings requires Bruschetta to be restarted, a
   * dialog is shown.
   */
  private async onMicPermissionChange_(): Promise<void> {
    if (await this.browserProxy_.checkBruschettaIsRunning()) {
      this.showBruschettaMicPermissionDialog_ = true;
    } else {
      this.getMicToggle_().sendPrefChange();
    }
  }

  private onBruschettaMicPermissionDialogClose_(e: CustomEvent): void {
    const toggle = this.getMicToggle_();
    if (e.detail.accepted) {
      toggle.sendPrefChange();
      this.browserProxy_.shutdownBruschetta();
    } else {
      toggle.resetToPrefValue();
    }

    this.showBruschettaMicPermissionDialog_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [BruschettaSubpageElement.is]: BruschettaSubpageElement;
  }
}

customElements.define(BruschettaSubpageElement.is, BruschettaSubpageElement);
