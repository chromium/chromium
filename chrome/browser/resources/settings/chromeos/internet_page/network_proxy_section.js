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
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../../controls/extension_controlled_indicator.js';
import '../../settings_vars.css.js';
import './internet_shared.css.js';
import '../../controls/settings_toggle_button.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {CrPolicyNetworkBehaviorMojo, CrPolicyNetworkBehaviorMojoInterface} from 'chrome://resources/ash/common/network/cr_policy_network_behavior_mojo.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ManagedProperties, ManagedString} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../os_route.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

/**
 * @typedef {{name: (string|undefined),
 *            id: (string|undefined),
 *            canBeDisabled: (boolean|undefined)}}
 */
let ExtensionInfo;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrPolicyNetworkBehaviorMojoInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const NetworkProxySectionElementBase = mixinBehaviors(
    [
      CrPolicyNetworkBehaviorMojo,
      I18nBehavior,
      PrefsBehavior,
      RouteObserverBehavior,
    ],
    PolymerElement);

/** @polymer */
class NetworkProxySectionElement extends NetworkProxySectionElementBase {
  static get is() {
    return 'network-proxy-section';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        value: false,
      },

      /** @type {!ManagedProperties|undefined} */
      managedProperties: Object,

      /**
       * Reflects prefs.settings.use_shared_proxies for data binding.
       * @private
       */
      useSharedProxies_: Boolean,

      /**
       * Indicates if the proxy if set by an extension in the Lacros primary
       * profile.
       * @private
       */
      isProxySetByLacrosExtension_: Boolean,

      /**
       * Information about the extension in the Ash or Lacros browser which
       * controlling the proxy. Can be null is the proxy is not controlled by an
       * extension.
       * @type {!ExtensionInfo|undefined}
       * @private
       */
      extensionInfo_: Object,
    };
  }

  static get observers() {
    return [
      'useSharedProxiesChanged_(prefs.settings.use_shared_proxies.value)',
      'extensionProxyChanged_(prefs.ash.lacros_proxy_controlling_extension)',
    ];
  }

  /**
   * Returns the allow shared CrToggleElement.
   * @return {?CrToggleElement}
   */
  getAllowSharedToggle() {
    return /** @type {?CrToggleElement} */ (
        this.shadowRoot.querySelector('#allowShared'));
  }

  /** @protected RouteObserverBehavior */
  currentRouteChanged(newRoute) {
    if (newRoute === routes.NETWORK_DETAIL) {
      /** @type {NetworkProxyElement} */ (
          this.shadowRoot.querySelector('network-proxy'))
          .reset();
    }
  }

  /** @private */
  useSharedProxiesChanged_() {
    const pref = this.getPref('settings.use_shared_proxies');
    this.useSharedProxies_ = !!pref && !!pref.value;
  }

  /** @private */
  extensionProxyChanged_() {
    if (this.proxySetByAshExtension_()) {
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

  /**
   * @return {boolean}
   * @private
   */
  proxySetByAshExtension_() {
    const property = this.getProxySettingsTypeProperty_();
    if (!property || !this.isExtensionControlled(property)) {
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
   * @returns {boolean}
   * @private
   */
  isProxySetByExtension_() {
    return this.proxySetByAshExtension_() || this.isProxySetByLacrosExtension_;
  }

  /**
   * @return {boolean}
   * @private
   */
  isShared_() {
    return this.managedProperties.source === OncSource.kDevice ||
        this.managedProperties.source === OncSource.kDevicePolicy;
  }

  /**
   * @return {!ManagedString|undefined}
   * @private
   */
  getProxySettingsTypeProperty_() {
    if (!this.managedProperties) {
      return undefined;
    }
    const proxySettings = this.managedProperties.proxySettings;
    return proxySettings ? proxySettings.type : undefined;
  }

  /**
   * @param {boolean} allowShared
   * @return {string}
   * @private
   */
  getAllowSharedDialogTitle_(allowShared) {
    if (allowShared) {
      return this.i18n('networkProxyAllowSharedDisableWarningTitle');
    }
    return this.i18n('networkProxyAllowSharedEnableWarningTitle');
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowNetworkPolicyIndicator_() {
    const property = this.getProxySettingsTypeProperty_();
    return !!property && !this.isProxySetByExtension_() &&
        this.isNetworkPolicyEnforced(property);
  }

  /**
   * @param {!OncMojo.ManagedProperty} property
   * @return {boolean}
   * @private
   */
  shouldShowAllowShared_(property) {
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
   * @param {!Event} event
   * @private
   */
  onAllowSharedProxiesChange_(event) {
    this.$.confirmAllowSharedDialog.showModal();
  }

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   * @private
   */
  onAllowSharedDialogConfirm_() {
    /** @type {!SettingsToggleButtonElement} */ (this.$.allowShared)
        .sendPrefChange();
    this.$.confirmAllowSharedDialog.close();
  }

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
   * event.
   * @private
   */
  onAllowSharedDialogCancel_() {
    /** @type {!SettingsToggleButtonElement} */ (this.$.allowShared)
        .resetToPrefValue();
    this.$.confirmAllowSharedDialog.close();
  }

  /** @private */
  onAllowSharedDialogClose_() {
    this.$.allowShared.focus();
  }
}

customElements.define(
    NetworkProxySectionElement.is, NetworkProxySectionElement);
