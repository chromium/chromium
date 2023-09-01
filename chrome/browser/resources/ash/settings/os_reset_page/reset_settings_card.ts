// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'reset-settings-card' is the card element containing reset settings.
 */
import '../settings_shared.css.js';
import '../os_settings_page/settings_card.js';
import './os_powerwash_dialog.js';

import {getEuicc, getNonPendingESimProfiles} from 'chrome://resources/ash/common/cellular_setup/esim_manager_utils.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {ESimProfileRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './reset_settings_card.html.js';

export interface ResetSettingsCardElement {
  $: {
    powerwashButton: CrButtonElement,
  };
}

const ResetSettingsCardElementBase =
    DeepLinkingMixin(RouteObserverMixin(PolymerElement));

export class ResetSettingsCardElement extends ResetSettingsCardElementBase {
  static get is() {
    return 'reset-settings-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showPowerwashDialog_: {
        type: Boolean,
        value: false,
      },

      installedESimProfiles_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([Setting.kPowerwash]),
      },
    };
  }

  private installedESimProfiles_: ESimProfileRemote[];
  private route_: Route;
  private showPowerwashDialog_: boolean;

  constructor() {
    super();

    this.route_ = isRevampWayfindingEnabled() ? routes.SYSTEM_PREFERENCES :
                                                routes.OS_RESET;
  }

  private async onShowPowerwashDialog_(e: Event): Promise<void> {
    e.preventDefault();

    const euicc = await getEuicc();
    if (!euicc) {
      this.installedESimProfiles_ = [];
      this.showPowerwashDialog_ = true;
      return;
    }

    const profiles = await getNonPendingESimProfiles(euicc);
    this.installedESimProfiles_ = profiles;
    this.showPowerwashDialog_ = true;
  }

  private onPowerwashDialogClose_(): void {
    this.showPowerwashDialog_ = false;
    focusWithoutInk(this.$.powerwashButton);
  }

  override currentRouteChanged(newRoute: Route): void {
    // Check route change applies to this page.
    if (newRoute !== this.route_) {
      return;
    }

    this.attemptDeepLink();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ResetSettingsCardElement.is]: ResetSettingsCardElement;
  }
}

customElements.define(ResetSettingsCardElement.is, ResetSettingsCardElement);
