// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element hosting <network-proxy> in the internet
 * detail page. This element is responsible for setting 'Allow proxies for
 * shared networks'.
 */

import '//resources/cr_components/chromeos/network/cr_policy_network_indicator_mojo.m.js';
import '//resources/cr_components/chromeos/network/network_proxy.m.js';
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/cr_elements/hidden_style_css.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../../controls/extension_controlled_indicator.js';
import '../../settings_vars_css.js';
import './internet_shared_css.js';

import {CrPolicyNetworkBehaviorMojo} from '//resources/cr_components/chromeos/network/cr_policy_network_behavior_mojo.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {PrefsBehavior} from '../../prefs/prefs_behavior.js';
import {Route, RouteObserverBehavior, Router} from '../../router.js';
import {routes} from '../os_route.m.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'network-proxy-section',

  behaviors: [
    CrPolicyNetworkBehaviorMojo,
    I18nBehavior,
    PrefsBehavior,
    RouteObserverBehavior,
  ],

  properties: {
    disabled: {
      type: Boolean,
      value: false,
    },

    /** @type {!chromeos.networkConfig.mojom.ManagedProperties|undefined} */
    managedProperties: Object,

    /**
     * Reflects prefs.settings.use_shared_proxies for data binding.
     * @private
     */
    useSharedProxies_: Boolean,
  },

  observers: [
    'useSharedProxiesChanged_(prefs.settings.use_shared_proxies.value)',
  ],

  /**
   * Returns the allow shared CrToggleElement.
   * @return {?CrToggleElement}
   */
  getAllowSharedToggle() {
    return /** @type {?CrToggleElement} */ (this.$$('#allowShared'));
  },

  /** @protected RouteObserverBehavior */
  currentRouteChanged(newRoute) {
    if (newRoute === routes.NETWORK_DETAIL) {
      /** @type {NetworkProxyElement} */ (this.$$('network-proxy')).reset();
    }
  },

  /** @private */
  useSharedProxiesChanged_() {
    const pref = this.getPref('settings.use_shared_proxies');
    this.useSharedProxies_ = !!pref && !!pref.value;
  },

  /**
   * @return {boolean}
   * @private
   */
  isShared_() {
    const mojom = chromeos.networkConfig.mojom;
    return this.managedProperties.source === mojom.OncSource.kDevice ||
        this.managedProperties.source === mojom.OncSource.kDevicePolicy;
  },

  /**
   * @return {!chromeos.networkConfig.mojom.ManagedString|undefined}
   * @private
   */
  getProxySettingsTypeProperty_() {
    const proxySettings = this.managedProperties.proxySettings;
    return proxySettings ? proxySettings.type : undefined;
  },

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
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowNetworkPolicyIndicator_() {
    const property = this.getProxySettingsTypeProperty_();
    return !!property && !this.isExtensionControlled(property) &&
        this.isNetworkPolicyEnforced(property);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowExtensionIndicator_() {
    const property = this.getProxySettingsTypeProperty_();
    return !!property && this.isExtensionControlled(property);
  },

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
  },

  /**
   * Handles the change event for the shared proxy checkbox. Shows a
   * confirmation dialog.
   * @param {!Event} event
   * @private
   */
  onAllowSharedProxiesChange_(event) {
    this.$.confirmAllowSharedDialog.showModal();
  },

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   * @private
   */
  onAllowSharedDialogConfirm_() {
    /** @type {!SettingsToggleButtonElement} */ (this.$.allowShared)
        .sendPrefChange();
    this.$.confirmAllowSharedDialog.close();
  },

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
   * event.
   * @private
   */
  onAllowSharedDialogCancel_() {
    /** @type {!SettingsToggleButtonElement} */ (this.$.allowShared)
        .resetToPrefValue();
    this.$.confirmAllowSharedDialog.close();
  },

  /** @private */
  onAllowSharedDialogClose_() {
    this.$.allowShared.focus();
  },
});
