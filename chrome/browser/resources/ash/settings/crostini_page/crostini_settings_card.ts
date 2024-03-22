// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-settings-card' is the card element containing crostini settings.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isCrostiniAllowed, isCrostiniSupported, isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl} from './crostini_browser_proxy.js';
import {getTemplate} from './crostini_settings_card.html.js';

const CrostiniSettingsCardElementBase = DeepLinkingMixin(PrefsMixin(
    RouteOriginMixin(I18nMixin(WebUiListenerMixin(PolymerElement)))));

export class CrostiniSettingsCardElement extends
    CrostiniSettingsCardElementBase {
  static get is() {
    return 'crostini-settings-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether the install option should be enabled.
       */
      disableCrostiniInstall_: {
        type: Boolean,
      },

      isCrostiniSupported_: {
        type: Boolean,
        value: () => {
          return isCrostiniSupported();
        },
      },

      isCrostiniAllowed_: {
        type: Boolean,
        value: () => {
          return isCrostiniAllowed();
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([Setting.kSetUpCrostini]),
      },

      showBruschetta_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showBruschetta');
        },
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value: () => {
          return isRevampWayfindingEnabled();
        },
      },
    };
  }

  private browserProxy_: CrostiniBrowserProxy;
  private disableCrostiniInstall_: boolean;
  private isCrostiniAllowed_: boolean;
  private isCrostiniSupported_: boolean;
  private readonly isRevampWayfindingEnabled_: boolean;
  private readonly showBruschetta_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route =
        this.isRevampWayfindingEnabled_ ? routes.ABOUT : routes.CROSTINI;

    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    if (!this.isCrostiniAllowed_) {
      this.disableCrostiniInstall_ = true;
      return;
    }
    this.addWebUiListener(
        'crostini-installer-status-changed', (installerShowing: boolean) => {
          this.disableCrostiniInstall_ = installerShowing;
        });
    this.browserProxy_.requestCrostiniInstallerStatus();
  }

  override ready(): void {
    super.ready();

    this.addFocusConfig(routes.CROSTINI_DETAILS, '#crostini .subpage-arrow');
    this.addFocusConfig(
        routes.BRUSCHETTA_DETAILS, '#bruschetta .subpage-arrow');
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    super.currentRouteChanged(newRoute, oldRoute);

    // Does not apply to this page.
    if (newRoute !== this.route) {
      return;
    }

    this.attemptDeepLink();
  }

  private onEnableClick_(event: Event): void {
    this.browserProxy_.requestCrostiniInstallerView();
    event.stopPropagation();
    recordSettingChange(Setting.kSetUpCrostini);
  }

  private onSubpageClick_(event: Event): void {
    // We do not open the subpage if the click was on a link.
    if (event.target && (event.target as HTMLElement).tagName === 'A') {
      event.stopPropagation();
      return;
    }

    if (this.getPref('crostini.enabled').value) {
      Router.getInstance().navigateTo(routes.CROSTINI_DETAILS);
    }
  }

  private onBruschettaEnableClick_(event: Event): void {
    this.browserProxy_.requestBruschettaInstallerView();
    // Stop propagation so that onBruschettaSubpageClick_ isn't called.
    event.stopPropagation();
  }

  private onBruschettaSubpageClick_(): void {
    // This function is called on-click even if actionable=false.
    if (this.getPref('bruschetta.installed').value) {
      Router.getInstance().navigateTo(routes.BRUSCHETTA_DETAILS);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CrostiniSettingsCardElement.is]: CrostiniSettingsCardElement;
  }
}

customElements.define(
    CrostiniSettingsCardElement.is, CrostiniSettingsCardElement);
