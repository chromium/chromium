// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element hosting <network-proxy> in the internet
 * detail page. This element is responsible for setting 'Allow proxies for
 * shared networks'.
 */

import 'chrome://resources/ash/common/network/cr_policy_network_indicator_mojo.js';
import 'chrome://resources/ash/common/network/network_proxy.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/extension_controlled_indicator.js';
import '../settings_vars.css.js';
import './internet_shared.css.js';
import '../controls/settings_toggle_button.js';
import '../common/lacros_extension_controlled_indicator.js';

import {PrefsMixin, PrefsMixinInterface} from '/shared/settings/prefs/prefs_mixin.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {CrPolicyNetworkBehaviorMojo, CrPolicyNetworkBehaviorMojoInterface} from 'chrome://resources/ash/common/network/cr_policy_network_behavior_mojo.js';
import {NetworkProxyElement} from 'chrome://resources/ash/common/network/network_proxy.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ManagedProperties, ManagedString} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RouteObserverMixin, RouteObserverMixinInterface} from '../common/route_observer_mixin.js';
import {Constructor} from '../common/types.js';
import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './network_proxy_section.html.js';

interface ExtensionInfo {
  name: string|undefined;
  id: string|undefined;
  canBeDisabled: boolean|undefined;
}

export interface NetworkProxySectionElement {
  $: {
    allowShared: SettingsToggleButtonElement,
    confirmAllowSharedDialog: CrDialogElement,
  };
}

const NetworkProxySectionElementBase =
    mixinBehaviors(
        [
          CrPolicyNetworkBehaviorMojo,
        ],
        PrefsMixin(RouteObserverMixin(I18nMixin(PolymerElement)))) as
    Constructor<PolymerElement&I18nMixinInterface&RouteObserverMixinInterface&
                PrefsMixinInterface&CrPolicyNetworkBehaviorMojoInterface>;

export class NetworkProxySectionElement extends NetworkProxySectionElementBase {
  static get is() {
    return 'network-proxy-section' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        value: false,
      },

      managedProperties: Object,

      /**
       * Reflects prefs.settings.use_shared_proxies for data binding.
       */
      useSharedProxies_: Boolean,

      /**
       * Indicates if the proxy if set by an extension in the Lacros primary
       * profile.
       */
      isProxySetByLacrosExtension_: Boolean,

      /**
       * Information about the extension in the Ash or Lacros browser which
       * controlling the proxy. Can be null is the proxy is not controlled by an
       * extension.
       */
      extensionInfo_: Object,
    };
  }

  static get observers() {
    return [
      'useSharedProxiesChanged_(prefs.settings.use_shared_proxies.value)',
      'extensionProxyChanged_(prefs.ash.lacros_proxy_controlling_extension, ' +
          'managedProperties.proxySettings)',
    ];
  }

  disabled: boolean;
  managedProperties: ManagedProperties|undefined;
  private extensionInfo_: ExtensionInfo|undefined;
  private isProxySetByLacrosExtension_: boolean;
  private useSharedProxies_: boolean;

  /**
   * Returns the allow shared CrToggleElement.
   */
  getAllowSharedToggle(): CrToggleElement|null {
    return this.shadowRoot!.querySelector<CrToggleElement>('#allowShared');
  }

  override currentRouteChanged(newRoute: Route): void {
    if (newRoute === routes.NETWORK_DETAIL) {
      this.shadowRoot!.querySelector<NetworkProxyElement>(
                          'network-proxy')!.reset();
    }
  }

  private useSharedProxiesChanged_(): void {
    const pref = this.getPref('settings.use_shared_proxies');
    this.useSharedProxies_ = !!pref && !!pref.value;
  }

  private extensionProxyChanged_(): void {
    if (this.proxySetByAshExtension_()) {
      return;
    }

    const property = this.getProxySettingsTypeProperty_();
    if (!property || !this.isExtensionControlled(property)) {
      this.isProxySetByLacrosExtension_ = false;
      return;
    }

    const pref = this.getPref('ash.lacros_proxy_controlling_extension');
    this.isProxySetByLacrosExtension_ = !!pref.value &&
        !!pref.value['extension_id_key'] && !!pref.value['extension_name_key'];
    if (this.isProxySetByLacrosExtension_) {
      this.extensionInfo_ = {
        id: pref.value['extension_id_key'],
        name: pref.value['extension_name_key'],
        canBeDisabled: pref.value['can_be_disabled_key'],
      };
    }
  }

  private proxySetByAshExtension_(): boolean {
    const property = this.getProxySettingsTypeProperty_();
    if (!property || !this.isExtensionControlled(property) ||
        !this.prefs.proxy.controlledByName) {
      return false;
    }
    this.extensionInfo_ = {
      id: this.prefs.proxy.extensionId,
      name: this.prefs.proxy.controlledByName,
      canBeDisabled: this.prefs.proxy.extensionCanBeDisabled,
    };
    return true;
  }

  /**
   * Return true if the proxy is controlled by an extension in the Ash Browser
   * or in the Lacros Browser.
   */
  private isProxySetByExtension_(): boolean {
    return this.proxySetByAshExtension_() || this.isProxySetByLacrosExtension_;
  }

  private isShared_(): boolean {
    return this.managedProperties!.source === OncSource.kDevice ||
        this.managedProperties!.source === OncSource.kDevicePolicy;
  }

  private getProxySettingsTypeProperty_(): ManagedString|undefined {
    if (!this.managedProperties) {
      return undefined;
    }
    const proxySettings = this.managedProperties.proxySettings;
    return proxySettings ? proxySettings.type : undefined;
  }

  private getAllowSharedDialogTitle_(allowShared: boolean): string {
    if (allowShared) {
      return this.i18n('networkProxyAllowSharedDisableWarningTitle');
    }
    return this.i18n('networkProxyAllowSharedEnableWarningTitle');
  }

  private shouldShowNetworkPolicyIndicator_(): boolean {
    const property = this.getProxySettingsTypeProperty_();
    return !!property && !this.isProxySetByExtension_() &&
        this.isNetworkPolicyEnforced(property);
  }

  private shouldShowAllowShared_(_property: OncMojo.ManagedProperty): boolean {
    if (!this.isShared_()) {
      return false;
    }
    // We currently do not accurately determine the source if the policy
    // controlling the proxy setting, so always show the 'allow shared'
    // toggle for shared networks. http://crbug.com/662529.
    return true;
  }

  /**
   * Handles the change event for the shared proxy checkbox. Shows a
   * confirmation dialog.
   */
  private onAllowSharedProxiesChange_(): void {
    this.$.confirmAllowSharedDialog.showModal();
  }

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   */
  private onAllowSharedDialogConfirm_(): void {
    this.$.allowShared.sendPrefChange();
    this.$.confirmAllowSharedDialog.close();
  }

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
   * event.
   */
  private onAllowSharedDialogCancel_(): void {
    this.$.allowShared.resetToPrefValue();
    this.$.confirmAllowSharedDialog.close();
  }

  private onAllowSharedDialogClose_(): void {
    this.$.allowShared.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkProxySectionElement.is]: NetworkProxySectionElement;
  }
}

customElements.define(
    NetworkProxySectionElement.is, NetworkProxySectionElement);
